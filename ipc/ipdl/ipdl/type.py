# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, sys

from ipdl.ast import CxxInclude, Decl, Loc, QualifiedId, State, StructDecl, TransitionStmt, TypeSpec, UnionDecl, UsingStmt, Visitor, ASYNC, SYNC, RPC, IN, OUT, INOUT, ANSWER, CALL, RECV, SEND
import ipdl.builtin as builtin

_DELETE_MSG = '__delete__'


def _otherside(side):
    if side == 'parent':  return 'child'
    elif side == 'child': return 'parent'
    else:  assert 0 and 'unknown side "%s"'% (side)

def unique_pairs(s):
    n = len(s)
    for i, e1 in enumerate(s):
        for j in xrange(i+1, n):
            yield (e1, s[j])

def cartesian_product(s1, s2):
    for e1 in s1:
        for e2 in s2:
            yield (e1, e2)


class TypeVisitor:
    def __init__(self):
        self.visited = set()

    def defaultVisit(self, node, *args):
        raise Exception, "INTERNAL ERROR: no visitor for node type `%s'"% (
            node.__class__.__name__)

    def visitVoidType(self, v, *args):
        pass

    def visitBuiltinCxxType(self, t, *args):
        pass

    def visitImportedCxxType(self, t, *args):
        pass

    def visitStateType(self, s, *args):
        pass

    def visitMessageType(self, m, *args):
        for param in m.params:
            param.accept(self, *args)
        for ret in m.returns:
            ret.accept(self, *args)
        if m.cdtype is not None:
            m.cdtype.accept(self, *args)

    def visitProtocolType(self, p, *args):
        # NB: don't visit manager and manages. a naive default impl
        # could result in an infinite loop
        pass

    def visitActorType(self, a, *args):
        a.protocol.accept(self, *args)
        a.state.accept(self, *args)

    def visitStructType(self, s, *args):
        if s in self.visited:
            return

        self.visited.add(s)
        for field in s.fields:
            field.accept(self, *args)

    def visitUnionType(self, u, *args):
        if u in self.visited:
            return

        self.visited.add(u)
        for component in u.components:
            component.accept(self, *args)

    def visitArrayType(self, a, *args):
        a.basetype.accept(self, *args)

    def visitShmemType(self, s, *args):
        pass

    def visitShmemChmodType(self, c, *args):
        c.shmem.accept(self)


class Type:
    def __cmp__(self, o):
        return cmp(self.fullname(), o.fullname())
    def __eq__(self, o):
        return (self.__class__ == o.__class__
                and self.fullname() == o.fullname())
    def __hash__(self):
        return hash(self.fullname())

    # Is this a C++ type?
    def isCxx(self):
        return False
    # Is this an IPDL type?
    def isIPDL(self):
        return False
    # Is this type neither compound nor an array?
    def isAtom(self):
        return False
    # Can this type appear in IPDL programs?
    def isVisible(self):
        return False
    def isVoid(self):
        return False
    def typename(self):
        return self.__class__.__name__

    def name(self): raise Exception, 'NYI'
    def fullname(self): raise Exception, 'NYI'

    def accept(self, visitor, *args):
        visit = getattr(visitor, 'visit'+ self.__class__.__name__, None)
        if visit is None:
            return getattr(visitor, 'defaultVisit')(self, *args)
        return visit(self, *args)

class VoidType(Type):
    def isCxx(self):
        return True
    def isIPDL(self):
        return False
    def isAtom(self):
        return True
    def isVisible(self):
        return False
    def isVoid(self):
        return True

    def name(self): return 'void'
    def fullname(self): return 'void'

VOID = VoidType()

##--------------------
class CxxType(Type):
    def isCxx(self):
        return True
    def isAtom(self):
        return True
    def isBuiltin(self):
        return False
    def isImported(self):
        return False
    def isGenerated(self):
        return False
    def isVisible(self):
        return True

class BuiltinCxxType(CxxType):
    def __init__(self, qname):
        assert isinstance(qname, QualifiedId)
        self.loc = qname.loc
        self.qname = qname
    def isBuiltin(self):  return True

    def name(self):
        return self.qname.baseid
    def fullname(self):
        return str(self.qname)

class ImportedCxxType(CxxType):
    def __init__(self, qname):
        assert isinstance(qname, QualifiedId)
        self.loc = qname.loc
        self.qname = qname
    def isImported(self): return True

    def name(self):
        return self.qname.baseid
    def fullname(self):
        return str(self.qname)

##--------------------
class IPDLType(Type):
    def isIPDL(self):  return True
    def isVisible(self): return True
    def isState(self): return False
    def isMessage(self): return False
    def isProtocol(self): return False
    def isActor(self): return False
    def isStruct(self): return False
    def isUnion(self): return False
    def isArray(self): return False
    def isAtom(self):  return True
    def isCompound(self): return False
    def isShmem(self): return False
    def isChmod(self): return False

    def isAsync(self): return self.sendSemantics is ASYNC
    def isSync(self): return self.sendSemantics is SYNC
    def isRpc(self): return self.sendSemantics is RPC

    def talksAsync(self): return True
    def talksSync(self): return self.isSync() or self.isRpc()
    def talksRpc(self): return self.isRpc()

    def hasReply(self):  return self.isSync() or self.isRpc()

    def needsMoreJuiceThan(self, o):
        return (o.isAsync() and not self.isAsync()
                or o.isSync() and self.isRpc())

class StateType(IPDLType):
    def __init__(self, protocol, name, start=False):
        self.protocol = protocol
        self.name = name
        self.start = start
    def isState(self): return True
    def name(self):
        return self.name
    def fullname(self):
        return self.name()

class MessageType(IPDLType):
    def __init__(self, sendSemantics, direction,
                 ctor=False, dtor=False, cdtype=None):
        assert not (ctor and dtor)
        assert not (ctor or dtor) or type is not None

        self.sendSemantics = sendSemantics
        self.direction = direction
        self.params = [ ]
        self.returns = [ ]
        self.ctor = ctor
        self.dtor = dtor
        self.cdtype = cdtype
    def isMessage(self): return True

    def isCtor(self): return self.ctor
    def isDtor(self): return self.dtor
    def constructedType(self):  return self.cdtype

    def isIn(self): return self.direction is IN
    def isOut(self): return self.direction is OUT
    def isInout(self): return self.direction is INOUT

    def hasImplicitActorParam(self):
        return self.isCtor() or self.isDtor()

class Bridge:
    def __init__(self, parentPtype, childPtype):
        assert parentPtype.isToplevel() and childPtype.isToplevel()
        self.parent = parentPtype
        self.child = childPtype

    def __cmp__(self, o):
        return cmp(self.parent, o.parent) or cmp(self.child, o.child)
    def __eq__(self, o):
        return self.parent == o.parent and self.child == o.child
    def __hash__(self):
        return hash(self.parent) + hash(self.child)

class ProtocolType(IPDLType):
    def __init__(self, qname, sendSemantics, stateless=False):
        self.qname = qname
        self.sendSemantics = sendSemantics
        self.spawns = set()             # ProtocolType
        self.opens = set()              # ProtocolType
        self.managers = set()           # ProtocolType
        self.manages = [ ]
        self.stateless = stateless
        self.hasDelete = False
    def isProtocol(self): return True

    def name(self):
        return self.qname.baseid
    def fullname(self):
        return str(self.qname)

    def addManager(self, mgrtype):
        assert mgrtype.isIPDL() and mgrtype.isProtocol()
        self.managers.add(mgrtype)

    def addSpawn(self, ptype):
        assert self.isToplevel() and  ptype.isToplevel()
        self.spawns.add(ptype)

    def addOpen(self, ptype):
        assert self.isToplevel() and  ptype.isToplevel()
        self.opens.add(ptype)

    def managedBy(self, mgr):
        self.managers = mgr

    def toplevel(self):
        if self.isToplevel():
            return self
        for mgr in self.managers:
            if mgr is not self:
                return mgr.toplevel()

    def isManagerOf(self, pt):
        for managed in self.manages:
            if pt is managed:
                return True
        return False
    def isManagedBy(self, pt):
        return pt in self.managers

    def isManager(self):
        return len(self.manages) > 0
    def isManaged(self):
        return 0 < len(self.managers)
    def isToplevel(self):
        return not self.isManaged()

    def manager(self):
        assert 1 == len(self.managers)
        for mgr in self.managers: return mgr

class ActorType(IPDLType):
    def __init__(self, protocol, state=None, nullable=0):
        self.protocol = protocol
        self.state = state
        self.nullable = nullable
    def isActor(self): return True

    def name(self):
        return self.protocol.name()
    def fullname(self):
        return self.protocol.fullname()

class _CompoundType(IPDLType):
    def __init__(self):
        self.defined = False            # bool
        self.mutualRec = set()          # set(_CompoundType | ArrayType)
    def isAtom(self):
        return False
    def isCompound(self):
        return True
    def itercomponents(self):
        raise Exception('"pure virtual" method')

    def mutuallyRecursiveWith(self, t, exploring=None):
        '''|self| is mutually recursive with |t| iff |self| and |t|
are in a cycle in the type graph rooted at |self|.  This function
looks for such a cycle and returns True if found.'''
        if exploring is None:
            exploring = set()

        if t.isAtom():
            return False
        elif t is self or t in self.mutualRec:
            return True
        elif t.isArray():
            isrec = self.mutuallyRecursiveWith(t.basetype, exploring)
            if isrec:  self.mutualRec.add(t)
            return isrec
        elif t in exploring:
            return False

        exploring.add(t)
        for c in t.itercomponents():
            if self.mutuallyRecursiveWith(c, exploring):
                self.mutualRec.add(c)
                return True
        exploring.remove(t)

        return False

class StructType(_CompoundType):
    def __init__(self, qname, fields):
        _CompoundType.__init__(self)
        self.qname = qname
        self.fields = fields            # [ Type ]

    def isStruct(self):   return True
    def itercomponents(self):
        for f in self.fields:
            yield f
    
    def name(self): return self.qname.baseid
    def fullname(self): return str(self.qname)

class UnionType(_CompoundType):
    def __init__(self, qname, components):
        _CompoundType.__init__(self)
        self.qname = qname
        self.components = components    # [ Type ]

    def isUnion(self):    return True
    def itercomponents(self):
        for c in self.components:
            yield c

    def name(self): return self.qname.baseid
    def fullname(self): return str(self.qname)

class ArrayType(IPDLType):
    def __init__(self, basetype):
        self.basetype = basetype
    def isAtom(self):  return False
    def isArray(self): return True

    def name(self): return self.basetype.name() +'[]'
    def fullname(self): return self.basetype.fullname() +'[]'

class ShmemType(IPDLType):
    def __init__(self, qname):
        self.qname = qname
    def isShmem(self): return True

    def name(self):
        return self.qname.baseid
    def fullname(self):
        return str(self.qname)

def iteractortypes(t, visited=None):
    """Iterate over any actor(s) buried in |type|."""
    if visited is None:
        visited = set()

    # XXX |yield| semantics makes it hard to use TypeVisitor
    if not t.isIPDL():
        return
    elif t.isActor():
        yield t
    elif t.isArray():
        for actor in iteractortypes(t.basetype, visited):
            yield actor
    elif t.isCompound() and t not in visited:
        visited.add(t)
        for c in t.itercomponents():
            for actor in iteractortypes(c, visited):
                yield actor

def hasactor(type):
    """Return true iff |type| is an actor or has one buried within."""
    for _ in iteractortypes(type): return True
    return False

def hasshmem(type):
    """Return true iff |type| is shmem or has it buried within."""
    class found: pass
    class findShmem(TypeVisitor):
        def visitShmemType(self, s):  raise found()
    try:
        type.accept(findShmem())
    except found:
        return True
    return False

##--------------------
_builtinloc = Loc('<builtin>', 0)
def makeBuiltinUsing(tname):
    quals = tname.split('::')
    base = quals.pop()
    quals = quals[0:]
    return UsingStmt(_builtinloc,
                     TypeSpec(_builtinloc,
                              QualifiedId(_builtinloc, base, quals)))

builtinUsing = [ makeBuiltinUsing(t) for t in builtin.Types ]
builtinIncludes = [ CxxInclude(_builtinloc, f) for f in builtin.Includes ]

def errormsg(loc, fmt, *args):
    while not isinstance(loc, Loc):
        if loc is None:  loc = Loc.NONE
        else:            loc = loc.loc
    return '%s: error: %s'% (str(loc), fmt % args)

##--------------------
class SymbolTable:
    def __init__(self, errors):
        self.errors = errors
        self.scopes = [ { } ]   # stack({})
        self.globalScope = self.scopes[0]
        self.currentScope = self.globalScope
    
    def enterScope(self, node):
        assert (isinstance(self.scopes[0], dict)
                and self.globalScope is self.scopes[0])
        assert (isinstance(self.currentScope, dict))

        if not hasattr(node, 'symtab'):
            node.symtab = { }

        self.scopes.append(node.symtab)
        self.currentScope = self.scopes[-1]

    def exitScope(self, node):
        symtab = self.scopes.pop()
        assert self.currentScope is symtab

        self.currentScope = self.scopes[-1]

        assert (isinstance(self.scopes[0], dict)
                and self.globalScope is self.scopes[0])
        assert isinstance(self.currentScope, dict)

    def lookup(self, sym):
        # NB: since IPDL doesn't allow any aliased names of different types,
        # it doesn't matter in which order we walk the scope chain to resolve
        # |sym|
        for scope in self.scopes:
            decl = scope.get(sym, None)
            if decl is not None:  return decl
        return None

    def declare(self, decl):
        assert decl.progname or decl.shortname or decl.fullname
        assert decl.loc
        assert decl.type

        def tryadd(name):
            olddecl = self.lookup(name)
            if olddecl is not None:
                self.errors.append(errormsg(
                        decl.loc,
                        "redeclaration of symbol `%s', first declared at %s",
                        name, olddecl.loc))
                return
            self.currentScope[name] = decl
            decl.scope = self.currentScope

        if decl.progname:  tryadd(decl.progname)
        if decl.shortname: tryadd(decl.shortname)
        if decl.fullname:  tryadd(decl.fullname)


class TypeCheck:
    '''This pass sets the .type attribute of every AST node.  For some
nodes, the type is meaningless and it is set to "VOID."  This pass
also sets the .decl attribute of AST nodes for which that is relevant;
a decl says where, with what type, and under what name(s) a node was
declared.

With this information, it finally type checks the AST.'''

    def __init__(self):
        # NB: no IPDL compile will EVER print a warning.  A program has
        # one of two attributes: it is either well typed, or not well typed.
        self.errors = [ ]       # [ string ]

    def check(self, tu, errout=sys.stderr):
        def runpass(tcheckpass):
            tu.accept(tcheckpass)
            if len(self.errors):
                self.reportErrors(errout)
                return False
            return True

        tu.cxxIncludes = builtinIncludes + tu.cxxIncludes

        # tag each relevant node with "decl" information, giving type, name,
        # and location of declaration
        if not runpass(GatherDecls(builtinUsing, self.errors)):
            return False

        # now that the nodes have decls, type checking is much easier.
        if not runpass(CheckTypes(self.errors)):
            return False

        if not (runpass(BuildProcessGraph(self.errors))
                and runpass(CheckProcessGraph(self.errors))):
            return False

        if (tu.protocol
            and len(tu.protocol.startStates)
            and not runpass(CheckStateMachine(self.errors))):
            return False
        return True

    def reportErrors(self, errout):
        for error in self.errors:
            print >>errout, error


class TcheckVisitor(Visitor):
    def __init__(self, symtab, errors):
        self.symtab = symtab
        self.errors = errors

    def error(self, loc, fmt, *args):
        self.errors.append(errormsg(loc, fmt, *args))

    def declare(self, loc, type, shortname=None, fullname=None, progname=None):
        d = Decl(loc)
        d.type = type
        d.progname = progname
        d.shortname = shortname
        d.fullname = fullname
        self.symtab.declare(d)
        return d

class GatherDecls(TcheckVisitor):
    def __init__(self, builtinUsing, errors):
        # |self.symtab| is the symbol table for the translation unit
        # currently being visited
        TcheckVisitor.__init__(self, None, errors)
        self.builtinUsing = builtinUsing

    def visitTranslationUnit(self, tu):
        # all TranslationUnits declare symbols in global scope
        if hasattr(tu, 'symtab'):
            return
        tu.symtab = SymbolTable(self.errors)
        savedSymtab = self.symtab
        self.symtab = tu.symtab

        # pretend like the translation unit "using"-ed these for the
        # sake of type checking and C++ code generation
        tu.builtinUsing = self.builtinUsing

        # for everyone's sanity, enforce that the filename and tu name
        # match
        basefilename = os.path.basename(tu.filename)
        expectedfilename = '%s.ipdl'% (tu.name)
        if not tu.protocol:
            # header
            expectedfilename += 'h'
        if basefilename != expectedfilename:
            self.error(tu.loc,
                       "expected file for translation unit `%s' to be named `%s'; instead it's named `%s'",
                       tu.name, expectedfilename, basefilename)

        if tu.protocol:
            assert tu.name == tu.protocol.name

            p = tu.protocol

            # FIXME/cjones: it's a little weird and counterintuitive
            # to put both the namespace and non-namespaced name in the
            # global scope.  try to figure out something better; maybe
            # a type-neutral |using| that works for C++ and protocol
            # types?
            qname = p.qname()
            if 0 == len(qname.quals):
                fullname = None
            else:
                fullname = str(qname)
            p.decl = self.declare(
                loc=p.loc,
                type=ProtocolType(qname, p.sendSemantics,
                                  stateless=(0 == len(p.transitionStmts))),
                shortname=p.name,
                fullname=fullname)

            # XXX ugh, this sucks.  but we need this information to compute
            # what friend decls we need in generated C++
            p.decl.type._ast = p

        # make sure we have decls for all dependent protocols
        for pinc in tu.includes:
            pinc.accept(self)

        # declare imported (and builtin) C++ types
        for using in tu.builtinUsing:
            using.accept(self)
        for using in tu.using:
            using.accept(self)

        # first pass to "forward-declare" all structs and unions in
        # order to support recursive definitions
        for su in tu.structsAndUnions:
            self.declareStructOrUnion(su)

        # second pass to check each definition
        for su in tu.structsAndUnions:
            su.accept(self)
        for inc in tu.includes:
            if inc.tu.filetype == 'header':
                for su in inc.tu.structsAndUnions:
                    su.accept(self)

        if tu.protocol:
            # grab symbols in the protocol itself
            p.accept(self)


        tu.type = VOID

        self.symtab = savedSymtab

    def declareStructOrUnion(self, su):
        if hasattr(su, 'decl'):
            self.symtab.declare(su.decl)
            return

        qname = su.qname()
        if 0 == len(qname.quals):
            fullname = None
        else:
            fullname = str(qname)

        if isinstance(su, StructDecl):
            sutype = StructType(qname, [ ])
        elif isinstance(su, UnionDecl):
            sutype = UnionType(qname, [ ])
        else: assert 0 and 'unknown type'

        # XXX more suckage.  this time for pickling structs/unions
        # declared in headers.
        sutype._ast = su

        su.decl = self.declare(
            loc=su.loc,
            type=sutype,
            shortname=su.name,
            fullname=fullname)


    def visitInclude(self, inc):
        if inc.tu is None:
            self.error(
                inc.loc,
                "(type checking here will be unreliable because of an earlier error)")
            return
        inc.tu.accept(self)
        if inc.tu.protocol:
            self.symtab.declare(inc.tu.protocol.decl)
        else:
            # This is a header.  Import its "exported" globals into
            # our scope.
            for using in inc.tu.using:
                using.accept(self)
            for su in inc.tu.structsAndUnions:
                self.declareStructOrUnion(su)

    def visitStructDecl(self, sd):
        # If we've already processed this struct, don't do it again.
        if hasattr(sd, 'symtab'):
            return

        stype = sd.decl.type

        self.symtab.enterScope(sd)

        for f in sd.fields:
            ftypedecl = self.symtab.lookup(str(f.typespec))
            if ftypedecl is None:
                self.error(f.loc, "field `%s' of struct `%s' has unknown type `%s'",
                           f.name, sd.name, str(f.typespec))
                continue

            f.decl = self.declare(
                loc=f.loc,
                type=self._canonicalType(ftypedecl.type, f.typespec),
                shortname=f.name,
                fullname=None)
            stype.fields.append(f.decl.type)

        self.symtab.exitScope(sd)

    def visitUnionDecl(self, ud):
        utype = ud.decl.type

        # If we've already processed this union, don't do it again.
        if len(utype.components):
            return
        
        for c in ud.components:
            cdecl = self.symtab.lookup(str(c))
            if cdecl is None:
                self.error(c.loc, "unknown component type `%s' of union `%s'",
                           str(c), ud.name)
                continue
            utype.components.append(self._canonicalType(cdecl.type, c))

    def visitUsingStmt(self, using):
        fullname = str(using.type)
        if using.type.basename() == fullname:
            fullname = None
        if fullname == 'mozilla::ipc::Shmem':
            ipdltype = ShmemType(using.type.spec)
        else:
            ipdltype = ImportedCxxType(using.type.spec)
        using.decl = self.declare(
            loc=using.loc,
            type=ipdltype,
            shortname=using.type.basename(),
            fullname=fullname)

    def visitProtocol(self, p):
        # protocol scope
        self.symtab.enterScope(p)

        for spawns in p.spawnsStmts:
            spawns.accept(self)

        for bridges in p.bridgesStmts:
            bridges.accept(self)

        for opens in p.opensStmts:
            opens.accept(self)

        seenmgrs = set()
        for mgr in p.managers:
            if mgr.name in seenmgrs:
                self.error(mgr.loc, "manager `%s' appears multiple times",
                           mgr.name)
                continue

            seenmgrs.add(mgr.name)
            mgr.of = p
            mgr.accept(self)

        for managed in p.managesStmts:
            managed.manager = p
            managed.accept(self)

        if 0 == len(p.managers) and 0 == len(p.messageDecls):
            self.error(p.loc,
                       "top-level protocol `%s' cannot be empty",
                       p.name)

        setattr(self, 'currentProtocolDecl', p.decl)
        for msg in p.messageDecls:
            msg.accept(self)
        del self.currentProtocolDecl

        p.decl.type.hasDelete = (not not self.symtab.lookup(_DELETE_MSG))
        if not (p.decl.type.hasDelete or p.decl.type.isToplevel()):
            self.error(
                p.loc,
                "destructor declaration `%s(...)' required for managed protocol `%s'",
                _DELETE_MSG, p.name)

        for managed in p.managesStmts:
            mgdname = managed.name
            ctordecl = self.symtab.lookup(mgdname +'Constructor')

            if not (ctordecl and ctordecl.type.isCtor()):
                self.error(
                    managed.loc,
                    "constructor declaration required for managed protocol `%s' (managed by protocol `%s')",
                    mgdname, p.name)

        p.states = { }
        
        if len(p.transitionStmts):
            p.startStates = [ ts for ts in p.transitionStmts
                              if ts.state.start ]
            if 0 == len(p.startStates):
                p.startStates = [ p.transitionStmts[0] ]

        # declare implicit "any" and "dead" states
        self.declare(loc=State.ANY.loc,
                     type=StateType(p.decl.type, State.ANY.name, start=False),
                     progname=State.ANY.name)
        self.declare(loc=State.DEAD.loc,
                     type=StateType(p.decl.type, State.DEAD.name, start=False),
                     progname=State.DEAD.name)

        # declare each state before decorating their mention
        for trans in p.transitionStmts:
            p.states[trans.state] = trans
            trans.state.decl = self.declare(
                loc=trans.state.loc,
                type=StateType(p.decl.type, trans.state, trans.state.start),
                progname=trans.state.name)

        for trans in p.transitionStmts:
            self.seentriggers = set()
            trans.accept(self)

        if not (p.decl.type.stateless
                or (p.decl.type.isToplevel()
                    and None is self.symtab.lookup(_DELETE_MSG))):
            # add a special state |state DEAD: null goto DEAD;|
            deadtrans = TransitionStmt.makeNullStmt(State.DEAD)
            p.states[State.DEAD] = deadtrans           

        # visit the message decls once more and resolve the state names
        # attached to actor params and returns
        def resolvestate(loc, actortype):
            assert actortype.isIPDL() and actortype.isActor()

            # already resolved this guy's state
            if isinstance(actortype.state, Decl):
                return
            
            if actortype.state is None:
                # we thought this was a C++ type until type checking,
                # when we realized it was an IPDL actor type.  But
                # that means that the actor wasn't specified to be in
                # any particular state
                actortype.state = State.ANY

            statename = actortype.state.name
            # FIXME/cjones: this is just wrong.  we need the symbol table
            # of the protocol this actor refers to.  low priority bug
            # since nobody's using this feature yet
            statedecl = self.symtab.lookup(statename)
            if statedecl is None:
                self.error(
                    loc,
                    "protocol `%s' does not have the state `%s'",
                    actortype.protocol.name(),
                    statename)
            elif not statedecl.type.isState():
                self.error(
                    loc,
                    "tag `%s' is supposed to be of state type, but is instead of type `%s'",
                    statename,
                    statedecl.type.typename())
            else:
                actortype.state = statedecl.type

        for msg in p.messageDecls:
            for iparam in msg.inParams:
                loc = iparam.loc
                for actortype in iteractortypes(iparam.type):
                    resolvestate(loc, actortype)
            for oparam in msg.outParams:
                loc = oparam.loc
                for actortype in iteractortypes(oparam.type):
                    resolvestate(loc, actortype)

        # FIXME/cjones declare all the little C++ thingies that will
        # be generated.  they're not relevant to IPDL itself, but
        # those ("invisible") symbols can clash with others in the
        # IPDL spec, and we'd like to catch those before C++ compilers
        # are allowed to obfuscate the error

        self.symtab.exitScope(p)


    def visitSpawnsStmt(self, spawns):
        pname = spawns.proto
        spawns.proto = self.symtab.lookup(pname)
        if spawns.proto is None:
            self.error(spawns.loc,
                       "spawned protocol `%s' has not been declared",
                       pname)

    def visitBridgesStmt(self, bridges):
        def lookup(p):
            decl = self.symtab.lookup(p)
            if decl is None:
                self.error(bridges.loc,
                           "bridged protocol `%s' has not been declared", p)
            return decl
        bridges.parentSide = lookup(bridges.parentSide)
        bridges.childSide = lookup(bridges.childSide)

    def visitOpensStmt(self, opens):
        pname = opens.proto
        opens.proto = self.symtab.lookup(pname)
        if opens.proto is None:
            self.error(opens.loc,
                       "opened protocol `%s' has not been declared",
                       pname)


    def visitManager(self, mgr):
        mgrdecl = self.symtab.lookup(mgr.name)
        pdecl = mgr.of.decl
        assert pdecl

        pname, mgrname = pdecl.shortname, mgr.name
        loc = mgr.loc

        if mgrdecl is None:
            self.error(
                loc,
                "protocol `%s' referenced as |manager| of `%s' has not been declared",
                mgrname, pname)
        elif not isinstance(mgrdecl.type, ProtocolType):
            self.error(
                loc,
                "entity `%s' referenced as |manager| of `%s' is not of `protocol' type; instead it is of type `%s'",
                mgrname, pname, mgrdecl.type.typename())
        else:
            mgr.decl = mgrdecl
            pdecl.type.addManager(mgrdecl.type)


    def visitManagesStmt(self, mgs):
        mgsdecl = self.symtab.lookup(mgs.name)
        pdecl = mgs.manager.decl
        assert pdecl

        pname, mgsname = pdecl.shortname, mgs.name
        loc = mgs.loc

        if mgsdecl is None:
            self.error(loc,
                       "protocol `%s', managed by `%s', has not been declared",
                       mgsname, pname)
        elif not isinstance(mgsdecl.type, ProtocolType):
            self.error(
                loc,
                "%s declares itself managing a non-`protocol' entity `%s' of type `%s'",
                pname, mgsname, mgsdecl.type.typename())
        else:
            mgs.decl = mgsdecl
            pdecl.type.manages.append(mgsdecl.type)


    def visitMessageDecl(self, md):
        msgname = md.name
        loc = md.loc

        isctor = False
        isdtor = False
        cdtype = None

        decl = self.symtab.lookup(msgname)
        if decl is not None and decl.type.isProtocol():
            # probably a ctor.  we'll check validity later.
            msgname += 'Constructor'
            isctor = True
            cdtype = decl.type
        elif decl is not None:
            self.error(loc, "message name `%s' already declared as `%s'",
                       msgname, decl.type.typename())
            # if we error here, no big deal; move on to find more

        if _DELETE_MSG == msgname:
            isdtor = True
            cdtype = self.currentProtocolDecl.type


        # enter message scope
        self.symtab.enterScope(md)

        msgtype = MessageType(md.sendSemantics, md.direction,
                              ctor=isctor, dtor=isdtor, cdtype=cdtype)

        # replace inparam Param nodes with proper Decls
        def paramToDecl(param):
            ptname = param.typespec.basename()
            ploc = param.typespec.loc

            ptdecl = self.symtab.lookup(ptname)
            if ptdecl is None:
                self.error(
                    ploc,
                    "argument typename `%s' of message `%s' has not been declared",
                    ptname, msgname)
                ptype = VOID
            else:
                ptype = self._canonicalType(ptdecl.type, param.typespec,
                                            chmodallowed=1)
            return self.declare(loc=ploc,
                                type=ptype,
                                progname=param.name)

        for i, inparam in enumerate(md.inParams):
            pdecl = paramToDecl(inparam)
            msgtype.params.append(pdecl.type)
            md.inParams[i] = pdecl
        for i, outparam in enumerate(md.outParams):
            pdecl = paramToDecl(outparam)
            msgtype.returns.append(pdecl.type)
            md.outParams[i] = pdecl

        self.symtab.exitScope(md)

        md.decl = self.declare(
            loc=loc,
            type=msgtype,
            progname=msgname)
        md.protocolDecl = self.currentProtocolDecl
        md.decl._md = md


    def visitTransitionStmt(self, ts):
        self.seentriggers = set()
        TcheckVisitor.visitTransitionStmt(self, ts)

    def visitTransition(self, t):
        loc = t.loc

        # check the trigger message
        mname = t.msg
        if t in self.seentriggers:
            self.error(loc, "trigger `%s' appears multiple times", t.msg)
        self.seentriggers.add(t)

        mdecl = self.symtab.lookup(mname)
        if mdecl is not None and mdecl.type.isIPDL() and mdecl.type.isProtocol():
            mdecl = self.symtab.lookup(mname +'Constructor')
        
        if mdecl is None:
            self.error(loc, "message `%s' has not been declared", mname)
        elif not mdecl.type.isMessage():
            self.error(
                loc,
                "`%s' should have message type, but instead has type `%s'",
                mname, mdecl.type.typename())
        else:
            t.msg = mdecl

        # check the to-states
        seenstates = set()
        for toState in t.toStates:
            sname = toState.name
            sdecl = self.symtab.lookup(sname)

            if sname in seenstates:
                self.error(loc, "to-state `%s' appears multiple times", sname)
            seenstates.add(sname)

            if sdecl is None:
                self.error(loc, "state `%s' has not been declared", sname)
            elif not sdecl.type.isState():
                self.error(
                    loc, "`%s' should have state type, but instead has type `%s'",
                    sname, sdecl.type.typename())
            else:
                toState.decl = sdecl
                toState.start = sdecl.type.start

        t.toStates = set(t.toStates)


    def _canonicalType(self, itype, typespec, chmodallowed=0):
        loc = typespec.loc
        
        if itype.isIPDL():
            if itype.isProtocol():
                itype = ActorType(itype,
                                  state=typespec.state,
                                  nullable=typespec.nullable)
            # FIXME/cjones: ShmemChmod is disabled until bug 524193
            if 0 and chmodallowed and itype.isShmem():
                itype = ShmemChmodType(
                    itype,
                    myChmod=typespec.myChmod,
                    otherChmod=typespec.otherChmod)

        if ((typespec.myChmod or typespec.otherChmod)
            and not (itype.isIPDL() and (itype.isShmem() or itype.isChmod()))):
            self.error(
                loc,
                "fine-grained access controls make no sense for type `%s'",
                itype.name())

        if not chmodallowed and (typespec.myChmod or typespec.otherChmod):
            self.error(loc, "fine-grained access controls not allowed here")

        if typespec.nullable and not (itype.isIPDL() and itype.isActor()):
            self.error(
                loc,
                "`nullable' qualifier for type `%s' makes no sense",
                itype.name())

        if typespec.array:
            itype = ArrayType(itype)

        return itype


##-----------------------------------------------------------------------------

def checkcycles(p, stack=None):
    cycles = []

    if stack is None:
        stack = []

    for cp in p.manages:
        # special case for self-managed protocols
        if cp is p:
            continue
        
        if cp in stack:
            return [stack + [p, cp]]
        cycles += checkcycles(cp, stack + [p])

    return cycles

def formatcycles(cycles):
    r = []
    for cycle in cycles:
        s = " -> ".join([ptype.name() for ptype in cycle])
        r.append("`%s'" % s)
    return ", ".join(r)


def fullyDefined(t, exploring=None):
    '''The rules for "full definition" of a type are
  defined(atom)             := true
  defined(array basetype)   := defined(basetype)
  defined(struct f1 f2...)  := defined(f1) and defined(f2) and ...
  defined(union c1 c2 ...)  := defined(c1) or defined(c2) or ...
'''
    if exploring is None:
        exploring = set()

    if t.isAtom():
        return True
    elif t.isArray():
        return fullyDefined(t.basetype, exploring)
    elif t.defined:
        return True
    assert t.isCompound()

    if t in exploring:
        return False

    exploring.add(t)
    for c in t.itercomponents():
        cdefined = fullyDefined(c, exploring)
        if t.isStruct() and not cdefined:
            t.defined = False
            break
        elif t.isUnion() and cdefined:
            t.defined = True
            break
    else:
        if t.isStruct():   t.defined = True
        elif t.isUnion():  t.defined = False
    exploring.remove(t)

    return t.defined


class CheckTypes(TcheckVisitor):
    def __init__(self, errors):
        # don't need the symbol table, we just want the error reporting
        TcheckVisitor.__init__(self, None, errors)
        self.visited = set()
        self.ptype = None

    def visitInclude(self, inc):
        if inc.tu.filename in self.visited:
            return
        self.visited.add(inc.tu.filename)
        if inc.tu.protocol:
            inc.tu.protocol.accept(self)


    def visitStructDecl(self, sd):
        if not fullyDefined(sd.decl.type):
            self.error(sd.decl.loc,
                       "struct `%s' is only partially defined", sd.name)

    def visitUnionDecl(self, ud):
        if not fullyDefined(ud.decl.type):
            self.error(ud.decl.loc,
                       "union `%s' is only partially defined", ud.name)


    def visitProtocol(self, p):
        self.ptype = p.decl.type
        
        # check that we require no more "power" than our manager protocols
        ptype, pname = p.decl.type, p.decl.shortname

        if len(p.spawnsStmts) and not ptype.isToplevel():
            self.error(p.decl.loc,
                       "protocol `%s' is not top-level and so cannot declare |spawns|",
                       pname)

        if len(p.bridgesStmts) and not ptype.isToplevel():
            self.error(p.decl.loc,
                       "protocol `%s' is not top-level and so cannot declare |bridges|",
                       pname)

        if len(p.opensStmts) and not ptype.isToplevel():
            self.error(p.decl.loc,
                       "protocol `%s' is not top-level and so cannot declare |opens|",
                       pname)

        for mgrtype in ptype.managers:
            if mgrtype is not None and ptype.needsMoreJuiceThan(mgrtype):
                self.error(
                    p.decl.loc,
                    "protocol `%s' requires more powerful send semantics than its manager `%s' provides",
                    pname, mgrtype.name())

        # XXX currently we don't require a delete() message of top-level
        # actors.  need to let experience guide this decision
        if not ptype.isToplevel():
            for md in p.messageDecls:
                if _DELETE_MSG == md.name: break
            else:
                self.error(
                    p.decl.loc,
                   "managed protocol `%s' requires a `delete()' message to be declared",
                    p.name)
        else:
            cycles = checkcycles(p.decl.type)
            if cycles:
                self.error(
                    p.decl.loc,
                    "cycle(s) detected in manager/manages heirarchy: %s",
                    formatcycles(cycles))

        if 1 == len(ptype.managers) and ptype is ptype.manager():
            self.error(
                p.decl.loc,
                "top-level protocol `%s' cannot manage itself",
                p.name)

        return Visitor.visitProtocol(self, p)


    def visitSpawnsStmt(self, spawns):
        if not self.ptype.isToplevel():
            self.error(spawns.loc,
                       "only top-level protocols can have |spawns| statements; `%s' cannot",
                       self.ptype.name())
            return

        spawnedType = spawns.proto.type
        if not (spawnedType.isIPDL() and spawnedType.isProtocol()
                and spawnedType.isToplevel()):
            self.error(spawns.loc,
                       "cannot spawn non-top-level-protocol `%s'",
                       spawnedType.name())
        else:
            self.ptype.addSpawn(spawnedType)


    def visitBridgesStmt(self, bridges):
        if not self.ptype.isToplevel():
            self.error(bridges.loc,
                       "only top-level protocols can have |bridges| statements; `%s' cannot",
                       self.ptype.name())
            return

        parentType = bridges.parentSide.type
        childType = bridges.childSide.type
        if not (parentType.isIPDL() and parentType.isProtocol()
                and childType.isIPDL() and childType.isProtocol()
                and parentType.isToplevel() and childType.isToplevel()):
            self.error(bridges.loc,
                       "cannot bridge non-top-level-protocol(s) `%s' and `%s'",
                       parentType.name(), childType.name())


    def visitOpensStmt(self, opens):
        if not self.ptype.isToplevel():
            self.error(opens.loc,
                       "only top-level protocols can have |opens| statements; `%s' cannot",
                       self.ptype.name())
            return

        openedType = opens.proto.type
        if not (openedType.isIPDL() and openedType.isProtocol()
                and openedType.isToplevel()):
            self.error(opens.loc,
                       "cannot open non-top-level-protocol `%s'",
                       openedType.name())
        else:
            self.ptype.addOpen(openedType)


    def visitManagesStmt(self, mgs):
        pdecl = mgs.manager.decl
        ptype, pname = pdecl.type, pdecl.shortname

        mgsdecl = mgs.decl
        mgstype, mgsname = mgsdecl.type, mgsdecl.shortname

        loc = mgs.loc

        # we added this information; sanity check it
        assert ptype.isManagerOf(mgstype)

        # check that the "managed" protocol agrees
        if not mgstype.isManagedBy(ptype):
            self.error(
                loc,
                "|manages| declaration in protocol `%s' does not match any |manager| declaration in protocol `%s'",
                pname, mgsname)


    def visitManager(self, mgr):
        # FIXME/bug 541126: check that the protocol graph is acyclic
        
        pdecl = mgr.of.decl
        ptype, pname = pdecl.type, pdecl.shortname

        mgrdecl = mgr.decl
        mgrtype, mgrname = mgrdecl.type, mgrdecl.shortname

        # we added this information; sanity check it
        assert ptype.isManagedBy(mgrtype)

        loc = mgr.loc

        # check that the "manager" protocol agrees
        if not mgrtype.isManagerOf(ptype):
            self.error(
                loc,
                "|manager| declaration in protocol `%s' does not match any |manages| declaration in protocol `%s'",
                pname, mgrname)


    def visitMessageDecl(self, md):
        mtype, mname = md.decl.type, md.decl.progname
        ptype, pname = md.protocolDecl.type, md.protocolDecl.shortname

        loc = md.decl.loc

        if mtype.isSync() and (mtype.isOut() or mtype.isInout()):
            self.error(
                loc,
                "sync parent-to-child messages are verboten (here, message `%s' in protocol `%s')",
                mname, pname)

        if mtype.needsMoreJuiceThan(ptype):
            self.error(
                loc,
                "message `%s' requires more powerful send semantics than its protocol `%s' provides",
                mname, pname)

        if mtype.isAsync() and len(mtype.returns):
            # XXX/cjones could modify grammar to disallow this ...
            self.error(loc,
                       "asynchronous message `%s' declares return values",
                       mname)

        if mtype.isCtor() and not ptype.isManagerOf(mtype.constructedType()):
            self.error(
                loc,
                "ctor for protocol `%s', which is not managed by protocol `%s'", 
                mname[:-len('constructor')], pname)


    def visitTransition(self, t):
        _YNC = [ ASYNC, SYNC ]

        loc = t.loc
        impliedDirection, impliedSems = {
            SEND: [ OUT, _YNC ], RECV: [ IN, _YNC ],
            CALL: [ OUT, RPC ],  ANSWER: [ IN, RPC ],
         } [t.trigger]
        
        if (OUT is impliedDirection and t.msg.type.isIn()
            or IN is impliedDirection and t.msg.type.isOut()
            or _YNC is impliedSems and t.msg.type.isRpc()
            or RPC is impliedSems and (not t.msg.type.isRpc())):
            mtype = t.msg.type

            self.error(
                loc, "%s %s message `%s' is not `%s'd",
                mtype.sendSemantics.pretty, mtype.direction.pretty,
                t.msg.progname,
                t.trigger.pretty)

##-----------------------------------------------------------------------------

class Process:
    def __init__(self):
        self.actors = set()         # set(Actor)
        self.edges = { }            # Actor -> [ SpawnsEdge ]
        self.spawn = set()          # set(Actor)

    def edge(self, spawner, spawn):
        if spawner not in self.edges:  self.edges[spawner] = [ ]
        self.edges[spawner].append(SpawnsEdge(spawner, spawn))
        self.spawn.add(spawn)

    def iteredges(self):
        for edgelist in self.edges.itervalues():
            for edge in edgelist:
                yield edge

    def merge(self, o):
        'Merge the Process |o| into this Process'
        if self == o:
            return
        for actor in o.actors:
            ProcessGraph.actorToProcess[actor] = self
        self.actors.update(o.actors)
        self.edges.update(o.edges)
        self.spawn.update(o.spawn)
        ProcessGraph.processes.remove(o)

    def spawns(self, actor):
        return actor in self.spawn

    def __cmp__(self, o):  return cmp(self.actors, o.actors)
    def __eq__(self, o):   return self.actors == o.actors
    def __hash__(self):    return hash(id(self))
    def __repr__(self):
        return reduce(lambda a, x: str(a) + str(x) +'|', self.actors, '|')
    def __str__(self):     return repr(self)

class Actor:
    def __init__(self, ptype, side):
        self.ptype = ptype
        self.side = side

    def asType(self):
        return ActorType(self.ptype)
    def other(self):
        return Actor(self.ptype, _otherside(self.side))

    def __cmp__(self, o):
        return cmp(self.ptype, o.ptype) or cmp(self.side, o.side)
    def __eq__(self, o):
        return self.ptype == o.ptype and self.side == o.side
    def __hash__(self):  return hash(repr(self))
    def __repr__(self):  return '%s%s'% (self.ptype.name(), self.side.title())
    def __str__(self):   return repr(self)

class SpawnsEdge:
    def __init__(self, spawner, spawn):
        self.spawner = spawner      # Actor
        self.spawn = spawn          # Actor
    def __repr__(self):
        return '(%r)--spawns-->(%r)'% (self.spawner, self.spawn)
    def __str__(self):  return repr(self)

class BridgeEdge:
    def __init__(self, bridgeProto, parent, child):
        self.bridgeProto = bridgeProto # ProtocolType
        self.parent = parent           # Actor
        self.child = child             # Actor
    def __repr__(self):
        return '(%r)--%s bridge-->(%r)'% (
            self.parent, self.bridgeProto.name(), self.child)
    def __str__(self):  return repr(self)

class OpensEdge:
    def __init__(self, opener, openedProto):
        self.opener = opener            # Actor
        self.openedProto = openedProto  # ProtocolType
    def __repr__(self):
        return '(%r)--opens-->(%s)'% (self.opener, self.openedProto.name())
    def __str__(self):  return repr(self)

# "singleton" class with state that persists across type checking of
# all protocols
class ProcessGraph:
    processes = set()                   # set(Process)
    bridges = { }                       # ProtocolType -> [ BridgeEdge ]
    opens = { }                         # ProtocolType -> [ OpensEdge ]
    actorToProcess = { }                # Actor -> Process
    visitedSpawns = set()               # set(ActorType)
    visitedBridges = set()              # set(ActorType)

    @classmethod
    def findProcess(cls, actor):
        return cls.actorToProcess.get(actor, None)

    @classmethod
    def getProcess(cls, actor):
        if actor not in cls.actorToProcess:
            p = Process()
            p.actors.add(actor)
            cls.processes.add(p)
            cls.actorToProcess[actor] = p
        return cls.actorToProcess[actor]

    @classmethod
    def bridgesOf(cls, bridgeP):
        return cls.bridges.get(bridgeP, [])

    @classmethod
    def bridgeEndpointsOf(cls, ptype, side):
        actor = Actor(ptype, side)
        endpoints = []
        for b in cls.iterbridges():
            if b.parent == actor:
                endpoints.append(Actor(b.bridgeProto, 'parent'))
            elif b.child == actor:
                endpoints.append(Actor(b.bridgeProto, 'child'))
        return endpoints

    @classmethod
    def iterbridges(cls):
        for edges in cls.bridges.itervalues():
            for bridge in edges:
                yield bridge

    @classmethod
    def opensOf(cls, openedP):
        return cls.opens.get(openedP, [])

    @classmethod
    def opensEndpointsOf(cls, ptype, side):
        actor = Actor(ptype, side)
        endpoints = []
        for o in cls.iteropens():
            if actor == o.opener:
                endpoints.append(Actor(o.openedProto, o.opener.side))
            elif actor == o.opener.other():
                endpoints.append(Actor(o.openedProto, o.opener.other().side))
        return endpoints

    @classmethod
    def iteropens(cls):
        for edges in cls.opens.itervalues():
            for opens in edges:
                yield opens

    @classmethod
    def spawn(cls, spawner, remoteSpawn):
        localSpawn = remoteSpawn.other()
        spawnerProcess = ProcessGraph.getProcess(spawner)
        spawnerProcess.merge(ProcessGraph.getProcess(localSpawn))
        spawnerProcess.edge(spawner, remoteSpawn)

    @classmethod
    def bridge(cls, parent, child, bridgeP):
        bridgeParent = Actor(bridgeP, 'parent')
        parentProcess = ProcessGraph.getProcess(parent)
        parentProcess.merge(ProcessGraph.getProcess(bridgeParent))
        bridgeChild = Actor(bridgeP, 'child')
        childProcess = ProcessGraph.getProcess(child)
        childProcess.merge(ProcessGraph.getProcess(bridgeChild))
        if bridgeP not in cls.bridges:
            cls.bridges[bridgeP] = [ ]
        cls.bridges[bridgeP].append(BridgeEdge(bridgeP, parent, child))

    @classmethod
    def open(cls, opener, opened, openedP):
        remoteOpener, remoteOpened, = opener.other(), opened.other()
        openerProcess = ProcessGraph.getProcess(opener)
        openerProcess.merge(ProcessGraph.getProcess(opened))
        remoteOpenerProcess = ProcessGraph.getProcess(remoteOpener)
        remoteOpenerProcess.merge(ProcessGraph.getProcess(remoteOpened))
        if openedP not in cls.opens:
            cls.opens[openedP] = [ ]
        cls.opens[openedP].append(OpensEdge(opener, openedP))


class BuildProcessGraph(TcheckVisitor):
    class findSpawns(TcheckVisitor):
        def __init__(self, errors):
            TcheckVisitor.__init__(self, None, errors)

        def visitTranslationUnit(self, tu):
            TcheckVisitor.visitTranslationUnit(self, tu)

        def visitInclude(self, inc):
            if inc.tu.protocol:
                inc.tu.protocol.accept(self)

        def visitProtocol(self, p):
            ptype = p.decl.type
            # non-top-level protocols don't add any information
            if not ptype.isToplevel() or ptype in ProcessGraph.visitedSpawns:
                return

            ProcessGraph.visitedSpawns.add(ptype)
            self.visiting = ptype
            ProcessGraph.getProcess(Actor(ptype, 'parent'))
            ProcessGraph.getProcess(Actor(ptype, 'child'))
            return TcheckVisitor.visitProtocol(self, p)

        def visitSpawnsStmt(self, spawns):
            # The picture here is:
            #  [ spawner | localSpawn | ??? ]  (process 1)
            #                  |
            #                  |
            #            [ remoteSpawn | ???]  (process 2)
            #
            # A spawns stmt tells us that |spawner| and |localSpawn|
            # are in the same process.
            spawner = Actor(self.visiting, spawns.side)
            remoteSpawn = Actor(spawns.proto.type, spawns.spawnedAs)
            ProcessGraph.spawn(spawner, remoteSpawn)

    def __init__(self, errors):
        TcheckVisitor.__init__(self, None, errors)
        self.visiting = None            # ActorType
        self.visited = set()            # set(ActorType)

    def visitTranslationUnit(self, tu):
        tu.accept(self.findSpawns(self.errors))
        TcheckVisitor.visitTranslationUnit(self, tu)

    def visitInclude(self, inc):
        if inc.tu.protocol:
            inc.tu.protocol.accept(self)

    def visitProtocol(self, p):
        ptype = p.decl.type
        # non-top-level protocols don't add any information
        if not ptype.isToplevel() or ptype in ProcessGraph.visitedBridges:
            return

        ProcessGraph.visitedBridges.add(ptype)
        self.visiting = ptype
        return TcheckVisitor.visitProtocol(self, p)

    def visitBridgesStmt(self, bridges):
        bridgeProto = self.visiting
        parentSideProto = bridges.parentSide.type
        childSideProto = bridges.childSide.type

        # the picture here is:
        #                                                   (process 1|
        #  [ parentSide(Parent|Child) | childSide(Parent|Child) | ... ]
        #         |                                       |
        #         |                        (process 2|    |
        #  [ parentSide(Child|Parent) | bridgeParent ]    |
        #                                   |             |
        #                                   |             |       (process 3|
        #                           [ bridgeChild | childSide(Child|Parent) ]
        #
        # First we have to figure out which parentSide/childSide
        # actors live in the same process.  The possibilities are {
        # parent, child } x { parent, child }.  (Multiple matches
        # aren't allowed yet.)  Then we make ProcessGraph aware of the
        # new bridge.
        parentSideActor, childSideActor = None, None
        pc = ( 'parent', 'child' )
        for parentSide, childSide in cartesian_product(pc, pc):
            pactor = Actor(parentSideProto, parentSide)
            pproc = ProcessGraph.findProcess(pactor)
            cactor = Actor(childSideProto, childSide)
            cproc = ProcessGraph.findProcess(cactor)
            assert pproc and cproc

            if pproc == cproc:
                if parentSideActor is not None:
                    self.error(bridges.loc,
                               "ambiguous bridge `%s' between `%s' and `%s'",
                               bridgeProto.type.name(),
                               parentSideProto.name(),
                               childSideProto.name())
                else:
                    parentSideActor, childSideActor = pactor.other(), cactor.other()

        if parentSideActor is None:
            self.error(bridges.loc,
                       "`%s' and `%s' cannot be bridged by `%s' ",
                       parentSideProto.name(), childSideProto.name(),
                       bridgeProto.name())

        ProcessGraph.bridge(parentSideActor, childSideActor, bridgeProto)

    def visitOpensStmt(self, opens):
        openedP = opens.proto.type
        opener = Actor(self.visiting, opens.side)
        opened = Actor(openedP, opens.side)

        # The picture here is:
        #  [ opener       | opened ]   (process 1)
        #      |               |
        #      |               |
        #  [ remoteOpener | remoteOpened ]  (process 2)
        #
        # An opens stmt tells us that the pairs |opener|/|opened|
        # and |remoteOpener|/|remoteOpened| are each in the same
        # process.
        ProcessGraph.open(opener, opened, openedP)


class CheckProcessGraph(TcheckVisitor):
    def __init__(self, errors):
        TcheckVisitor.__init__(self, None, errors)

    # TODO: verify spawns-per-process assumption and check that graph
    # is a dag
    def visitTranslationUnit(self, tu):
        if 0:
            print 'Processes'
            for process in ProcessGraph.processes:
                print '  ', process
                for edge in process.iteredges():
                    print '    ', edge
            print 'Bridges'
            for bridgeList in ProcessGraph.bridges.itervalues():
                for bridge in bridgeList:
                    print '  ', bridge
            print 'Opens'
            for opensList in ProcessGraph.opens.itervalues():
                for opens in opensList:
                    print '  ', opens

##-----------------------------------------------------------------------------

class CheckStateMachine(TcheckVisitor):
    def __init__(self, errors):
        # don't need the symbol table, we just want the error reporting
        TcheckVisitor.__init__(self, None, errors)
        self.p = None

    def visitProtocol(self, p):
        self.p = p
        self.checkReachability(p)
        for ts in p.transitionStmts:
            ts.accept(self)

    def visitTransitionStmt(self, ts):
        # We want to disallow "race conditions" in protocols.  These
        # can occur when a protocol state machine has a state that
        # allows triggers of opposite direction.  That declaration
        # allows the parent to send the child a message at the
        # exact instance the child sends the parent a message.  One of
        # those messages would (probably) violate the state machine
        # and cause the child to be terminated.  It's obviously very
        # nice if we can forbid this at the level of IPDL state
        # machines, rather than resorting to static or dynamic
        # checking of C++ implementation code.
        #
        # An easy way to avoid this problem in IPDL is to only allow
        # "unidirectional" protocol states; that is, from each state,
        # only send or only recv triggers are allowed.  This approach
        # is taken by the Singularity project's "contract-based
        # message channels."  However, this can be something of a
        # notational burden for stateful protocols.
        #
        # If two messages race, the effect is that the parent's and
        # child's states get temporarily out of sync.  Informally,
        # IPDL allows this *only if* the state machines get out of
        # sync for only *one* step (state machine transition), then
        # sync back up.  This is a design decision: the states could
        # be allowd to get out of sync for any constant k number of
        # steps.  (If k is unbounded, there's no point in presenting
        # the abstraction of parent and child actor states being
        # "entangled".)  The working hypothesis is that the more steps
        # the states are allowed to be out of sync, the harder it is
        # to reason about the protocol.
        #
        # Slightly less informally, two messages are allowed to race
        # only if processing them in either order leaves the protocol
        # in the same state.  That is, messages A and B are allowed to
        # race only if processing A then B leaves the protocol in
        # state S, *and* processing B then A also leaves the protocol
        # in state S.  Technically, if this holds, then messages A and
        # B could be called "commutative" wrt to actor state.
        #
        # "Formally", state machine definitions must adhere to two
        # rules.
        #
        #   *Rule 1*: from a state S, all sync triggers must be of the same
        # "direction," i.e. only |send| or only |recv|
        #
        # (Pairs of sync messages can't commute, because otherwise
        # deadlock can occur from simultaneously in-flight sync
        # requests.)
        #
        #   *Rule 2*: the "Diamond Rule".
        #   from a state S,
        #     for any pair of triggers t1 and t2,
        #         where t1 and t2 have opposite direction,
        #         and t1 transitions to state T1 and t2 to T2,
        #       then the following must be true:
        #         (T2 allows the trigger t1, transitioning to state U)
        #         and
        #         (T1 allows the trigger t2, transitioning to state U)
        #         and
        #         (
        #           (
        #             (all of T1's triggers have the same direction as t2)
        #             and
        #             (all of T2's triggers have the same direction as t1)
        #           )
        #           or
        #           (T1, T2, and U are the same "terminal state")
        #         )
        #
        # A "terminal state" S is one from which all triggers
        # transition back to S itself.
        #
        # The presence of triggers with multiple out states complicates
        # this check slightly, but doesn't fundamentally change it.
        #
        #   from a state S,
        #     for any pair of triggers t1 and t2,
        #         where t1 and t2 have opposite direction,
        #       for each pair of states (T1, T2) \in t1_out x t2_out,
        #           where t1_out is the set of outstates from t1
        #                 t2_out is the set of outstates from t2
        #                 t1_out x t2_out is their Cartesian product
        #                 and t1 transitions to state T1 and t2 to T2,
        #         then the following must be true:
        #           (T2 allows the trigger t1, with out-state set { U })
        #           and
        #           (T1 allows the trigger t2, with out-state set { U })
        #           and
        #           (
        #             (
        #               (all of T1's triggers have the same direction as t2)
        #               and
        #               (all of T2's triggers have the same direction as t1)
        #             )
        #             or
        #             (T1, T2, and U are the same "terminal state")
        #           )

        # check Rule 1
        syncdirection = None
        syncok = True
        for trans in ts.transitions:
            if not trans.msg.type.isSync(): continue
            if syncdirection is None:
                syncdirection = trans.trigger.direction()
            elif syncdirection is not trans.trigger.direction():
                self.error(
                    trans.loc,
                    "sync trigger at state `%s' in protocol `%s' has different direction from earlier sync trigger at same state",
                    ts.state.name, self.p.name)
                syncok = False
        # don't check the Diamond Rule if Rule 1 doesn't hold
        if not syncok:
            return

        # helper functions
        def triggerTargets(S, t):
            '''Return the set of states transitioned to from state |S|
upon trigger |t|, or { } if |t| is not a trigger in |S|.'''
            for trans in self.p.states[S].transitions:
                if t.trigger is trans.trigger and t.msg is trans.msg:
                    return trans.toStates
            return set()

        def allTriggersSameDirectionAs(S, t):
            '''Return true iff all the triggers from state |S| have the same
direction as trigger |t|'''
            direction = t.direction()
            for trans in self.p.states[S].transitions:
                if direction != trans.trigger.direction():
                    return False
            return True

        def terminalState(S):
            '''Return true iff |S| is a "terminal state".'''
            for trans in self.p.states[S].transitions:
                for S_ in trans.toStates:
                    if S_ != S:  return False
            return True

        def sameTerminalState(S1, S2, S3):
            '''Return true iff states |S1|, |S2|, and |S3| are all the same
"terminal state".'''
            if isinstance(S3, set):
                assert len(S3) == 1
                for S3_ in S3: pass
                S3 = S3_

            return (S1 == S2 == S3) and terminalState(S1)

        S = ts.state.name

        # check the Diamond Rule
        for (t1, t2) in unique_pairs(ts.transitions):
            # if the triggers have the same direction, they can't race,
            # since only one endpoint can initiate either (and delivery
            # is in-order)
            if t1.trigger.direction() == t2.trigger.direction():
                continue

            loc = t1.loc
            t1_out = t1.toStates
            t2_out = t2.toStates

            for (T1, T2) in cartesian_product(t1_out, t2_out):
                # U1 <- { u | T1 --t2--> u }
                U1 = triggerTargets(T1, t2)
                # U2 <- { u | T2 --t1--> u }
                U2 = triggerTargets(T2, t1)

                # don't report more than one Diamond Rule violation
                # per state. there may be O(n^4) total, way too many
                # for a human to parse
                #
                # XXX/cjones: could set a limit on #printed and stop
                # after that limit ...
                raceError = False
                errT1 = None
                errT2 = None

                if 0 == len(U1) or 0 == len(U2):
                    print "******* case 1"
                    raceError = True
                elif 1 < len(U1) or 1 < len(U2):
                    raceError = True
                    # there are potentially many unpaired states; just
                    # pick two
                    print "******* case 2"
                    for u1, u2 in cartesian_product(U1, U2):
                        if u1 != u2:
                            errT1, errT2 = u1, u2
                            break
                elif U1 != U2:
                    print "******* case 3"
                    raceError = True
                    for errT1 in U1: pass
                    for errT2 in U2: pass

                if raceError:
                    self.reportRaceError(loc, S,
                                         [ T1, t1, errT1 ],
                                         [ T2, t2, errT2 ])
                    return

                if not ((allTriggersSameDirectionAs(T1, t2.trigger)
                           and allTriggersSameDirectionAs(T2, t1.trigger))
                          or sameTerminalState(T1, T2, U1)):
                    self.reportRunawayError(loc, S, [ T1, t1, None ], [ T2, t2, None ])
                    return

    def checkReachability(self, p):
        def explore(ts, visited):
            if ts.state in visited:
                return
            visited.add(ts.state)
            for outedge in ts.transitions:
                for toState in outedge.toStates:
                    explore(p.states[toState], visited)

        checkfordelete = (State.DEAD in p.states)

        allvisited = set()         # set(State)
        for root in p.startStates:
            visited = set()

            explore(root, visited)
            allvisited.update(visited)

            if checkfordelete and State.DEAD not in visited:
                self.error(
                    root.loc,
                    "when starting from state `%s', actors of protocol `%s' cannot be deleted", root.state.name, p.name)

        for ts in p.states.itervalues():
            if ts.state is not State.DEAD and ts.state not in allvisited:
                self.error(ts.loc,
                           "unreachable state `%s' in protocol `%s'",
                           ts.state.name, p.name)


    def _normalizeTransitionSequences(self, t1Seq, t2Seq):
        T1, M1, U1 = t1Seq
        T2, M2, U2 = t2Seq
        assert M1 is not None and M2 is not None

        # make sure that T1/M1/U1 is the parent side of the race
        if M1.trigger is RECV or M1.trigger is ANSWER:
            T1, M1, U1, T2, M2, U2 = T2, M2, U2, T1, M1, U1

        def stateName(S):
            if S: return S.name
            return '[error]'

        T1 = stateName(T1)
        T2 = stateName(T2)
        U1 = stateName(U1)
        U2 = stateName(U2)

        return T1, M1.msg.progname, U1, T2, M2.msg.progname, U2
        

    def reportRaceError(self, loc, S, t1Seq, t2Seq):
        T1, M1, U1, T2, M2, U2 = self._normalizeTransitionSequences(t1Seq, t2Seq)
        self.error(
            loc,
"""in protocol `%(P)s', the sequence of events
     parent:    +--`send %(M1)s'-->( state `%(T1)s' )--`recv %(M2)s'-->( state %(U1)s )
               /
 ( state `%(S)s' )
               \\
      child:    +--`send %(M2)s'-->( state `%(T2)s' )--`recv %(M1)s'-->( state %(U2)s )
results in error(s) or leaves parent/child state out of sync for more than one step and is thus a race hazard; i.e., triggers `%(M1)s' and `%(M2)s' fail to commute in state `%(S)s'"""% {
                'P': self.p.name, 'S': S, 'M1': M1, 'M2': M2,
                'T1': T1, 'T2': T2, 'U1': U1, 'U2': U2
        })


    def reportRunawayError(self, loc, S, t1Seq, t2Seq):
        T1, M1, _, T2, M2, __ = self._normalizeTransitionSequences(t1Seq, t2Seq)
        self.error(
            loc,
        """in protocol `%(P)s', the sequence of events
     parent:    +--`send %(M1)s'-->( state `%(T1)s' )
               /
 ( state `%(S)s' )
               \\
      child:    +--`send %(M2)s'-->( state `%(T2)s' )
lead to parent/child states in which parent/child state can become more than one step out of sync (though this divergence might not lead to error conditions)"""% {
                'P': self.p.name, 'S': S, 'M1': M1, 'M2': M2, 'T1': T1, 'T2': T2
        })
