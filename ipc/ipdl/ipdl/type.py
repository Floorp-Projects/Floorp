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

from ipdl.ast import CxxInclude, Decl, Loc, QualifiedId, State, TypeSpec, UsingStmt, Visitor, ASYNC, SYNC, RPC, IN, OUT, INOUT, ANSWER, CALL, RECV, SEND
import ipdl.builtin as builtin

class Type:
    # Is this a C++ type?
    def isCxx(self):
        return False
    # Is this an IPDL type?
    def isIPDL(self):
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

class VoidType(Type):
    # the following are not a type-o's (hah): void is both a Cxx and IPDL type
    def isCxx():
        return True
    def isIPDL():
        return True
    def isVisible(self):
        return True
    def isVoid(self):
        return True

    def name(self): return 'void'
    def fullname(self): return 'void'

VOID = VoidType()

##--------------------
class CxxType(Type):
    def isCxx(self):
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

class GeneratedCxxType(CxxType):
    def isGenerated(self): return True
    def isVisible(self):   return False

##--------------------
class IPDLType(Type):
    def isIPDL(self):  return True
    def isVisible(self): return True
    def isState(self): return False
    def isMessage(self): return False
    def isProtocol(self): return False
    def isActor(self): return False

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
    def __init__(self, start=False):
        self.start = start
    def isState(self):
        return True

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

class ProtocolType(IPDLType):
    def __init__(self, qname, sendSemantics):
        self.qname = qname
        self.sendSemantics = sendSemantics
        self.manager = None
        self.manages = [ ]
    def isProtocol(self): return True

    def name(self):
        return self.qname.baseid
    def fullname(self):
        return str(self.qname)

    def managedBy(self, mgr):
        self.manager = mgr

    def toplevel(self):
        if self.isToplevel():
            return self
        return self.manager.toplevel()

    def isManagerOf(self, pt):
        for managed in self.manages:
            if pt is managed:
                return True
        return False
    def isManager(self):
        return len(self.manages) > 0
    def isManaged(self):
        return self.manager is not None
    def isToplevel(self):
        return not self.isManaged()

class ActorType(IPDLType):
    def __init__(self, protocol, state):
        self.protocol = protocol
        self.state = state
    def isActor(self): return True

    def name(self):
        return self.protocol.name()
    def fullname(self):
        return self.protocol.fullname()

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

        if (len(tu.protocol.startStates)
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

    def declareBuiltins(self):
        for using in self.builtinUsing:
            fullname = str(using.type)
            if using.type.basename() == fullname:
                fullname = None
            using.decl = self.declareLocalGlobal(
                loc=using.loc,
                type=BuiltinCxxType(using.type.spec),
                shortname=using.type.basename(),
                fullname=fullname)
            self.symtab.declare(using.decl)

    def visitTranslationUnit(self, tu):
        # all TranslationUnits declare symbols in global scope
        if hasattr(tu, 'symtab'):
            return
        tu.symtab = SymbolTable(self.errors)
        savedSymtab = self.symtab
        self.symtab = tu.symtab

        # pretend like the translation unit "using"-ed these for the
        # sake of type checking and C++ code generation
        tu.using = self.builtinUsing + tu.using

        p = tu.protocol

        # for everyone's sanity, enforce that the filename and
        # protocol name match
        basefilename = os.path.basename(tu.filename)
        expectedfilename = '%s.ipdl'% (p.name)

        if basefilename != expectedfilename:
            self.error(p.loc,
                       "expected file defining protocol `%s' to be named `%s'; instead it's named `%s'",
                       p.name, expectedfilename, basefilename)

        # FIXME/cjones: it's a little weird and counterintuitive to put
        # both the namespace and non-namespaced name in the global scope.
        # try to figure out something better; maybe a type-neutral |using|
        # that works for C++ and protocol types?
        qname = QualifiedId(p.loc, p.name,
                            [ ns.namespace for ns in p.namespaces ])
        if 0 == len(qname.quals):
            fullname = None
        else:
            fullname = str(qname)
        p.decl = self.declare(
            loc=p.loc,
            type=ProtocolType(qname, p.sendSemantics),
            shortname=p.name,
            fullname=fullname)

        # make sure we have decls for all dependent protocols
        for pinc in tu.protocolIncludes:
            pinc.accept(self)

        # declare imported (and builtin) C++ types
        for using in tu.using:
            using.accept(self)

        # grab symbols in the protocol itself
        p.accept(self)

        tu.type = VOID

        self.symtab = savedSymtab


    def visitProtocolInclude(self, pi):
        pi.tu.accept(self)
        self.symtab.declare(pi.tu.protocol.decl)

    def visitUsingStmt(self, using):
        fullname = str(using.type)
        if using.type.basename() == fullname:
            fullname = None
        using.decl = self.declare(
            loc=using.loc,
            type=ImportedCxxType(using.type.spec),
            shortname=using.type.basename(),
            fullname=fullname)

    def visitProtocol(self, p):
        # protocol scope
        self.symtab.enterScope(p)

        if p.manager is not None:
            p.manager.of = p
            p.manager.accept(self)

        for managed in p.managesStmts:
            managed.manager = p
            managed.accept(self)

        if p.manager is None and 0 == len(p.messageDecls):
            self.error(p.loc,
                       "top-level protocol `%s' cannot be empty",
                       p.name)

        setattr(self, 'currentProtocolDecl', p.decl)
        for msg in p.messageDecls:
            msg.accept(self)
        del self.currentProtocolDecl

        for managed in p.managesStmts:
            mgdname = managed.name
            ctordecl = self.symtab.lookup(mgdname +'Constructor')
            dtordecl = self.symtab.lookup(mgdname +'Destructor')

            if not(ctordecl and dtordecl
                   and ctordecl.type.isCtor() and dtordecl.type.isDtor()):
                self.error(
                    managed.loc,
                    "constructor and destructor declarations are required for managed protocol `%s' (managed by protocol `%s')",
                    mgdname, p.name)

        p.states = { }
        
        if len(p.transitionStmts):
            p.startStates = [ ts for ts in p.transitionStmts
                              if ts.state.start ]
            if 0 == len(p.startStates):
                p.startStates = [ p.transitionStmts[0] ]
                
        # declare each state before decorating their mention
        for trans in p.transitionStmts:
            p.states[trans.state] = trans
            trans.state.decl = self.declare(
                loc=trans.state.loc,
                type=StateType(trans.state.start),
                progname=trans.state.name)

        # declare implicit "any" state
        self.declare(loc=State.ANY.loc,
                     type=StateType(start=False),
                     progname=State.ANY.name)

        for trans in p.transitionStmts:
            self.seentriggers = set()
            trans.accept(self)

        # visit the message decls once more and resolve the state names
        # attached to actor params and returns
        def resolvestate(param):
            if param.type.state is None:
                # we thought this was a C++ type until type checking,
                # when we realized it was an IPDL actor type.  But
                # that means that the actor wasn't specified to be in
                # any particular state
                param.type.state = State.ANY

            loc = param.loc
            statename = param.type.state.name
            statedecl = self.symtab.lookup(statename)
            if statedecl is None:
                self.error(
                    loc,
                    "protocol `%s' does not have the state `%s'",
                    param.type.protocol.name(),
                    statename)
            elif not statedecl.type.isState():
                self.error(
                    loc,
                    "tag `%s' is supposed to be of state type, but is instead of type `%s'",
                    statename,
                    statedecl.type.typename())
            else:
                param.type.state = statedecl

        for msg in p.messageDecls:
            for iparam in msg.inParams:
                if iparam.type and iparam.type.isIPDL() and iparam.type.isActor():
                    resolvestate(iparam)
            for oparam in msg.outParams:
                if oparam.type and oparam.type.isIPDL() and oparam.type.isActor():
                    resolvestate(oparam)

        # FIXME/cjones declare all the little C++ thingies that will
        # be generated.  they're not relevant to IPDL itself, but
        # those ("invisible") symbols can clash with others in the
        # IPDL spec, and we'd like to catch those before C++ compilers
        # are allowed to obfuscate the error

        self.symtab.exitScope(p)


    def visitManagerStmt(self, mgr):
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
            assert pdecl.type.manager is None
            mgr.decl = mgrdecl
            pdecl.type.manager = mgrdecl.type


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

        if '~' == msgname[0]:
            # it's a destructor.  look up the constructed type
            msgname = msgname[1:]

            decl = self.symtab.lookup(msgname)
            if decl is None:
                self.error(loc, "type `%s' has not been declared", msgname)
            elif not decl.type.isProtocol():
                self.error(loc, "destructor for non-protocol type `%s'",
                           msgname)
            else:
                msgname += 'Destructor'
                isdtor = True
                cdtype = decl.type

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
        decl = None

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
                return None
            else:
                if ptdecl.type.isIPDL() and ptdecl.type.isProtocol():
                    ptype = ActorType(ptdecl.type,
                                      param.typespec.state)
                else:
                    ptype = ptdecl.type
                return self.declare(
                    loc=ploc,
                    type=ptype,
                    progname=param.name)

        for i, inparam in enumerate(md.inParams):
            pdecl = paramToDecl(inparam)
            if pdecl is not None:
                msgtype.params.append(pdecl.type)
                md.inParams[i] = pdecl
            else:
                md.inParams[i].type = None
        for i, outparam in enumerate(md.outParams):
            pdecl = paramToDecl(outparam)
            if pdecl is not None:
                msgtype.returns.append(pdecl.type)
                md.outParams[i] = pdecl
            else:
                md.outParams[i].type = None

        self.symtab.exitScope(md)

        md.decl = self.declare(
            loc=loc,
            type=msgtype,
            progname=msgname)
        md.protocolDecl = self.currentProtocolDecl


    def visitTransitionStmt(self, ts):
        self.seentriggers = set()
        TcheckVisitor.visitTransitionStmt(self, ts)

    def visitTransition(self, t):
        loc = t.loc

        # check the trigger message
        mname = t.msg
        if mname in self.seentriggers:
            self.error(loc, "trigger `%s' appears multiple times", mname)
        self.seentriggers.add(mname)
        
        mdecl = self.symtab.lookup(mname)
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

##-----------------------------------------------------------------------------

class CheckTypes(TcheckVisitor):
    def __init__(self, errors):
        # don't need the symbol table, we just want the error reporting
        TcheckVisitor.__init__(self, None, errors)
        self.visited = set()

    def visitProtocolInclude(self, inc):
        if inc.tu.filename in self.visited:
            return
        self.visited.add(inc.tu.filename)
        inc.tu.protocol.accept(self)


    def visitProtocol(self, p):
        # check that we require no more "power" than our manager protocol
        ptype, pname = p.decl.type, p.decl.shortname
        mgrtype = ptype.manager
        if mgrtype is not None and ptype.needsMoreJuiceThan(mgrtype):
            mgrname = p.manager.decl.shortname
            self.error(
                p.decl.loc,
                "protocol `%s' requires more powerful send semantics than its manager `%s' provides",
                pname, mgrname)

        return Visitor.visitProtocol(self, p)
        

    def visitManagesStmt(self, mgs):
        pdecl = mgs.manager.decl
        ptype, pname = pdecl.type, pdecl.shortname

        mgsdecl = mgs.decl
        mgstype, mgsname = mgsdecl.type, mgsdecl.shortname

        loc = mgs.loc

        # we added this information; sanity check it
        for managed in ptype.manages:
            if managed is mgstype:
                break
        else:
            assert False

        # check that the "managed" protocol agrees
        if mgstype.manager is not ptype:
            self.error(
                loc,
                "|manages| declaration in protocol `%s' does not match any |manager| declaration in protocol `%s'",
                pname, mgsname)


    def visitManagerStmt(self, mgr):
        pdecl = mgr.of.decl
        ptype, pname = pdecl.type, pdecl.shortname

        mgrdecl = mgr.decl
        mgrtype, mgrname = mgrdecl.type, mgrdecl.shortname

        # we added this information; sanity check it
        assert ptype.manager is mgrtype

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

        if (mtype.isCtor() or mtype.isDtor()) and not ptype.isManagerOf(mtype.constructedType()):
            self.error(
                loc,
                "ctor/dtor for protocol `%s', which is not managed by protocol `%s'", 
                mname[:-len('constructor')], pname)


    def visitTransition(self, t):
        _YNC = [ ASYNC, SYNC ]

        loc = t.loc
        impliedDirection, impliedSems = {
            SEND: [ OUT, _YNC ], RECV: [ IN, _YNC ],
            CALL: [ OUT, RPC ],  ANSWER: [ IN, RPC ]
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

def unique_pairs(s):
    n = len(s)
    for i, e1 in enumerate(s):
        for j in xrange(i+1, n):
            yield (e1, s[j])

def cartesian_product(s1, s2):
    for e1 in s1:
        for e2 in s2:
            yield (e1, e2)


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
        # can occur when a protocol state machine has triggers of
        # opposite direction from the same state.  That means that,
        # e.g., the parent could send the child a message at the exact
        # instance the child sends the parent a message.  One of those
        # messages would (probably) violate the state machine and
        # cause the child to be terminated.  It's obviously very nice
        # if we can forbid this at the level of IPDL state machines,
        # rather than resorting to static or dynamic checking of C++
        # implementation code.
        #
        # An easy way to avoid this problem in IPDL is to only allow
        # "unidirectional" protocol states; that is, from each state,
        # only send or only recv triggers are allowed.  This approach
        # is taken by the Singularity project's "contract-based
        # message channels."  However, this is a bit of a notational
        # burden.
        #
        # IPDL's solution is to allow allow the IPDL programmer to
        # define "commutative transitions," that is, pairs of
        # transitions (A, B) that can happen in either order: first A
        # then B, or first B then A.  So instead of checking state
        # unidirectionality, we instead do the following two checks.
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
        #         T2 allows the trigger t1, transitioning to state U
        #         T1 allows the trigger t2, transitioning to state U
        #
        # This is a more formal way of expressing "it doesn't matter
        # in which order the triggers t1 and t2 occur / are processed."
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
        #           T2 allows the trigger t1, with out-state set { U }
        #           T1 allows the trigger t2, with out-state set { U }
        #
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

        def triggerTargets(S, t):
            '''Return the set of states transitioned to from state |S|
upon trigger |t|, or { } if |t| is not a trigger in |S|.'''
            for trans in self.p.states[S].transitions:
                if t.trigger is trans.trigger and t.msg is trans.msg:
                    return trans.toStates
            return set()


        for (t1, t2) in unique_pairs(ts.transitions):
            # if the triggers have the same direction, they can't race,
            # since only one endpoint can initiate either (and delivery
            # is in-order)
            if t1.trigger.direction() == t2.trigger.direction():
                continue

            t1_out = t1.toStates
            t2_out = t2.toStates

            for (T1, T2) in cartesian_product(t1_out, t2_out):
                U1 = triggerTargets(T1, t2)
                U2 = triggerTargets(T2, t1)

                if (0 == len(U1)
                    or 1 < len(U1) or 1 < len(U2)
                    or U1 != U2):
                    self.error(
                        t2.loc,
                        "in protocol `%s' state `%s', trigger `%s' potentially races (does not commute) with `%s'",
                        self.p.name, ts.state.name,
                        t1.msg.progname, t2.msg.progname)
                    # don't report more than one Diamond Rule
                    # violation per state. there may be O(n^4)
                    # total, way too many for a human to parse
                    #
                    # XXX/cjones: could set a limit on #printed
                    # and stop after that limit ...
                    return

    def checkReachability(self, p):
        visited = set()         # set(State)
        def explore(ts):
            if ts.state in visited:
                return
            visited.add(ts.state)
            for outedge in ts.transitions:
                for toState in outedge.toStates:
                    explore(p.states[toState])

        for root in p.startStates:
            explore(root)
        for ts in p.transitionStmts:
            if ts.state not in visited:
                self.error(ts.loc, "unreachable state `%s' in protocol `%s'",
                           ts.state.name, p.name)
