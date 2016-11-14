# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

NOT_NESTED = 1
INSIDE_SYNC_NESTED = 2
INSIDE_CPOW_NESTED = 3

NORMAL_PRIORITY = 1
HIGH_PRIORITY = 2

class Visitor:
    def defaultVisit(self, node):
        raise Exception, "INTERNAL ERROR: no visitor for node type `%s'"% (
            node.__class__.__name__)

    def visitTranslationUnit(self, tu):
        for cxxInc in tu.cxxIncludes:
            cxxInc.accept(self)
        for inc in tu.includes:
            inc.accept(self)
        for su in tu.structsAndUnions:
            su.accept(self)
        for using in tu.builtinUsing:
            using.accept(self)
        for using in tu.using:
            using.accept(self)
        if tu.protocol:
            tu.protocol.accept(self)


    def visitCxxInclude(self, inc):
        pass

    def visitInclude(self, inc):
        # Note: we don't visit the child AST here, because that needs delicate
        # and pass-specific handling
        pass

    def visitStructDecl(self, struct):
        for f in struct.fields:
            f.accept(self)

    def visitStructField(self, field):
        field.typespec.accept(self)

    def visitUnionDecl(self, union):
        for t in union.components:
            t.accept(self)

    def visitUsingStmt(self, using):
        pass

    def visitProtocol(self, p):
        for namespace in p.namespaces:
            namespace.accept(self)
        for spawns in p.spawnsStmts:
            spawns.accept(self)
        for bridges in p.bridgesStmts:
            bridges.accept(self)
        for opens in p.opensStmts:
            opens.accept(self)
        for mgr in p.managers:
            mgr.accept(self)
        for managed in p.managesStmts:
            managed.accept(self)
        for msgDecl in p.messageDecls:
            msgDecl.accept(self)
        for transitionStmt in p.transitionStmts:
            transitionStmt.accept(self)

    def visitNamespace(self, ns):
        pass

    def visitSpawnsStmt(self, spawns):
        pass

    def visitBridgesStmt(self, bridges):
        pass

    def visitOpensStmt(self, opens):
        pass

    def visitManager(self, mgr):
        pass

    def visitManagesStmt(self, mgs):
        pass

    def visitMessageDecl(self, md):
        for inParam in md.inParams:
            inParam.accept(self)
        for outParam in md.outParams:
            outParam.accept(self)

    def visitTransitionStmt(self, ts):
        ts.state.accept(self)
        for trans in ts.transitions:
            trans.accept(self)

    def visitTransition(self, t):
        for toState in t.toStates:
            toState.accept(self)

    def visitState(self, s):
        pass

    def visitParam(self, decl):
        pass

    def visitTypeSpec(self, ts):
        pass

    def visitDecl(self, d):
        pass

class Loc:
    def __init__(self, filename='<??>', lineno=0):
        assert filename
        self.filename = filename
        self.lineno = lineno
    def __repr__(self):
        return '%r:%r'% (self.filename, self.lineno)
    def __str__(self):
        return '%s:%s'% (self.filename, self.lineno)

Loc.NONE = Loc(filename='<??>', lineno=0)

class _struct:
    pass

class Node:
    def __init__(self, loc=Loc.NONE):
        self.loc = loc

    def accept(self, visitor):
        visit = getattr(visitor, 'visit'+ self.__class__.__name__, None)
        if visit is None:
            return getattr(visitor, 'defaultVisit')(self)
        return visit(self)

    def addAttrs(self, attrsName):
        if not hasattr(self, attrsName):
            setattr(self, attrsName, _struct())


class NamespacedNode(Node):
    def __init__(self, loc=Loc.NONE, name=None):
        Node.__init__(self, loc)
        self.name = name
        self.namespaces = [ ]

    def addOuterNamespace(self, namespace):
        self.namespaces.insert(0, namespace)

    def qname(self):
        return QualifiedId(self.loc, self.name,
                           [ ns.name for ns in self.namespaces ])

class TranslationUnit(NamespacedNode):
    def __init__(self, type, name):
        NamespacedNode.__init__(self, name=name)
        self.filetype = type
        self.filename = None
        self.cxxIncludes = [ ]
        self.includes = [ ]
        self.builtinUsing = [ ]
        self.using = [ ]
        self.structsAndUnions = [ ]
        self.protocol = None

    def addCxxInclude(self, cxxInclude): self.cxxIncludes.append(cxxInclude)
    def addInclude(self, inc): self.includes.append(inc)
    def addStructDecl(self, struct): self.structsAndUnions.append(struct)
    def addUnionDecl(self, union): self.structsAndUnions.append(union)
    def addUsingStmt(self, using): self.using.append(using)

    def setProtocol(self, protocol): self.protocol = protocol

class CxxInclude(Node):
    def __init__(self, loc, cxxFile):
        Node.__init__(self, loc)
        self.file = cxxFile

class Include(Node):
    def __init__(self, loc, type, name):
        Node.__init__(self, loc)
        suffix = 'ipdl'
        if type == 'header':
            suffix += 'h'
        self.file = "%s.%s" % (name, suffix)

class UsingStmt(Node):
    def __init__(self, loc, cxxTypeSpec, cxxHeader=None, kind=None):
        Node.__init__(self, loc)
        assert not isinstance(cxxTypeSpec, str)
        assert cxxHeader is None or isinstance(cxxHeader, str);
        assert kind is None or kind == 'class' or kind == 'struct'
        self.type = cxxTypeSpec
        self.header = cxxHeader
        self.kind = kind
    def canBeForwardDeclared(self):
        return self.isClass() or self.isStruct()
    def isClass(self):
        return self.kind == 'class'
    def isStruct(self):
        return self.kind == 'struct'

# "singletons"
class PrettyPrinted:
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def __str__(cls):  return cls.pretty

class ASYNC(PrettyPrinted):
    pretty = 'async'
class INTR(PrettyPrinted):
    pretty = 'intr'
class SYNC(PrettyPrinted):
    pretty = 'sync'

class INOUT(PrettyPrinted):
    pretty = 'inout'
class IN(PrettyPrinted):
    pretty = 'in'
class OUT(PrettyPrinted):
    pretty = 'out'


class Namespace(Node):
    def __init__(self, loc, namespace):
        Node.__init__(self, loc)
        self.name = namespace

class Protocol(NamespacedNode):
    def __init__(self, loc):
        NamespacedNode.__init__(self, loc)
        self.sendSemantics = ASYNC
        self.nested = NOT_NESTED
        self.spawnsStmts = [ ]
        self.bridgesStmts = [ ]
        self.opensStmts = [ ]
        self.managers = [ ]
        self.managesStmts = [ ]
        self.messageDecls = [ ]
        self.transitionStmts = [ ]
        self.startStates = [ ]

class StructField(Node):
    def __init__(self, loc, type, name):
        Node.__init__(self, loc)
        self.typespec = type
        self.name = name

class StructDecl(NamespacedNode):
    def __init__(self, loc, name, fields):
        NamespacedNode.__init__(self, loc, name)
        self.fields = fields

class UnionDecl(NamespacedNode):
    def __init__(self, loc, name, components):
        NamespacedNode.__init__(self, loc, name)
        self.components = components

class SpawnsStmt(Node):
    def __init__(self, loc, side, proto, spawnedAs):
        Node.__init__(self, loc)
        self.side = side
        self.proto = proto
        self.spawnedAs = spawnedAs

class BridgesStmt(Node):
    def __init__(self, loc, parentSide, childSide):
        Node.__init__(self, loc)
        self.parentSide = parentSide
        self.childSide = childSide

class OpensStmt(Node):
    def __init__(self, loc, side, proto):
        Node.__init__(self, loc)
        self.side = side
        self.proto = proto

class Manager(Node):
    def __init__(self, loc, managerName):
        Node.__init__(self, loc)
        self.name = managerName

class ManagesStmt(Node):
    def __init__(self, loc, managedName):
        Node.__init__(self, loc)
        self.name = managedName

class MessageDecl(Node):
    def __init__(self, loc):
        Node.__init__(self, loc)
        self.name = None
        self.sendSemantics = ASYNC
        self.nested = NOT_NESTED
        self.prio = NORMAL_PRIORITY
        self.direction = None
        self.inParams = [ ]
        self.outParams = [ ]
        self.compress = ''
        self.verify = ''

    def addInParams(self, inParamsList):
        self.inParams += inParamsList

    def addOutParams(self, outParamsList):
        self.outParams += outParamsList

    def addModifiers(self, modifiers):
        for modifier in modifiers:
            if modifier.startswith('compress'):
                self.compress = modifier
            elif modifier == 'verify':
                self.verify = modifier
            elif modifier != '':
                raise Exception, "Unexpected message modifier `%s'"% modifier

class Transition(Node):
    def __init__(self, loc, trigger, msg, toStates):
        Node.__init__(self, loc)
        self.trigger = trigger
        self.msg = msg
        self.toStates = toStates

    def __cmp__(self, o):
        c = cmp(self.msg, o.msg)
        if c: return c
        c = cmp(self.trigger, o.trigger)
        if c: return c

    def __hash__(self): return hash(str(self))
    def __str__(self): return '%s %s'% (self.trigger, self.msg)

    @staticmethod
    def nameToTrigger(name):
        return { 'send': SEND, 'recv': RECV, 'call': CALL, 'answer': ANSWER }[name]

Transition.NULL = Transition(Loc.NONE, None, None, [ ])

class TransitionStmt(Node):
    def __init__(self, loc, state, transitions):
        Node.__init__(self, loc)
        self.state = state
        self.transitions = transitions

    @staticmethod
    def makeNullStmt(state):
        return TransitionStmt(Loc.NONE, state, [ Transition.NULL ])

class SEND:
    pretty = 'send'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return OUT
class RECV:
    pretty = 'recv'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return IN
class CALL:
    pretty = 'call'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return OUT
class ANSWER:
    pretty = 'answer'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return IN

class State(Node):
    def __init__(self, loc, name, start=False):
        Node.__init__(self, loc)
        self.name = name
        self.start = start
    def __eq__(self, o):
         return (isinstance(o, State)
                 and o.name == self.name
                 and o.start == self.start)
    def __hash__(self):
        return hash(repr(self))
    def __ne__(self, o):
        return not (self == o)
    def __repr__(self): return '<State %r start=%r>'% (self.name, self.start)
    def __str__(self): return '<State %s start=%s>'% (self.name, self.start)

State.ANY = State(Loc.NONE, '[any]', start=True)
State.DEAD = State(Loc.NONE, '[dead]', start=False)
State.DYING = State(Loc.NONE, '[dying]', start=False)

class Param(Node):
    def __init__(self, loc, typespec, name):
        Node.__init__(self, loc)
        self.name = name
        self.typespec = typespec

class TypeSpec(Node):
    def __init__(self, loc, spec):
        Node.__init__(self, loc)
        self.spec = spec                # QualifiedId
        self.array = 0                  # bool
        self.nullable = 0               # bool
        self.myChmod = None             # None or string
        self.otherChmod = None          # None or string

    def basename(self):
        return self.spec.baseid

    def __str__(self):  return str(self.spec)

class QualifiedId:              # FIXME inherit from node?
    def __init__(self, loc, baseid, quals=[ ]):
        assert isinstance(baseid, str)
        for qual in quals: assert isinstance(qual, str)

        self.loc = loc
        self.baseid = baseid
        self.quals = quals

    def qualify(self, id):
        self.quals.append(self.baseid)
        self.baseid = id

    def __str__(self):
        if 0 == len(self.quals):
            return self.baseid
        return '::'.join(self.quals) +'::'+ self.baseid

# added by type checking passes
class Decl(Node):
    def __init__(self, loc):
        Node.__init__(self, loc)
        self.progname = None    # what the programmer typed, if relevant
        self.shortname = None   # shortest way to refer to this decl
        self.fullname = None    # full way to refer to this decl
        self.loc = loc
        self.type = None
        self.scope = None
