# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# Contributor(s):
#   Chris Jones <jones.chris.g@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import os, sys
from ply import lex, yacc

from ipdl.ast import *

def _getcallerpath():
    '''Return the absolute path of the file containing the code that
**CALLED** this function.'''
    return os.path.abspath(sys._getframe(1).f_code.co_filename)

# we want PLY to generate its output in the module directory, not wherever
# the user chooses to run ipdlc from
_thisdir, _ = os.path.split(_getcallerpath())

##-----------------------------------------------------------------------------

class Parser:
    # when we reach an |include protocol "foo.ipdl";| statement, we need to
    # save the current parser state and create a new one.  this "stack" is
    # where that state is saved
    #
    # there is one Parser per file
    current = None
    parseStack = [ ]
    parsed = { }

    def __init__(self, debug=0):
        self.debug = debug
        self.filename = None
        self.loc = None         # not always up to date
        self.lexer = None
        self.parser = None
        self.tu = TranslationUnit()

    def parse(self, input, filename):
        realpath = os.path.abspath(filename)
        if realpath in Parser.parsed:
            return Parser.parsed[realpath].tu

        self.lexer = lex.lex(debug=self.debug,
                             optimize=not self.debug,
                             lextab="ipdl_lextab",
                             outputdir=_thisdir)
        self.parser = yacc.yacc(debug=self.debug,
                                optimize=not self.debug,
                                tabmodule="ipdl_yacctab",
                                outputdir=_thisdir)
        self.filename = filename
        self.tu.filename = realpath

        Parser.parsed[realpath] = self
        Parser.parseStack.append(Parser.current)
        Parser.current = self

        ast = self.parser.parse(input=input, lexer=self.lexer,
                                 debug=self.debug)

        Parser.current = Parser.parseStack.pop()
        return ast

    # returns a GCC-style string representation of the include stack.
    # e.g.,
    #   in file included from 'foo.ipdl', line 120:
    #   in file included from 'bar.ipd', line 12:
    # which can be printed above a proper error message or warning
    @staticmethod
    def includeStackString():
        s = ''
        for parse in Parser.parseStack[1:]:
            s += "  in file included from `%s', line %d:\n"% (
                parse.loc.filename, parse.loc.lineno)
        return s

def locFromTok(p, num):
    return Loc(Parser.current.filename, p.lineno(num))


##-----------------------------------------------------------------------------

reserved = set((
        'answer',
        'async',
        'call',
        'goto',
        'in',
        'include',
        'inout',
        'manager',
        'manages',
        'namespace',
        'out',
        'protocol',
        'recv',
        'returns',
        'rpc',
        'send',
        'share',
        'sync',
        'transfer',
        'using'))
tokens = [
    'COLONCOLON', 'ID', 'STRING'
] + [ r.upper() for r in reserved ]

t_COLONCOLON = '::'

literals = '(){}[];,~'
t_ignore = ' \f\t\v'

def t_linecomment(t):
    r'//[^\n]*'

def t_multilinecomment(t):
    r'/\*(\n|.)*?\*/'
    t.lexer.lineno += t.value.count('\n')

def t_NL(t):
    r'\n+'
    t.lexer.lineno += len(t.value)

def t_ID(t):
    r'[a-zA-Z_][a-zA-Z0-9_]*'
    if t.value in reserved:
        t.type = t.value.upper()
    return t

def t_STRING(t):
    r'"[^"\n]*"'
    t.value = t.value[1:-1]
    return t

def t_error(t):
    includeStackStr = Parser.includeStackString()
    raise Exception, '%s%s: lexically invalid characters %s'% (
        includeStackStr, Loc(Parser.current.filename, t.lineno), str(t))

##-----------------------------------------------------------------------------

def p_TranslationUnit(p):
    """TranslationUnit : Preamble NamespacedProtocolDefn"""
    tu = Parser.current.tu
    for stmt in p[1]:
        if isinstance(stmt, CxxInclude):  tu.addCxxInclude(stmt)
        elif isinstance(stmt, ProtocolInclude): tu.addProtocolInclude(stmt)
        elif isinstance(stmt, UsingStmt): tu.addUsingStmt(stmt)
        else:
            assert 0
    tu.protocol = p[2]
    p[0] = tu

##--------------------
## Preamble
def p_Preamble(p):
    """Preamble : Preamble PreambleStmt ';'
                |"""
    if 1 == len(p):
        p[0] = [ ]
    else:
        p[1].append(p[2])
        p[0] = p[1]

def p_PreambleStmt(p):
    """PreambleStmt : CxxIncludeStmt
                    | ProtocolIncludeStmt
                    | UsingStmt"""
    p[0] = p[1]

def p_CxxIncludeStmt(p):
    """CxxIncludeStmt : INCLUDE STRING"""
    p[0] = CxxInclude(locFromTok(p, 1), p[2])

def p_ProtocolIncludeStmt(p):
    """ProtocolIncludeStmt : INCLUDE PROTOCOL STRING"""
    loc = locFromTok(p, 1)
    Parser.current.loc = loc

    inc = ProtocolInclude(loc, p[3])
    inc.tu = Parser().parse(open(inc.file).read(), inc.file)
    p[0] = inc

def p_UsingStmt(p):
    """UsingStmt : USING CxxType"""
    p[0] = UsingStmt(locFromTok(p, 1), p[2])

##--------------------
## Protocol definition
def p_NamespacedProtocolDefn(p):
    """NamespacedProtocolDefn : NAMESPACE ID '{' NamespacedProtocolDefn '}'
                              | ProtocolDefn"""
    if 2 == len(p):
        p[0] = p[1]
    else:
        protocol = p[4]
        protocol.addOuterNamespace(Namespace(locFromTok(p, 1), p[2]))
        p[0] = protocol

def p_ProtocolDefn(p):
    """ProtocolDefn : SendSemanticsQual PROTOCOL ID '{' ManagerStmtOpt ManagesStmts MessageDecls TransitionStmts '}' ';'"""
    protocol = Protocol(locFromTok(p, 2))
    protocol.name = p[3]
    protocol.sendSemantics = p[1]
    protocol.manager = p[5]
    protocol.addManagesStmts(p[6])
    protocol.addMessageDecls(p[7])
    protocol.addTransitionStmts(p[8])
    p[0] = protocol

def p_ManagesStmts(p):
    """ManagesStmts : ManagesStmts ManagesStmt
                    | """
    if 1 == len(p):
        p[0] = [ ]
    else:
        p[1].append(p[2])
        p[0] = p[1]

def p_ManagerStmtOpt(p):
    """ManagerStmtOpt : MANAGER ID ';' 
                      | """
    if 1 == len(p):
        p[0] = None
    else:
        p[0] = ManagerStmt(locFromTok(p, 1), p[2])

def p_ManagesStmt(p):
    """ManagesStmt : MANAGES ID ';'"""
    p[0] = ManagesStmt(locFromTok(p, 1), p[2])

def p_MessageDecls(p):
    """MessageDecls : MessageDecls MessageDecl ';'
                    | MessageDecl ';'"""
    if 3 == len(p):
        p[0] = [ p[1] ]
    else:
        p[1].append(p[2])
        p[0] = p[1]

def p_MessageDecl(p):
    """MessageDecl : SendSemanticsQual DirectionQual MessageBody"""
    msg = p[3]
    msg.sendSemantics = p[1]
    msg.direction = p[2]
    p[0] = msg

def p_MessageBody(p):
    """MessageBody : MessageId MessageInParams MessageOutParams"""
    # FIXME/cjones: need better loc info: use one of the quals
    msg = MessageDecl(locFromTok(p, 1))
    msg.name = p[1]
    msg.addInParams(p[2])
    msg.addOutParams(p[3])
    p[0] = msg

def p_MessageId(p):
    """MessageId : ID
                 | '~' ID"""
    if 3 == len(p):
        p[1] += p[2] # munge dtor name to "~Foo". handled later
    p[0] = p[1]

def p_MessageInParams(p):
    """MessageInParams : '(' ParamList ')'"""
    p[0] = p[2]

def p_MessageOutParams(p):
    """MessageOutParams : RETURNS '(' ParamList ')'
                        | """
    if 1 == len(p):
        p[0] = [ ]
    else:
        p[0] = p[3]

def p_TransitionStmts(p):
    """TransitionStmts : """
    # FIXME/cjones: impl
    p[0] = [ ]

##--------------------
## Minor stuff
def p_SendSemanticsQual(p):
    """SendSemanticsQual : ASYNC
                         | RPC
                         | SYNC
                         | """
    if 1 == len(p):
        p[0] = ASYNC
        return

    s = p[1]
    if 'async' == s: p[0] = ASYNC
    elif 'rpc' == s: p[0] = RPC
    elif 'sync'== s: p[0] = SYNC
    else:
        assert 0

def p_DirectionQual(p):
    """DirectionQual : IN
                     | INOUT
                     | OUT"""
    s = p[1]
    if 'in' == s:  p[0] = IN
    elif 'inout' == s:  p[0] = INOUT
    elif 'out' == s:  p[0] = OUT
    else:
        assert 0

def p_ParamList(p):
    """ParamList : ParamList ',' Param
                 | Param
                 | """
    if 1 == len(p):
        p[0] = [ ]
    elif 2 == len(p):
        p[0] = [ p[1] ]
    else:
        p[1].append(p[3])
        p[0] = p[1]

def p_Param(p):
    """Param : ID ID"""
    loc = locFromTok(p, 1)
    p[0] = Param(loc,
                 TypeSpec(loc, QualifiedId(loc, p[1])),
                 p[2])

##--------------------
## C++ stuff
def p_CxxType(p):
    """CxxType : QualifiedID
               | ID"""
    if isinstance(p[0], QualifiedId):
        p[0] = TypeSpec(p[1].loc, p[1])
    else:
        loc = locFromTok(p, 1)
        p[0] = TypeSpec(loc, QualifiedId(loc, p[1]))

def p_QualifiedID(p):
    """QualifiedID : QualifiedID COLONCOLON ID
                   | ID COLONCOLON ID"""
    if isinstance(p[1], QualifiedId):
        p[1].qualify(p[3])
        p[0] = p[1]
    else:
        p[0] = QualifiedId(locFromTok(p, 1), p[3])

def p_error(t):
    includeStackStr = Parser.includeStackString()
    raise Exception, '%s%s: syntax error near "%s"'% (
        includeStackStr, Loc(Parser.current.filename, t.lineno), t.value)
