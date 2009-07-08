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

import sys

from ipdl.ast import CxxInclude, Decl, Loc, QualifiedId, TypeSpec, UsingStmt, Visitor, ASYNC, SYNC, RPC, IN, OUT, INOUT, ANSWER, CALL, RECV, SEND
import ipdl.builtin as builtin

class Type:
    # Is this a C++ type?
    def isCxx():
        return False
    # Is this an IPDL type?
    def isIPDL():
        return False
    # Can this type appear in IPDL programs?
    def isVisible():
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
    def __init__(self): pass
    def isState(self): return True

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

##--------------------
def errormsg(loc, fmt, *args):
    while not isinstance(loc, Loc):
        if loc is None:  loc = Loc.NONE
        else:            loc = loc.loc
    return '%s: error: %s'% (str(loc), fmt % args)


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


class TypeCheck(Visitor):
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
        self.symtab = SymbolTable(self.errors)

    def check(self, tu, errout=sys.stderr):
        tu.cxxIncludes = builtinIncludes + tu.cxxIncludes

        # tag each relevant node with "decl" information, giving type, name,
        # and location of declaration
        tu.accept(GatherDecls(builtinUsing, self.symtab, self.errors))
        if len(self.errors):
            # no point in checking types if we couldn't even resolve symbols
            self.reportErrors(errout)
            return False

        # now that the nodes have decls, type checking is much easier.
        tu.accept(CheckTypes(self.symtab, self.errors))
        if (len(self.errors)):
            # no point in later passes if type checking fails
            self.reportErrors(errout)
            return False

        return True

    def reportErrors(self, errout):
        for error in self.errors:
            print >>errout, error


class GatherDecls(Visitor):
    def __init__(self, builtinUsing, symtab, errors):
        self.builtinUsing = builtinUsing
        self.symtab = symtab
        self.errors = errors
        self.depth = 0

    def visitTranslationUnit(self, tu):
        # all TranslationUnits declare symbols in global scope
        if hasattr(tu, '_tchecked') and tu._tchecked:
            return
        tu._tchecked = True
        self.depth += 1

        # bit of a hack here --- we want the builtin |using|
        # statements to be added to the symbol table before anything
        # else, but we also want them in the translation units' list
        # of using stmts so that we can use them later down the pipe.
        # so we add them to the symbol table before anything else, and
        # prepend them to the TUs after visiting all their |using|
        # decls
        if 1 == self.depth:
            for using in self.builtinUsing:
                udecl = Decl(using.loc)
                udecl.shortname = using.type.basename()
                fullname = str(using.type)
                if udecl.shortname != fullname:
                    udecl.fullname = fullname
                udecl.type = BuiltinCxxType(using.type.spec)
                self.symtab.declare(udecl)
                using.decl = udecl

        p = tu.protocol

        # FIXME/cjones: it's a little weird and counterintuitive to put
        # both the namespace and non-namespaced name in the global scope.
        # try to figure out something better; maybe a type-neutral |using|
        # that works for C++ and protocol types?
        pdecl = Decl(p.loc)
        pdecl.shortname = p.name

        fullname = QualifiedId(p.loc, p.name,
                               [ ns.namespace for ns in p.namespaces ])
        if len(fullname.quals):
            pdecl.fullname = str(fullname)

        pdecl.type = ProtocolType(fullname, p.sendSemantics)
        pdecl.body = p
        self.symtab.declare(pdecl)

        p.decl = pdecl
        p.type = pdecl.type

        # make sure we have decls for all dependent protocols
        for pinc in tu.protocolIncludes:
            pinc.accept(self)

        # each protocol has its own scope for types brought in through |using|
        self.symtab.enterScope(tu)

        # and for all imported C++ types
        for using in tu.using:
            using.accept(self)

        # (see long comment above)
        tu.using = self.builtinUsing + tu.using

        # grab symbols in the protocol itself
        p.accept(self)

        self.symtab.exitScope(tu)

        tu.type = VOID
        self.depth -= 1

    def visitProtocolInclude(self, pi):
        pi.tu.accept(self)

    def visitUsingStmt(self, using):
        decl = Decl(using.loc)
        decl.shortname = using.type.basename()
        fullname = str(using.type)
        if decl.shortname != fullname:
            decl.fullname = fullname
        decl.type = ImportedCxxType(using.type.spec)
        self.symtab.declare(decl)
        using.decl = decl


    def visitProtocol(self, p):
        # protocol scope
        self.symtab.enterScope(p)

        if p.manager is not None:
            p.manager.of = p
            p.manager.accept(self)

        for managed in p.managesStmts:
            managed.manager = p
            managed.accept(self)

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
                self.errors.append(
                    errormsg(
                        managed.loc,
                        "constructor and destructor declarations are required for managed protocol `%s' (managed by protocol `%s')",
                        mgdname, p.name))

        # declare each state before decorating their mention
        for trans in p.transitionStmts:
            sdecl = Decl(trans.state.loc)
            sdecl.progname = trans.state.name
            sdecl.type = StateType()

            self.symtab.declare(sdecl)
            trans.state.decl = sdecl

        for trans in p.transitionStmts:
            trans.accept(self)

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
            self.errors.append(
                errmsg(
                    loc,
                    "protocol `%s' referenced as |manager| of `%s' has not been declared",
                    mgrname, pname))
        elif not isinstance(mgrdecl.type, ProtocolType):
            self.errors.append(
                errmsg(
                    loc,
                    "entity `%s' referenced as |manager| of `%s' is not of `protocol' type; instead it is of type `%s'",
                    mgrname, pname, mgrdecl.type.typename()))
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
            self.errors.append(
                errormsg(
                    loc,
                    "protocol `%s', managed by `%s', has not been declared",
                    mgsname, pdeclname))
        elif not isinstance(mgsdecl.type, ProtocolType):
            self.errors.append(
                errormsg(
                    loc,
                    "%s declares itself managing a non-`protocol' entity `%s' of type `%s'",
                    pdeclname, mgsname, mgsdecl.type.typename()))
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
                self.errors.append(
                    errormsg(
                        loc,
                        "type `%s' has not been declared",
                        msgname))
            elif not decl.type.isProtocol():
                self.errors.append(
                    errormsg(
                        loc,
                        "destructor for non-protocol type `%s'",
                        msgname))
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
            self.errors.append(
                errormsg(
                    loc,
                    "message name `%s' already declared as `%s'",
                    msgname, decl.type.typename()))
            # if we error here, no big deal; move on to find more
        decl = None

        # enter message scope
        self.symtab.enterScope(md)

        msgtype = MessageType(md.sendSemantics, md.direction,
                              ctor=isctor, dtor=isdtor, cdtype=cdtype)

        # replace inparam Param nodes with proper Decls
        for i, inparam in enumerate(md.inParams):
            inptname = inparam.typespec.basename()
            inploc = inparam.typespec.loc

            inptdecl = self.symtab.lookup(inptname)

            if inptdecl is None:
                self.errors.append(
                    errormsg(
                        inploc,
                        "inparam typename `%s' of message `%s' has not been declared",
                        inptname, msgname))
            else:
                inpdecl = Decl(inploc)
                inpdecl.progname = inparam.name
                inpdecl.type = inptdecl.type

                self.symtab.declare(inpdecl)

                msgtype.params.append(inpdecl.type)
                md.inParams[i] = inpdecl

        # replace outparam Param with proper Decls
        for i, outparam in enumerate(md.outParams):
            outptname = outparam.typespec.basename()
            outploc = outparam.typespec.loc

            outptdecl = self.symtab.lookup(outptname)

            if outptdecl is None:
                self.errors.append(
                    errormsg(
                        outploc,
                        "outparam typename `%s' of message `%s' has not been declared",
                        outptname, msgname))
            else:
                outpdecl = Decl(outploc)
                outpdecl.progname = outparam.name
                outpdecl.type = outptdecl.type

                self.symtab.declare(outpdecl)

                msgtype.returns.append(outpdecl.type)
                md.outParams[i] = outpdecl

        self.symtab.exitScope(md)

        decl = Decl(loc)
        decl.progname = msgname
        decl.type = msgtype

        self.symtab.declare(decl)
        md.decl = decl
        md.protocolDecl = self.currentProtocolDecl


    def visitTransition(self, t):
        loc = t.loc

        sname = t.toState.name
        sdecl = self.symtab.lookup(sname)
        if sdecl is None:
            self.errors.append(
                errormsg(loc, "state `%s' has not been declared", sname))
        elif not sdecl.type.isState():
            self.errors.append(
                errormsg(
                    loc,
                    "`%s' should have state type, but instead has type `%s'",
                    sname, sdecl.type.typename()))
        else:
            t.toState.decl = sdecl

        mname = t.msg
        mdecl = self.symtab.lookup(mname)
        if mdecl is None:
            self.errors.append(
                errormsg(loc, "message `%s' has not been declared", mname))
        elif not mdecl.type.isMessage():
            self.errors.append(
                errormsg(
                    loc,
                    "`%s' should have message type, but instead has type `%s'",
                    mname, mdecl.type.typename()))
        else:
            t.msg = mdecl


class CheckTypes(Visitor):
    def __init__(self, symtab, errors):
        self.symtab = symtab
        self.errors = errors
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
            self.errors.append(errormsg(
                    p.decl.loc,
                    "protocol `%s' requires more powerful send semantics than its manager `%s' provides",
                    pname,
                    mgrname))

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
            self.errors.append(errormsg(
                    loc,
                    "|manages| declaration in protocol `%s' does not match any |manager| declaration in protocol `%s'",
                    pname, mgsname))


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
            self.errors.append(errormsg(
                    loc,
                    "|manager| declaration in protocol `%s' does not match any |manages| declaration in protocol `%s'",
                    pname, mgrname))


    def visitMessageDecl(self, md):
        mtype, mname = md.decl.type, md.decl.progname
        ptype, pname = md.protocolDecl.type, md.protocolDecl.shortname

        loc = md.decl.loc

        if mtype.needsMoreJuiceThan(ptype):
            self.errors.append(errormsg(
                    loc,
                    "message `%s' requires more powerful send semantics than its protocol `%s' provides",
                    mname,
                    pname))

        if mtype.isAsync() and len(mtype.returns):
            # XXX/cjones could modify grammar to disallow this ...
            self.errors.append(errormsg(
                    loc,
                    "asynchronous message `%s' requests returned values",
                    mname))

        if (mtype.isCtor() or mtype.isDtor()) and not ptype.isManagerOf(mtype.constructedType()):
            self.errors.append(errormsg(
                    loc,
                    "ctor/dtor for protocol `%s', which is not managed by protocol `%s'", 
                    mname[:-len('constructor')],
                    pname))


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

            self.errors.append(errormsg(
                    loc, "%s %s message `%s' is not `%s'd",
                    mtype.sendSemantics.pretty, mtype.direction.pretty,
                    t.msg.progname,
                    trigger))
