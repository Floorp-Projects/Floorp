# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from ply import lex, yacc

from ipdl.ast import *

# -----------------------------------------------------------------------------


class ParseError(Exception):
    def __init__(self, loc, fmt, *args):
        self.loc = loc
        self.error = (
            "%s%s: error: %s" % (Parser.includeStackString(), loc, fmt)
        ) % args

    def __str__(self):
        return self.error


def _safeLinenoValue(t):
    lineno, value = 0, "???"
    if hasattr(t, "lineno"):
        lineno = t.lineno
    if hasattr(t, "value"):
        value = t.value
    return lineno, value


def _error(loc, fmt, *args):
    raise ParseError(loc, fmt, *args)


class Parser:
    # when we reach an |include [protocol] foo;| statement, we need to
    # save the current parser state and create a new one.  this "stack" is
    # where that state is saved
    #
    # there is one Parser per file
    current = None
    parseStack = []
    parsed = {}

    def __init__(self, type, name, debug=False):
        assert type and name
        self.type = type
        self.debug = debug
        self.filename = None
        self.includedirs = None
        self.loc = None  # not always up to date
        self.lexer = None
        self.parser = None
        self.tu = TranslationUnit(type, name)
        self.direction = None

    def parse(self, input, filename, includedirs):
        assert os.path.isabs(filename)

        if self.tu.name in Parser.parsed:
            priorTU = Parser.parsed[self.tu.name].tu
            if os.path.normcase(priorTU.filename) != os.path.normcase(filename):
                _error(
                    Loc(filename),
                    "Trying to load `%s' from a file when we'd already seen it in file `%s'"
                    % (self.tu.name, priorTU.filename),
                )

            return priorTU

        self.lexer = lex.lex(debug=self.debug)
        self.parser = yacc.yacc(debug=self.debug, write_tables=False)
        self.filename = filename
        self.includedirs = includedirs
        self.tu.filename = filename

        Parser.parsed[self.tu.name] = self
        Parser.parseStack.append(Parser.current)
        Parser.current = self

        try:
            ast = self.parser.parse(input=input, lexer=self.lexer, debug=self.debug)
        finally:
            Parser.current = Parser.parseStack.pop()

        return ast

    def resolveIncludePath(self, filepath):
        """Return the absolute path from which the possibly partial
        |filepath| should be read, or |None| if |filepath| cannot be located."""
        for incdir in self.includedirs + [""]:
            realpath = os.path.join(incdir, filepath)
            if os.path.isfile(realpath):
                return os.path.abspath(realpath)
        return None

    # returns a GCC-style string representation of the include stack.
    # e.g.,
    #   in file included from 'foo.ipdl', line 120:
    #   in file included from 'bar.ipd', line 12:
    # which can be printed above a proper error message or warning
    @staticmethod
    def includeStackString():
        s = ""
        for parse in Parser.parseStack[1:]:
            s += "  in file included from `%s', line %d:\n" % (
                parse.loc.filename,
                parse.loc.lineno,
            )
        return s


def locFromTok(p, num):
    return Loc(Parser.current.filename, p.lineno(num))


# -----------------------------------------------------------------------------

reserved = set(
    (
        "async",
        "both",
        "child",
        "class",
        "from",
        "include",
        "manager",
        "manages",
        "namespace",
        "nullable",
        "or",
        "parent",
        "protocol",
        "returns",
        "struct",
        "sync",
        "union",
        "UniquePtr",
        "using",
    )
)
tokens = [
    "COLONCOLON",
    "ID",
    "STRING",
] + [r.upper() for r in reserved]

t_COLONCOLON = "::"

literals = "(){}[]<>;:,?="
t_ignore = " \f\t\v"


def t_linecomment(t):
    r"//[^\n]*"


def t_multilinecomment(t):
    r"/\*(\n|.)*?\*/"
    t.lexer.lineno += t.value.count("\n")


def t_NL(t):
    r"(?:\r\n|\n|\n)+"
    t.lexer.lineno += len(t.value)


def t_ID(t):
    r"[a-zA-Z_][a-zA-Z0-9_]*"
    if t.value in reserved:
        t.type = t.value.upper()
    return t


def t_STRING(t):
    r'"[^"\n]*"'
    t.value = StringLiteral(Loc(Parser.current.filename, t.lineno), t.value[1:-1])
    return t


def t_error(t):
    _error(
        Loc(Parser.current.filename, t.lineno),
        "lexically invalid characters `%s",
        t.value,
    )


# -----------------------------------------------------------------------------


def p_TranslationUnit(p):
    """TranslationUnit : Preamble NamespacedStuff"""
    tu = Parser.current.tu
    tu.loc = Loc(tu.filename)
    for stmt in p[1]:
        if isinstance(stmt, CxxInclude):
            tu.addCxxInclude(stmt)
        elif isinstance(stmt, Include):
            tu.addInclude(stmt)
        elif isinstance(stmt, UsingStmt):
            tu.addUsingStmt(stmt)
        else:
            assert 0

    for thing in p[2]:
        if isinstance(thing, StructDecl):
            tu.addStructDecl(thing)
        elif isinstance(thing, UnionDecl):
            tu.addUnionDecl(thing)
        elif isinstance(thing, Protocol):
            if tu.protocol is not None:
                _error(thing.loc, "only one protocol definition per file")
            tu.protocol = thing
        else:
            assert 0

    # The "canonical" namespace of the tu, what it's considered to be
    # in for the purposes of C++: |#include "foo/bar/TU.h"|
    if tu.protocol:
        assert tu.filetype == "protocol"
        tu.namespaces = tu.protocol.namespaces
        tu.name = tu.protocol.name
    else:
        assert tu.filetype == "header"
        # There's not really a canonical "thing" in headers.  So
        # somewhat arbitrarily use the namespace of the last
        # interesting thing that was declared.
        for thing in reversed(tu.structsAndUnions):
            tu.namespaces = thing.namespaces
            break

    p[0] = tu


# --------------------
# Preamble


def p_Preamble(p):
    """Preamble : Preamble PreambleStmt ';'
    |"""
    if 1 == len(p):
        p[0] = []
    else:
        p[1].append(p[2])
        p[0] = p[1]


def p_PreambleStmt(p):
    """PreambleStmt : CxxIncludeStmt
    | IncludeStmt
    | UsingStmt"""
    p[0] = p[1]


def p_CxxIncludeStmt(p):
    """CxxIncludeStmt : INCLUDE STRING"""
    p[0] = CxxInclude(locFromTok(p, 1), p[2].value)


def p_IncludeStmt(p):
    """IncludeStmt : INCLUDE PROTOCOL ID
    | INCLUDE ID"""
    loc = locFromTok(p, 1)

    Parser.current.loc = loc
    if 4 == len(p):
        id = p[3]
        type = "protocol"
    else:
        id = p[2]
        type = "header"
    inc = Include(loc, type, id)

    path = Parser.current.resolveIncludePath(inc.file)
    if path is None:
        raise ParseError(loc, "can't locate include file `%s'" % (inc.file))

    inc.tu = Parser(type, id).parse(open(path).read(), path, Parser.current.includedirs)
    p[0] = inc


def p_UsingKind(p):
    """UsingKind : CLASS
    | STRUCT
    |"""
    p[0] = p[1] if 2 == len(p) else None


def p_UsingStmt(p):
    """UsingStmt : Attributes USING UsingKind CxxType FROM STRING"""
    p[0] = UsingStmt(
        locFromTok(p, 2),
        attributes=p[1],
        kind=p[3],
        cxxTypeSpec=p[4],
        cxxHeader=p[6].value,
    )


# --------------------
# Namespaced stuff


def p_NamespacedStuff(p):
    """NamespacedStuff : NamespacedStuff NamespaceThing
    | NamespaceThing"""
    if 2 == len(p):
        p[0] = p[1]
    else:
        p[1].extend(p[2])
        p[0] = p[1]


def p_NamespaceThing(p):
    """NamespaceThing : NAMESPACE ID '{' NamespacedStuff '}'
    | StructDecl
    | UnionDecl
    | ProtocolDefn"""
    if 2 == len(p):
        p[0] = [p[1]]
    else:
        for thing in p[4]:
            thing.addOuterNamespace(Namespace(locFromTok(p, 1), p[2]))
        p[0] = p[4]


def p_StructDecl(p):
    """StructDecl : Attributes STRUCT ID '{' StructFields '}' ';'
    | Attributes STRUCT ID '{' '}' ';'"""
    if 8 == len(p):
        p[0] = StructDecl(locFromTok(p, 2), p[3], p[5], p[1])
    else:
        p[0] = StructDecl(locFromTok(p, 2), p[3], [], p[1])


def p_StructFields(p):
    """StructFields : StructFields StructField ';'
    | StructField ';'"""
    if 3 == len(p):
        p[0] = [p[1]]
    else:
        p[1].append(p[2])
        p[0] = p[1]


def p_StructField(p):
    """StructField : Type ID"""
    p[0] = StructField(locFromTok(p, 1), p[1], p[2])


def p_UnionDecl(p):
    """UnionDecl : Attributes UNION ID '{' ComponentTypes  '}' ';'"""
    p[0] = UnionDecl(locFromTok(p, 2), p[3], p[5], p[1])


def p_ComponentTypes(p):
    """ComponentTypes : ComponentTypes Type ';'
    | Type ';'"""
    if 3 == len(p):
        p[0] = [p[1]]
    else:
        p[1].append(p[2])
        p[0] = p[1]


def p_ProtocolDefn(p):
    """ProtocolDefn : Attributes OptionalSendSemantics \
                      PROTOCOL ID '{' ProtocolBody '}' ';'"""
    protocol = p[6]
    protocol.loc = locFromTok(p, 3)
    protocol.name = p[4]
    protocol.attributes = p[1]
    protocol.sendSemantics = p[2]
    p[0] = protocol

    if Parser.current.type == "header":
        _error(
            protocol.loc,
            "can't define a protocol in a header.  Do it in a protocol spec instead.",
        )


def p_ProtocolBody(p):
    """ProtocolBody : ManagersStmtOpt"""
    p[0] = p[1]


# --------------------
# manager/manages stmts


def p_ManagersStmtOpt(p):
    """ManagersStmtOpt : ManagersStmt ManagesStmtsOpt
    | ManagesStmtsOpt"""
    if 2 == len(p):
        p[0] = p[1]
    else:
        p[2].managers = p[1]
        p[0] = p[2]


def p_ManagersStmt(p):
    """ManagersStmt : MANAGER ManagerList ';'"""
    if 1 == len(p):
        p[0] = []
    else:
        p[0] = p[2]


def p_ManagerList(p):
    """ManagerList : ID
    | ManagerList OR ID"""
    if 2 == len(p):
        p[0] = [Manager(locFromTok(p, 1), p[1])]
    else:
        p[1].append(Manager(locFromTok(p, 3), p[3]))
        p[0] = p[1]


def p_ManagesStmtsOpt(p):
    """ManagesStmtsOpt : ManagesStmt ManagesStmtsOpt
    | MessageDeclsOpt"""
    if 2 == len(p):
        p[0] = p[1]
    else:
        p[2].managesStmts.insert(0, p[1])
        p[0] = p[2]


def p_ManagesStmt(p):
    """ManagesStmt : MANAGES ID ';'"""
    p[0] = ManagesStmt(locFromTok(p, 1), p[2])


# --------------------
# Message decls


def p_MessageDeclsOpt(p):
    """MessageDeclsOpt : MessageDeclThing MessageDeclsOpt
    |"""
    if 1 == len(p):
        # we fill in |loc| in the Protocol rule
        p[0] = Protocol(None)
    else:
        p[2].messageDecls.insert(0, p[1])
        p[0] = p[2]


def p_MessageDeclThing(p):
    """MessageDeclThing : MessageDirectionLabel ':' MessageDecl ';'
    | MessageDecl ';'"""
    if 3 == len(p):
        p[0] = p[1]
    else:
        p[0] = p[3]


def p_MessageDirectionLabel(p):
    """MessageDirectionLabel : PARENT
    | CHILD
    | BOTH"""
    if p[1] == "parent":
        Parser.current.direction = IN
    elif p[1] == "child":
        Parser.current.direction = OUT
    elif p[1] == "both":
        Parser.current.direction = INOUT
    else:
        assert 0


def p_MessageDecl(p):
    """MessageDecl : Attributes SendSemantics MessageBody"""
    msg = p[3]
    msg.attributes = p[1]
    msg.sendSemantics = p[2]

    if Parser.current.direction is None:
        _error(msg.loc, "missing message direction")
    msg.direction = Parser.current.direction

    p[0] = msg


def p_MessageBody(p):
    """MessageBody : ID MessageInParams MessageOutParams"""
    # FIXME/cjones: need better loc info: use one of the quals
    name = p[1]
    msg = MessageDecl(locFromTok(p, 1))
    msg.name = name
    msg.addInParams(p[2])
    msg.addOutParams(p[3])

    p[0] = msg


def p_MessageInParams(p):
    """MessageInParams : '(' ParamList ')'"""
    p[0] = p[2]


def p_MessageOutParams(p):
    """MessageOutParams : RETURNS '(' ParamList ')'
    |"""
    if 1 == len(p):
        p[0] = []
    else:
        p[0] = p[3]


# --------------------
# Attributes
def p_Attributes(p):
    """Attributes : '[' AttributeList ']'
    |"""
    p[0] = {}
    if 4 == len(p):
        for attr in p[2]:
            if attr.name in p[0]:
                _error(attr.loc, "Repeated extended attribute `%s'", attr.name)
            p[0][attr.name] = attr


def p_AttributeList(p):
    """AttributeList : Attribute ',' AttributeList
    | Attribute"""
    p[0] = [p[1]]
    if 4 == len(p):
        p[0] += p[3]


def p_Attribute(p):
    """Attribute : ID AttributeValue"""
    p[0] = Attribute(locFromTok(p, 1), p[1], p[2])


def p_AttributeValue(p):
    """AttributeValue : '=' ID
    | '=' STRING
    |"""
    if 1 == len(p):
        p[0] = None
    else:
        p[0] = p[2]


def p_SendSemantics(p):
    """SendSemantics : ASYNC
    | SYNC"""
    if p[1] == "async":
        p[0] = ASYNC
    else:
        assert p[1] == "sync"
        p[0] = SYNC


def p_OptionalSendSemantics(p):
    """OptionalSendSemantics : SendSemantics
    |"""
    if 2 == len(p):
        p[0] = p[1]
    else:
        p[0] = ASYNC


# --------------------
# Minor stuff


def p_ParamList(p):
    """ParamList : ParamList ',' Param
    | Param
    |"""
    if 1 == len(p):
        p[0] = []
    elif 2 == len(p):
        p[0] = [p[1]]
    else:
        p[1].append(p[3])
        p[0] = p[1]


def p_Param(p):
    """Param : Attributes Type ID"""
    p[0] = Param(locFromTok(p, 2), p[2], p[3], p[1])


def p_Type(p):
    """Type : MaybeNullable BasicType"""
    # only some types are nullable; we check this in the type checker
    p[2].nullable = p[1]
    p[0] = p[2]


def p_BasicType(p):
    """BasicType : CxxID
    | CxxID '[' ']'
    | CxxID '?'
    | CxxUniquePtrInst"""
    # ID == CxxType; we forbid qnames here,
    # in favor of the |using| declaration
    if not isinstance(p[1], TypeSpec):
        assert (len(p[1]) == 2) or (len(p[1]) == 3)
        if 2 == len(p[1]):
            # p[1] is CxxID. isunique = False
            p[1] = p[1] + (False,)
        loc, id, isunique = p[1]
        p[1] = TypeSpec(loc, id)
        p[1].uniqueptr = isunique
    if 4 == len(p):
        p[1].array = True
    if 3 == len(p):
        p[1].maybe = True
    p[0] = p[1]


def p_MaybeNullable(p):
    """MaybeNullable : NULLABLE
    |"""
    p[0] = 2 == len(p)


# --------------------
# C++ stuff


def p_CxxType(p):
    """CxxType : QualifiedID
    | CxxID"""
    if isinstance(p[1], QualifiedId):
        p[0] = p[1]
    else:
        loc, id = p[1]
        p[0] = QualifiedId(loc, id)


def p_QualifiedID(p):
    """QualifiedID : QualifiedID COLONCOLON CxxID
    | CxxID COLONCOLON CxxID"""
    if isinstance(p[1], QualifiedId):
        loc, id = p[3]
        p[1].qualify(id)
        p[0] = p[1]
    else:
        loc1, id1 = p[1]
        _, id2 = p[3]
        p[0] = QualifiedId(loc1, id2, [id1])


def p_CxxID(p):
    """CxxID : ID
    | CxxTemplateInst"""
    if isinstance(p[1], tuple):
        p[0] = p[1]
    else:
        p[0] = (locFromTok(p, 1), str(p[1]))


def p_CxxTemplateInst(p):
    """CxxTemplateInst : ID '<' ID '>'"""
    p[0] = (locFromTok(p, 1), str(p[1]) + "<" + str(p[3]) + ">")


def p_CxxUniquePtrInst(p):
    """CxxUniquePtrInst : UNIQUEPTR '<' ID '>'"""
    p[0] = (locFromTok(p, 1), str(p[3]), True)


def p_error(t):
    lineno, value = _safeLinenoValue(t)
    _error(Loc(Parser.current.filename, lineno), "bad syntax near `%s'", value)
