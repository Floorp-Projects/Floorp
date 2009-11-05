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

import os, re, sys
from copy import deepcopy

import ipdl.ast
from ipdl.cxx.ast import *
from ipdl.type import TypeVisitor

# FIXME/cjones: the chromium Message logging code doesn't work on
# gcc/POSIX, because it wprintf()s across the chromium/mozilla
# boundary. one side builds with -fshort-wchar, the other doesn't.
# this code will remain off until the chromium base lib is replaced
EMIT_LOGGING_CODE = ('win32' == sys.platform)

##-----------------------------------------------------------------------------
## "Public" interface to lowering
##
class LowerToCxx:
    def lower(self, tu):
        '''returns a list of File representing the lowered form of |tu|'''
        # annotate the AST with IPDL/C++ IR-type stuff used later
        tu.accept(_DecorateWithCxxStuff())

        pname = tu.protocol.name

        pheader = File(pname +'.h')
        _GenerateProtocolHeader().lower(tu, pheader)

        parentheader = File(pname +'Parent.h')
        _GenerateProtocolParentHeader().lower(tu, pname+'Parent', parentheader)

        childheader = File(pname +'Child.h')
        _GenerateProtocolChildHeader().lower(tu, pname+'Child', childheader)

        return pheader, parentheader, childheader


##-----------------------------------------------------------------------------
## Helper code
##
class _struct: pass

def _protocolHeaderBaseFilename(p):
    pfx = '/'.join([ ns.name for ns in p.namespaces ])
    if pfx: return pfx +'/'+ p.name
    else:   return p.name

def _includeGuardMacroName(headerfile):
    return re.sub(r'[./]', '_', headerfile.name)

def _includeGuardStart(headerfile):
    guard = _includeGuardMacroName(headerfile)
    return [ CppDirective('ifndef', guard),
             CppDirective('define', guard)  ]

def _includeGuardEnd(headerfile):
    guard = _includeGuardMacroName(headerfile)
    return [ CppDirective('endif', '// ifndef '+ guard) ]

def _actorName(pname, side):
    """|pname| is the protocol name. |side| is 'Parent' or 'Child'."""
    tag = side
    if not tag[0].isupper():  tag = side.title()
    return pname + tag

def _actorIdType():
    return Type('int32')

def _safeActorId(actor):
    return ExprConditional(actor, _actorId(actor), ExprLiteral.ZERO)

def _actorId(actor):
    return ExprSelect(actor, '->', 'mId')

def _actorHId(actorhandle):
    return ExprSelect(actorhandle, '.', 'mId')

def _actorChannel(actor):
    return ExprSelect(actor, '->', 'mChannel')

def _actorManager(actor):
    return ExprSelect(actor, '->', 'mManager')

def _safeLookupActor(idexpr, actortype):
    return ExprCast(
        ExprConditional(
            ExprBinary(idexpr, '==', ExprLiteral.ZERO),
            ExprLiteral.NULL,
            _lookupListener(idexpr)),
        actortype,
        reinterpret=1)

def _safeLookupActorHandle(handle, actortype):
    return _safeLookupActor(_actorHId(handle), actortype)

def _lookupActor(idexpr, actortype):
    return ExprCast(idexpr, actortype, reinterpret=1)

def _lookupActorHandle(handle, actortype):
    return _lookupActor(_actorHId(handle), actortype)

def _lookupListener(idexpr):
    return ExprCall(ExprVar('Lookup'), args=[ idexpr ])

def _makeForwardDecl(ptype, side):
    clsname = _actorName(ptype.qname.baseid, side)

    fd = ForwardDecl(clsname, cls=1)
    if 0 == len(ptype.qname.quals):
        return fd

    outerns = Namespace(ptype.qname.quals[0])
    innerns = outerns
    for ns in ptype.qname.quals[1:]:
        tmpns = Namespace(ns)
        innerns.addstmt(tmpns)
        innerns = tmpns

    innerns.addstmt(fd)
    return outerns

def _putInNamespaces(cxxthing, namespaces):
    """|namespaces| is in order [ outer, ..., inner ]"""
    if 0 == len(namespaces):  return cxxthing

    outerns = Namespace(namespaces[0].name)
    innerns = outerns
    for ns in namespaces[1:]:
        newns = Namespace(ns.name)
        innerns.addstmt(newns)
        innerns = newns
    innerns.addstmt(cxxthing)
    return outerns

def _sendPrefix(msgtype):
    """Prefix of the name of the C++ method that sends |msgtype|."""
    if msgtype.isRpc():
        return 'Call'
    return 'Send'

def _recvPrefix(msgtype):
    """Prefix of the name of the C++ method that handles |msgtype|."""
    if msgtype.isRpc():
        return 'Answer'
    return 'Recv'

def _flatTypeName(ipdltype):
    """Return a 'flattened' IPDL type name that can be used as an
identifier.
E.g., |Foo[]| --> |ArrayOfFoo|."""
    # NB: this logic depends heavily on what IPDL types are allowed to
    # be constructed; e.g., Foo[][] is disallowed.  needs to be kept in
    # sync with grammar.
    if ipdltype.isIPDL() and ipdltype.isArray():
        return 'ArrayOf'+ ipdltype.basetype.name()
    return ipdltype.name()


def _hasVisibleActor(ipdltype):
    """Return true iff a C++ decl of |ipdltype| would have an Actor* type.
For example: |Actor[]| would turn into |nsTArray<ActorParent*>|, so this
function would return true for |Actor[]|."""
    return (ipdltype.isIPDL()
            and (ipdltype.isActor()
                 or (ipdltype.isArray()
                     and _hasVisibleActor(ipdltype.basetype))))

def _abortIfFalse(cond, msg):
    return StmtExpr(ExprCall(
        ExprVar('NS_ABORT_IF_FALSE'),
        [ cond, ExprLiteral.String(msg) ]))

def _runtimeAbort(msg):
    return StmtExpr(ExprCall(ExprVar('NS_RUNTIMEABORT'),
                                     [ ExprLiteral.String(msg) ]))

def _cxxArrayType(basetype):
    return Type('nsTArray', T=basetype)

def _callCxxArrayLength(arr):
    return ExprCall(ExprSelect(arr, '.', 'Length'))

def _callCxxArraySetLength(arr, lenexpr):
    return ExprCall(ExprSelect(arr, '.', 'SetLength'),
                    args=[ lenexpr ])

def _otherSide(side):
    if side == 'child':  return 'parent'
    if side == 'parent':  return 'child'
    assert 0

def _ifLogging(stmts):
    iflogging = StmtIf(ExprCall(ExprVar('mozilla::ipc::LoggingEnabled')))
    iflogging.addifstmts(stmts)
    return iflogging

##-----------------------------------------------------------------------------
## Intermediate representation (IR) nodes used during lowering

class _ConvertToCxxType(TypeVisitor):
    def __init__(self, side):  self.side = side
    
    def visitBuiltinCxxType(self, t):
        return Type(t.name())

    def visitImportedCxxType(self, t):
        return Type(t.name())

    def visitActorType(self, a):
        return Type(_actorName(a.protocol.name(), self.side), ptr=1)

    def visitUnionType(self, u):
        return Type(u.name())

    def visitArrayType(self, a):
        basecxxtype = a.basetype.accept(self)
        return _cxxArrayType(basecxxtype)

    def visitProtocolType(self, p): assert 0
    def visitMessageType(self, m): assert 0
    def visitVoidType(self, v): assert 0
    def visitStateType(self, st): assert 0


class _ConvertToSerializableCxxType(TypeVisitor):
    def visitBuiltinCxxType(self, t):
        return Type(t.name())

    def visitImportedCxxType(self, t):
        return Type(t.name())

    def visitActorType(self, a):
        return Type('ActorHandle')

    def visitUnionType(self, u):
        return Type(u.name())

    def visitArrayType(self, a):
        basecxxtype = a.basetype.accept(self)
        return _cxxArrayType(basecxxtype)

    def visitProtocolType(self, p): assert 0
    def visitMessageType(self, m): assert 0
    def visitVoidType(self, v): assert 0
    def visitStateType(self): assert 0

##
## A _HybridDecl straddles IPDL and C++ decls.  It knows which C++
## types correspond to which IPDL types, and it also knows how
## serialize and deserialize "special" IPDL C++ types.
##
## NB: the current serialization/deserialization strategy is rather
## simplistic.  We take the values to be serialized, repack them into
## "safe" values, and then pass those values to the appropriate
## Msg_* constructor.  This can potentially result in a lot of
## unnecessary value copying and temporary variables.
##
## If this becomes a performance problem, this code should be modified
## to use a "streaming" model, in which code is generated to directly
## write serialized values into the Msg_*'s payload.
##
class _HybridDecl:
    """A hybrid decl stores both an IPDL type and all the C++ type
info needed by later passes, along with a basic name for the decl."""
    def __init__(self, ipdltype, name):
        self.ipdltype = ipdltype
        self.name = name
        self.idnum = 0

    def var(self):
        return ExprVar(self.name)

    def bareType(self, side):
        """Return this decl's unqualified C++ type."""
        return self.ipdltype.accept(_ConvertToCxxType(side))

    def refType(self, side):
        """Return this decl's C++ type as a 'reference' type, which is not
necessarily a C++ reference."""
        t = self.bareType(side)
        t.ref = 1
        return t

    def constRefType(self, side):
        """Return this decl's C++ type as a const, 'reference' type."""
        t = self.bareType(side)
        if self.ipdltype.isIPDL() and self.ipdltype.isActor():
            t.const = 1                 # const Actor*
            return t
        t.const = 1
        t.ref = 1
        return t

    def ptrToType(self, side):
        t = self.bareType(side)
        if self.ipdltype.isIPDL() and self.ipdltype.isActor():
            t.ptr = 0
            t.ptrptr = 1
            return t
        t.ptr = 1
        return t

    def constPtrToType(self, side):
        t = self.bareType(side)
        if self.ipdltype.isIPDL() and self.ipdltype.isActor():
            t.ptr = 0
            t.const = 1
            t.ptrconstptr = 1           # const Actor* const*
            return t
        t.const = 1
        t.ptrconst = 1
        return t

    def inType(self, side):
        """Return this decl's C++ Type with inparam semantics."""
        if self.ipdltype.isIPDL() and self.ipdltype.isActor():
            return self.bareType(side)
        return self.constRefType(side)

    def outType(self, side):
        """Return this decl's C++ Type with outparam semantics."""
        t = self.bareType(side)
        if self.ipdltype.isIPDL() and self.ipdltype.isActor():
            t.ptr = 0;  t.ptrptr = 1
            return t
        t.ptr = 1
        return t

    def barePipeType(self):
        """Return this decl's C++ serializable Type."""
        return self.ipdltype.accept(_ConvertToSerializableCxxType())

    def inPipeType(self):
        """Return this decl's serializable C++ type with inparam semantics"""
        t = self.barePipeType()
        t.const = 1
        t.ref = 1
        return t

    def outPipeType(self):
        """Return this decl's serializable C++ type with outparam semantics"""
        t = self.barePipeType()
        t.ptr = 1
        return t

    # the biggies: serialization/deserialization

    def serialize(self, expr, side):
        if not ipdl.type.hasactor(self.ipdltype):
            return expr, [ ]
        # XXX could use TypeVisitor, but it doesn't feel right here
        _, sexpr, stmts = self._serialize(self.ipdltype, expr, side)
        return sexpr, stmts

    def _serialize(self, etype, expr, side):
        '''Serialize |expr| of type |etype|, which has some actor type
buried in it.  Return |pipetype, serializedExpr, serializationStmts|.'''
        assert etype.isIPDL()           # only IPDL types may contain actors

        if etype.isActor():
            return self._serializeActor(etype, expr)
        elif etype.isArray():
            return self._serializeArray(etype, expr, side)
        elif etype.isUnion():
            return self._serializeUnion(etype, expr, side)
        else: assert 0


    def _serializeActor(self, actortype, expr):
        actorhandlevar = ExprVar(self._nextuid('handle'))
        pipetype = Type('ActorHandle')
        # TODO nullability
        stmts = [
            Whitespace('// serializing actor type\n', indent=1),
            StmtDecl(Decl(pipetype, actorhandlevar.name)),
            StmtExpr(ExprAssn(
                _actorHId(actorhandlevar),
                ExprConditional(ExprNot(expr),
                                ExprLiteral.NULL, _actorId(expr)))),
            Whitespace.NL
        ]
        return pipetype, actorhandlevar, stmts


    def _serializeArray(self, arraytype, expr, side):
        newarrayvar = ExprVar(self._nextuid('serArray'))
        lenvar = ExprVar(self._nextuid('length'))
        ivar = ExprVar(self._nextuid('i'))

        # FIXME hacky init of |i|
        forloop = StmtFor(init=ExprAssn(Decl(Type.UINT32, ivar.name),
                                        ExprLiteral.ZERO),
                          cond=ExprBinary(ivar, '<', lenvar),
                          update=ExprPrefixUnop(ivar, '++'))

        ithNewElt = ExprIndex(newarrayvar, ivar)
        ithOldElt = ExprIndex(expr, ivar)

        eltType, serializedExpr, forbodystmts = self._serialize(
            arraytype.basetype, ithOldElt, side)

        forloop.addstmts(forbodystmts)
        forloop.addstmt(StmtExpr(ExprAssn(ithNewElt, serializedExpr)))

        pipetype = _cxxArrayType(eltType)
        stmts = [
            Whitespace('// serializing array type\n', indent=1),
            StmtDecl(Decl(Type.UINT32, lenvar.name),
                     init=_callCxxArrayLength(expr)),
            StmtDecl(Decl(pipetype, newarrayvar.name)),
            StmtExpr(_callCxxArraySetLength(newarrayvar, lenvar)),
            forloop,
            Whitespace.NL
        ]
        return pipetype, newarrayvar, stmts


    def _serializeUnion(self, uniontype, expr, side):
        def insaneActorCast(actor, actortype):
            return ExprCast(
                ExprCast(_safeActorId(actor), Type.INTPTR, static=1),
                actortype,
                reinterpret=1)

        pipetype = Type(uniontype.name())
        serunionvar = ExprVar(self._nextuid('serUnion'))
        ud = uniontype._ud

        switch = StmtSwitch(ud.callType(expr))
        for c in ud.components:
            ct = c.ipdltype
            if not ipdl.type.hasactor(ct):
                continue
            assert ct.isIPDL()

            # we special-case two types here: actor's and actor[]'s.  these
            # get repacked into the out-array.  otherwise we recurse
            case = StmtBlock()
            getvalue = ExprCall(ExprSelect(expr, '.', c.getConstTypeName()))

            if ct.isActor():
                if c.side != side:
                    case.addstmt(_runtimeAbort('wrong side!'))
                else:
                    # may god have mercy on our souls
                    case.addstmt(StmtExpr(ExprAssn(
                        serunionvar,
                        insaneActorCast(getvalue, c.bareType()))))
            elif ct.isArray() and ct.basetype.isActor():
                if c.side != side:
                    case.addstmt(_runtimeAbort('wrong side!'))
                else:
                    # no more apologies
                    actortype = ct.basetype.accept(_ConvertToCxxType(c.side))
                    lenvar = ExprVar(self._nextuid('len'))
                    newarrvar = ExprVar(self._nextuid('idarray'))

                    ivar = ExprVar(self._nextuid('i'))
                    ithOldElt = ExprIndex(getvalue, ivar)
                    ithNewElt = ExprIndex(newarrvar, ivar)
                    loop = StmtFor(init=ExprAssn(Decl(Type.UINT32, ivar.name),
                                          ExprLiteral.ZERO),
                                   cond=ExprBinary(ivar, '<', lenvar),
                                   update=ExprPrefixUnop(ivar, '++'))
                    loop.addstmt(StmtExpr(ExprAssn(
                        ithNewElt,
                        insaneActorCast(ithOldElt, actortype))))

                    case.addstmts([
                        StmtDecl(Decl(Type.UINT32, lenvar.name),
                                 init=_callCxxArrayLength(getvalue)),
                        StmtDecl(Decl(c.bareType(), newarrvar.name)),
                        StmtExpr(_callCxxArraySetLength(newarrvar, lenvar)),
                        Whitespace.NL,
                        loop,
                        StmtExpr(ExprAssn(serunionvar, newarrvar))
                    ])
            else:
                # NB: here we rely on the serialized expression
                # coming back with the same type
                _, newexpr, sstmts = self._serialize(ct, getvalue, side)
                case.addstmts(sstmts
                              + [ Whitespace.NL,
                                  StmtExpr(ExprAssn(serunionvar, newexpr)) ])

            case.addstmt(StmtBreak())
            switch.addcase(CaseLabel(c.pqEnum()), case)

        switch.addcase(
            DefaultLabel(),
            StmtBlock([ StmtExpr(ExprAssn(serunionvar, expr)),
                        StmtBreak() ]))

        stmts = [
            Whitespace('// serializing union type\n', indent=1),
            StmtDecl(Decl(pipetype, serunionvar.name)),
            Whitespace.NL,
            switch
        ]
        return pipetype, serunionvar, stmts


    def makePipeDecls(self, toExpr):
        if not ipdl.type.hasactor(self.ipdltype):
            return 0, toExpr, [ ]
        tempvar = ExprVar(self._nextuid('deTemp'))
        return (1,
                tempvar,
                [ StmtDecl(Decl(self.barePipeType(), tempvar.name)) ])

    def makeDeserializedDecls(self, side):
        return self.var(), [ StmtDecl(Decl(self.bareType(side),
                                           self.var().name)) ]

    def deserialize(self, expr, side, sems):
        """|expr| is a pointer the return type."""
        if not ipdl.type.hasactor(self.ipdltype):
            return [ ]
        if sems == 'in':
            toexpr = self.var()
        elif sems == 'out':
            toexpr = ExprDeref(self.var())
        else: assert 0
        _, stmts = self._deserialize(expr, self.ipdltype, toexpr, side)
        return stmts

    def _deserialize(self, pipeExpr, targetType, targetExpr, side):
        if not ipdl.type.hasactor(targetType):
            return targetType, [ ]
        elif targetType.isActor():
            return self._deserializeActor(
                pipeExpr, targetType, targetExpr, side)
        elif targetType.isArray():
            return self._deserializeArray(
                pipeExpr, targetType, targetExpr, side)
        elif targetType.isUnion():
            return self._deserializeUnion(
                pipeExpr, targetType, targetExpr, side)
        else: assert 0

    def _deserializeActor(self, actorhandle, actortype, outactor, side):
        cxxtype = actortype.accept(_ConvertToCxxType(side))
        return (
            cxxtype,
            [
                Whitespace('// deserializing actor type\n', indent=1),
                StmtExpr(ExprAssn(
                    outactor,
                    _safeLookupActorHandle(actorhandle, cxxtype)))
            ])


    def _deserializeArray(self, pipearray, arraytype, outarray, side):
        cxxArrayType = arraytype.accept(_ConvertToCxxType(side))
        lenvar = ExprVar(self._nextuid('length'))

        stmts = [
            Whitespace('// deserializing array type\n', indent=1),
            StmtDecl(Decl(Type.UINT32, lenvar.name),
                     _callCxxArrayLength(pipearray)),
            StmtExpr(ExprCall(ExprSelect(outarray, '.', 'SetLength'),
                              args=[ lenvar ])),
            Whitespace.NL
        ]
        ivar = ExprVar(self._nextuid('i'))
        forloop = StmtFor(init=ExprAssn(Decl(Type.UINT32, ivar.name),
                                        ExprLiteral.ZERO),
                          cond=ExprBinary(ivar, '<', lenvar),
                          update=ExprPrefixUnop(ivar, '++'))       
        ithNewElt = ExprIndex(outarray, ivar)
        ithOldElt = ExprIndex(pipearray, ivar)

        outelttype, forstmts = self._deserialize(
            ithOldElt, arraytype.basetype, ithNewElt, side)
        forloop.addstmts(forstmts)

        stmts.append(forloop)

        return cxxArrayType, stmts


    def _deserializeUnion(self, pipeunion, uniontype, outunion, side):
        def actorIdCast(expr):
            return ExprCast(
                ExprCast(expr, Type.INTPTR, reinterpret=1),
                _actorIdType(),
                static=1)

        cxxUnionType = Type(uniontype.name())
        ud = uniontype._ud

        switch = StmtSwitch(ud.callType(pipeunion))
        for c in ud.components:
            ct = c.ipdltype
            if not ipdl.type.hasactor(ct):
                continue
            assert ct.isIPDL()

            # like in _serializeUnion, we special-case actor's and
            # actor[]'s.  we look up the actors that correspond to the
            # actor ID's we, sigh, packed into the actor pointers
            case = StmtBlock()
            getvalue = ExprCall(ExprSelect(pipeunion, '.', c.getTypeName()))

            if ct.isActor():
                # ParamTraits<union>::Read() magically flips the side on
                # our behalf
                if c.side != side:
                    case.addstmt(_runtimeAbort('wrong side!'))
                else:
                    idvar = ExprVar(self._nextuid('id'))
                    case.addstmts([
                        StmtDecl(Decl(_actorIdType(), idvar.name),
                                 actorIdCast(getvalue)),
                        StmtExpr(ExprAssn(
                            outunion,
                            _safeLookupActor(idvar, c.bareType())))
                    ])
            elif ct.isArray() and ct.basetype.isActor():
                if c.side != side:
                    case.addstmt(_runtimeAbort('wrong side!'))
                else:
                    idvar = ExprVar(self._nextuid('id'))
                    arrvar = ExprVar(self._nextuid('arr'))
                    ivar = ExprVar(self._nextuid('i'))
                    ithElt = ExprIndex(arrvar, ivar)

                    loop = StmtFor(
                        init=ExprAssn(Decl(Type.UINT32, ivar.name),
                                      ExprLiteral.ZERO),
                        cond=ExprBinary(ivar, '<',
                                        _callCxxArrayLength(arrvar)),
                        update=ExprPrefixUnop(ivar, '++'))
                    loop.addstmts([
                        StmtDecl(Decl(_actorIdType(), idvar.name),
                                 actorIdCast(ithElt)),
                        StmtExpr(ExprAssn(
                            ithElt,
                            _safeLookupActor(
                                idvar,
                                ct.basetype.accept(_ConvertToCxxType(side)))))
                    ])
                
                    case.addstmts([
                        StmtDecl(Decl(c.refType(), arrvar.name),
                                 getvalue),
                        loop,
                        StmtExpr(ExprAssn(outunion, arrvar))
                    ])
            else:
                tempvar = ExprVar('tempUnionElt')
                elttype, dstmts = self._deserialize(
                    getvalue, ct, tempvar, side)
                case.addstmts(
                    [ StmtDecl(Decl(elttype, tempvar.name)),
                      Whitespace.NL ]
                    + dstmts
                    + [ Whitespace.NL,
                        StmtExpr(ExprAssn(outunion, tempvar)) ])

            case.addstmt(StmtBreak())
            switch.addcase(CaseLabel(c.pqEnum()), case)

        switch.addcase(
            DefaultLabel(),
            StmtBlock([ StmtExpr(ExprAssn(outunion, pipeunion)),
                        StmtBreak() ]))
        
        stmts = [
            Whitespace('// deserializing union type\n', indent=1),
            switch
        ]

        return cxxUnionType, stmts


    def _nextuid(self, descr):
        """Return an identifier that's unique wrt to |self| and |self.name|."""
        self.idnum += 1
        return '%s_%s_%d'% (self.name, descr, self.idnum)

##--------------------------------------------------

class UnionDecl(ipdl.ast.UnionDecl):
    def fqClassName(self):
        return self.decl.type.fullname()

    def callType(self, var=None):
        func = ExprVar('type')
        if var is not None:
            func = ExprSelect(var, '.', func.name)
        return ExprCall(func)

    @staticmethod
    def upgrade(unionDecl):
        assert isinstance(unionDecl, ipdl.ast.UnionDecl)
        unionDecl.__class__ = UnionDecl
        return unionDecl


class _UnionMember(_HybridDecl):
    """Not in the AFL sense, but rather a member (e.g. |int;|) of an
IPDL union type."""
    def __init__(self, ipdltype, ud, side=None):
        flatname = _flatTypeName(ipdltype)
        special = _hasVisibleActor(ipdltype)
        if special:
            flatname += side.title()

        _HybridDecl.__init__(self, ipdltype, 'V'+ flatname)
        self.flattypename = flatname
        self.side = side
        self.special = special
        self.ud = ud

    def enum(self):
        return 'T' + self.flattypename

    def pqEnum(self):
        return self.ud.name +'::'+ self.enum()

    def enumvar(self):
        return ExprVar(self.enum())

    def unionType(self):
        """Type used for storage in generated C union decl."""
        return TypeArray(Type('char'), ExprSizeof(self.bareType()))

    def unionValue(self):
        # NB: knows that Union's storage C union is named |mValue|
        return ExprSelect(ExprVar('mValue'), '.', self.name)

    def typedef(self):
        return self.flattypename +'__tdef'

    def callGetConstPtr(self):
        """Return an expression of type self.constptrToSelfType()"""
        return ExprCall(ExprVar(self.getConstPtrName()))

    def callGetPtr(self):
        """Return an expression of type self.ptrToSelfType()"""
        return ExprCall(ExprVar(self.getPtrName()))

    def callOperatorEq(self, rhs):
        if self.ipdltype.isIPDL() and self.ipdltype.isActor():
            rhs = ExprCast(rhs, self.bareType(), const=1)
        return ExprAssn(ExprDeref(self.callGetPtr()), rhs)

    def callPlacementCtor(self, expr=None):
        assert not isinstance(expr, list)
        
        if expr is None:
            args = None
        elif self.ipdltype.isIPDL() and self.ipdltype.isActor():
            args = [ ExprCast(expr, self.bareType(), const=1) ]
        else:
            args = [ expr ]

        return ExprNew(self.bareType(self.side),
                     args=args,
                     newargs=[ self.callGetPtr() ])

    def callPlacementDtor(self):
        return ExprCall(
            ExprSelect(self.callGetPtr(), '->', '~'+ self.typedef()))

    def getTypeName(self): return 'get_'+ self.flattypename
    def getConstTypeName(self): return 'get_'+ self.flattypename

    def getPtrName(self): return 'ptr_'+ self.flattypename
    def getConstPtrName(self): return 'constptr_'+ self.flattypename

    def ptrToSelfExpr(self):
        """|*ptrToSelfExpr()| has type |self.bareType()|"""
        return ExprCast(ExprAddrOf(self.unionValue()),
                        self.ptrToType(),
                        reinterpret=1)

    def constptrToSelfExpr(self):
        """|*constptrToSelfExpr()| has type |self.constType()|"""
        return ExprCast(ExprAddrOf(self.unionValue()),
                        self.constPtrToType(),
                        reinterpret=1)

    # @override the following methods to pass |self.side| instead of
    # forcing the caller to remember which side we're declared to
    # represent.
    def bareType(self, side=None):
        return _HybridDecl.bareType(self, self.side)
    def refType(self, side=None):
        return _HybridDecl.refType(self, self.side)
    def constRefType(self, side=None):
        return _HybridDecl.constRefType(self, self.side)
    def ptrToType(self, side=None):
        return _HybridDecl.ptrToType(self, self.side)
    def constPtrToType(self, side=None):
        return _HybridDecl.constPtrToType(self, self.side)
    def inType(self, side=None):
        return _HybridDecl.inType(self, self.side)

    def otherSideBareType(self):
        assert self.ipdltype.isIPDL()
        otherside = _otherSide(self.side)
        it = self.ipdltype

        if it.isArray():
            assert it.basetype.isIPDL() and it.basetype.isActor()
            return _cxxArrayType(
                it.basetype.accept(_ConvertToCxxType(otherside)))
        elif it.isActor():
            return _HybridDecl.bareType(self, otherside)
        else: assert 0

##--------------------------------------------------

class MessageDecl(ipdl.ast.MessageDecl):
    def baseName(self):
        if self.decl.type.isDtor():
            return self.name[1:]
        return self.name
    
    def recvMethod(self):
        name = _recvPrefix(self.decl.type) + self.baseName()
        if self.decl.type.isCtor():
            name += 'Constructor'
        elif self.decl.type.isDtor():
            name += 'Destructor'
        return ExprVar(name)

    def sendMethod(self):
        name = _sendPrefix(self.decl.type) + self.baseName()
        if self.decl.type.isCtor():
            name += 'Constructor'
        elif self.decl.type.isDtor():
            name += 'Destructor'
        return ExprVar(name)

    def hasReply(self):
        return (self.decl.type.hasReply()
                or self.decl.type.isCtor()
                or self.decl.type.isDtor())

    # XXX best place for this?
    def allocMethod(self):
        return ExprVar('Alloc'+ self.baseName())

    def deallocMethod(self):
        return ExprVar('Dealloc'+ self.baseName())

    def msgClass(self):
        return 'Msg_%s'% (self.decl.progname)

    def pqMsgClass(self):
        return '%s::%s'% (self.namespace, self.msgClass())

    def msgCast(self, msgexpr):
        return ExprCast(msgexpr, Type(self.pqMsgClass(), const=1, ptr=1),
                        reinterpret=1)

    def msgId(self):  return self.msgClass()+ '__ID'
    def pqMsgId(self):
        return '%s::%s'% (self.namespace, self.msgId())

    def replyClass(self):
        return 'Reply_%s'% (self.decl.progname)

    def pqReplyClass(self):
        return '%s::%s'% (self.namespace, self.replyClass())

    def replyCast(self, replyexpr):
        return ExprCast(replyexpr, Type(self.pqReplyClass(), const=1, ptr=1),
                        reinterpret=1)

    def replyId(self):  return self.replyClass()+ '__ID'
    def pqReplyId(self):
        return '%s::%s'% (self.namespace, self.replyId())

    def actorDecl(self):
        return self.params[0]

    def makeCxxParams(self, paramsems='in', returnsems='out',
                      pipetypes=0, side=None, implicit=1):
        """Return a list of C++ decls per the spec'd configuration.
    |params| and |returns| is the C++ semantics of those: 'in', 'out', or None.
    |pipetypes| specifies whether to return serializable types."""

        def makeDecl(d, sems, pipetypes):
            if sems is 'in' and pipetypes:
                return Decl(d.inPipeType(), d.name)
            elif sems is 'in':
                return Decl(d.inType(side), d.name)
            elif sems is 'out' and pipetypes:
                return Decl(d.outPipeType(), d.name)
            elif sems is 'out':
                return Decl(d.outType(side), d.name)
            else: assert 0

        cxxparams = [ ]
        if paramsems is not None:
            cxxparams.extend(
                [ makeDecl(d, paramsems, pipetypes) for d in self.params ])

        if returnsems is not None:
            cxxparams.extend(
                [ makeDecl(r, returnsems, pipetypes) for r in self.returns ])

        if not implicit and self.decl.type.hasImplicitActorParam():
            cxxparams = cxxparams[1:]

        return cxxparams

    def makeCxxArgs(self, params=1, retsems='out', retcallsems='out',
                    implicit=1):
        assert not implicit or params     # implicit => params
        assert not retcallsems or retsems # retcallsems => returnsems
        cxxargs = [ ]

        if params:
            cxxargs.extend([ p.var() for p in self.params ])

        for ret in self.returns:
            if retsems is 'in':
                if retcallsems is 'in':
                    cxxargs.append(ret.var())
                elif retcallsems is 'out':
                    cxxargs.append(ExprAddrOf(ret.var()))
                else: assert 0
            elif retsems is 'out':
                if retcallsems is 'in':
                    cxxargs.append(ExprDeref(ret.var()))
                elif retcallsems is 'out':
                    cxxargs.append(ret.var())
                else: assert 0

        if not implicit:
            assert self.decl.type.hasImplicitActorParam()
            cxxargs = cxxargs[1:]

        return cxxargs


    @staticmethod
    def upgrade(messageDecl):
        assert isinstance(messageDecl, ipdl.ast.MessageDecl)
        if messageDecl.decl.type.hasImplicitActorParam():
            messageDecl.params.insert(
                0,
                _HybridDecl(
                    ipdl.type.ActorType(
                        messageDecl.decl.type.constructedType()),
                    'actor'))
        messageDecl.__class__ = MessageDecl
        return messageDecl

##--------------------------------------------------
def _semsToChannelParts(sems):
    if ipdl.ast.ASYNC == sems:   channel = 'AsyncChannel'
    elif ipdl.ast.SYNC == sems:  channel = 'SyncChannel'
    elif ipdl.ast.RPC == sems:   channel = 'RPCChannel'
    return [ 'mozilla', 'ipc', channel ]

def _semsToListener(sems):
    return { ipdl.ast.ASYNC: 'AsyncListener',
             ipdl.ast.SYNC: 'SyncListener',
             ipdl.ast.RPC: 'RPCListener' }[sems]


class Protocol(ipdl.ast.Protocol):
    def cxxTypedefs(self):
        return self.decl.cxxtypedefs

    def sendSems(self):
        return self.decl.type.toplevel().sendSemantics

    def channelName(self):
        return '::'.join(_semsToChannelParts(self.sendSems()))

    def channelSel(self):
        if self.decl.type.isToplevel():  return '.'
        return '->'

    def channelType(self):
        return Type('Channel', ptr=not self.decl.type.isToplevel())

    def channelHeaderFile(self):
        return '/'.join(_semsToChannelParts(self.sendSems())) +'.h'

    def listenerName(self):
        return _semsToListener(self.sendSems())

    def fqListenerName(self):
        return self.channelName() +'::'+ _semsToListener(self.sendSems())

    def managerCxxType(self, ptr=0):
        return Type('mozilla::ipc::IProtocolManager',
                    ptr=ptr,
                    T=Type(self.fqListenerName()))

    def registerMethod(self):
        return ExprVar('Register')

    def registerIDMethod(self):
        return ExprVar('RegisterID')

    def lookupIDMethod(self):
        return ExprVar('Lookup')

    def unregisterMethod(self):
        return ExprVar('Unregister')

    def otherProcessMethod(self):
        return ExprVar('OtherProcess')

    def nextActorIdExpr(self, side):
        assert self.decl.type.isToplevel()
        if side is 'parent':   op = '++'
        elif side is 'child':  op = '--'
        return ExprPrefixUnop(self.lastActorIdVar(), op)

    # an actor's C++ private variables
    def lastActorIdVar(self):
        assert self.decl.type.isToplevel()
        return ExprVar('mLastRouteId')

    def actorMapVar(self):
        assert self.decl.type.isToplevel()
        return ExprVar('mActorMap')

    def channelVar(self):
        return ExprVar('mChannel')

    def channelForSubactor(self):
        if self.decl.type.isToplevel():
            return ExprAddrOf(self.channelVar())
        return self.channelVar()

    def routingId(self):
        if self.decl.type.isToplevel():
            return ExprVar('MSG_ROUTING_CONTROL')
        return self.idVar()

    def idVar(self):
        assert not self.decl.type.isToplevel()
        return ExprVar('mId')

    def managerVar(self):
        assert not self.decl.type.isToplevel()
        return ExprVar('mManager')

    def otherProcessVar(self):
        assert self.decl.type.isToplevel()
        return ExprVar('mOtherProcess')

    @staticmethod
    def upgrade(protocol):
        assert isinstance(protocol, ipdl.ast.Protocol)
        protocol.__class__ = Protocol
        return protocol

##-----------------------------------------------------------------------------

class _DecorateWithCxxStuff(ipdl.ast.Visitor):
    """Phase 1 of lowering: decorate the IPDL AST with information
relevant to C++ code generation.

This pass results in an AST that is a poor man's "IR"; in reality, a
"hybrid" AST mainly consisting of IPDL nodes with new C++ info along
with some new IPDL/C++ nodes that are tuned for C++ codegen."""

    def __init__(self):
        # the set of typedefs that allow generated classes to
        # reference known C++ types by their "short name" rather than
        # fully-qualified name. e.g. |Foo| rather than |a::b::Foo|.
        self.typedefs = [ Typedef(Type('mozilla::ipc::ActorHandle'),
                                  'ActorHandle') ]
        self.protocolName = None

    def visitProtocol(self, pro):
        self.protocolName = pro.name
        pro.decl.cxxtypedefs = self.typedefs
        Protocol.upgrade(pro)
        return ipdl.ast.Visitor.visitProtocol(self, pro)


    def visitUsingStmt(self, using):
        if using.decl.fullname is not None:
            self.typedefs.append(Typedef(Type(using.decl.fullname),
                                         using.decl.shortname))

    def visitUnionDecl(self, ud):
        ud.decl.special = 0
        ud.decl.type._ud = ud           # sucky
        newcomponents = [ ]
        for ctype in ud.decl.type.components:
            if _hasVisibleActor(ctype):
                ud.decl.special = 1
                # if ctype has a visible actor, we need both
                # |ActorParent| and |ActorChild| union members
                newcomponents.append(_UnionMember(ctype, ud, side='parent'))
                newcomponents.append(_UnionMember(ctype, ud, side='child'))
            else:
                newcomponents.append(_UnionMember(ctype, ud))
        ud.components = newcomponents
        UnionDecl.upgrade(ud)

        if ud.decl.fullname is not None:
            self.typedefs.append(Typedef(Type(ud.fqClassName()), ud.name))
            

    def visitDecl(self, decl):
        return _HybridDecl(decl.type, decl.progname)

    def visitMessageDecl(self, md):
        md.namespace = self.protocolName
        md.params = [ param.accept(self) for param in md.inParams ]
        md.returns = [ ret.accept(self) for ret in md.outParams ]
        MessageDecl.upgrade(md)

    def visitTransitionStmt(self, ts):
        ts.state.decl.cxxenum = 'State_%s'% (ts.state.decl.progname)

##-----------------------------------------------------------------------------

class _GenerateProtocolHeader(ipdl.ast.Visitor):
    '''Creates a header containing code common to both the parent and
child actors.'''
    def __init__(self):
        self.protocol = None     # protocol we're generating a class for
        self.file = None         # File stuff is stuck in

    def lower(self, tu, outcxxfile):
        self.protocol = tu.protocol
        self.file = outcxxfile
        tu.accept(self)

    def visitTranslationUnit(self, tu):
        f = self.file

        f.addthing(Whitespace('''//
// Automatically generated by the IPDL compiler.
// Edit at your own risk
//

'''))
        f.addthings(_includeGuardStart(f))
        f.addthing(Whitespace.NL)

        ipdl.ast.Visitor.visitTranslationUnit(self, tu)

        f.addthing(Whitespace.NL)
        f.addthings(_includeGuardEnd(f))


    def visitCxxInclude(self, inc):
        self.file.addthing(CppDirective('include', '"'+ inc.file +'"'))

    def visitProtocolInclude(self, inc):
        self.file.addthing(CppDirective('include', '"%s.h"'% (
            _protocolHeaderBaseFilename(inc.tu.protocol))))

    def visitUnionDecl(self, ud):
        self.file.addthings(
            _generateCxxUnionStuff(ud))


    def visitProtocol(self, p):
        self.file.addthing(Whitespace("""
//-----------------------------------------------------------------------------
// Code common to %sChild and %sParent
//
"""% (p.name, p.name)))

        # construct the namespace into which we'll stick all our decls
        ns = Namespace(self.protocol.name)
        self.file.addthing(_putInNamespaces(ns, p.namespaces))
        ns.addstmt(Whitespace.NL)

        # state information
        stateenum = TypeEnum('State')
        for ts in p.transitionStmts:
            stateenum.addId(ts.state.decl.cxxenum)
        if len(p.transitionStmts):
            startstate = p.transitionStmts[0].state.decl.cxxenum
        else:
            startstate = '0'
        stateenum.addId('StateStart', startstate)
        stateenum.addId('StateError')
        stateenum.addId('StateLast')

        ns.addstmts([ StmtDecl(Decl(stateenum,'')), Whitespace.NL ])

        # spit out message type enum and classes
        msgenum = TypeEnum('MessageType')
        msgstart = self.protocol.name +'MsgStart << 10'
        msgenum.addId(self.protocol.name +'Start', msgstart)
        msgenum.addId(self.protocol.name +'PreStart', '('+ msgstart +') - 1')

        for md in p.messageDecls:
            msgenum.addId(md.msgId())
            if md.hasReply():
                msgenum.addId(md.replyId())

        msgenum.addId(self.protocol.name +'End')
        ns.addstmts([ StmtDecl(Decl(msgenum, '')), Whitespace.NL ])

        typedefs = self.protocol.decl.cxxtypedefs
        for md in p.messageDecls:
            paramsIn = md.makeCxxParams(paramsems='in', returnsems=None,
                                        pipetypes=1)
            paramsOut = md.makeCxxParams(paramsems='out', returnsems=None,
                                         pipetypes=1)
            ns.addstmts([
                _generateMessageClass(md.msgClass(), md.msgId(),
                                      paramsIn, paramsOut, typedefs),
                Whitespace.NL ])
            if md.hasReply():
                returnsIn = md.makeCxxParams(paramsems=None, returnsems='in',
                                             pipetypes=1)
                returnsOut = md.makeCxxParams(paramsems=None, returnsems='out',
                                              pipetypes=1)
                ns.addstmts([
                    _generateMessageClass(
                        md.replyClass(), md.replyId(), returnsIn, returnsOut,
                        typedefs),
                    Whitespace.NL ])

        ns.addstmts([ Whitespace.NL, Whitespace.NL ])

##--------------------------------------------------

def _generateMessageClass(clsname, msgid, inparams, outparams, typedefs):
    cls = Class(name=clsname, inherits=[ Inherit(Type('IPC::Message')) ])
    cls.addstmt(Label.PRIVATE)
    cls.addstmts(typedefs)
    cls.addstmt(Whitespace.NL)

    cls.addstmt(Label.PUBLIC)

    idenum = TypeEnum()
    idenum.addId('ID', msgid)
    cls.addstmt(StmtDecl(Decl(idenum, '')))

    # make the message constructor (serializer)
    ctor = ConstructorDefn(
        ConstructorDecl(clsname,
                        params=inparams),
        memberinits=[ExprMemberInit(ExprVar('IPC::Message'),
                                    [ ExprVar('MSG_ROUTING_NONE'),
                                      ExprVar('ID'),
                                      ExprVar('PRIORITY_NORMAL') ]) ])
    ctor.addstmts([
        StmtExpr(ExprCall(ExprVar('IPC::WriteParam'),
                          args=[ ExprVar.THIS, ExprVar(p.name) ]))
        for p in inparams
    ])
    
    cls.addstmts([ ctor, Whitespace.NL ])

    # make the message deserializer
    msgvar = ExprVar('msg')
    msgdecl = Decl(Type('Message', ptr=1, const=1), msgvar.name)
    reader = MethodDefn(MethodDecl(
        'Read', params=[ msgdecl ] + outparams, ret=Type.BOOL, static=1))

    itervar = ExprVar('iter')
    if len(outparams):
        reader.addstmts([
            StmtDecl(Decl(Type('void', ptr=True), itervar.name),
                     ExprLiteral.ZERO),
            Whitespace.NL ])

    for oparam in outparams:
        failif = StmtIf(ExprNot(ExprCall(
            ExprVar('IPC::ReadParam'),
            args=[ msgvar, ExprAddrOf(itervar), ExprVar(oparam.name) ])))
        failif.addifstmt(StmtReturn(ExprLiteral.FALSE))
        reader.addstmts([ failif, Whitespace.NL ])

    if len(outparams):
        ifdata = StmtIf(itervar)
        ifdata.addifstmt(StmtExpr(ExprCall(
            ExprSelect(msgvar, '->', 'EndRead'),
            args=[ itervar ])))
        reader.addstmt(ifdata)

    reader.addstmt(StmtReturn(ExprLiteral.TRUE))
    cls.addstmts([ reader, Whitespace.NL ])

    # generate a logging function
    # 'pfx' will be something like "[FooParent] sent"
    pfxvar = ExprVar('__pfx')
    outfvar = ExprVar('__outf')
    logger = MethodDefn(MethodDecl(
        'Log',
        params=([ Decl(Type('std::string', const=1, ref=1), pfxvar.name),
                  Decl(Type('FILE', ptr=True), outfvar.name) ]),
        const=1))
    # TODO/cjones: allow selecting what information is printed to 
    # the log
    msgvar = ExprVar('__logmsg')
    logger.addstmt(StmtDecl(Decl(Type('std::string'), msgvar.name)))

    def appendToMsg(thing):
        return StmtExpr(ExprCall(ExprSelect(msgvar, '.', 'append'),
                                 args=[ thing ]))
    logger.addstmts([
        StmtExpr(ExprCall(
            ExprVar('StringAppendF'),
            args=[ ExprAddrOf(msgvar),
                   ExprLiteral.String('[time:%" PRId64 "]'),
                   ExprCall(ExprVar('PR_Now')) ])),
        appendToMsg(pfxvar),
        appendToMsg(ExprLiteral.String(clsname +'(')),
        Whitespace.NL
    ])

    # TODO turn this back on when string stuff is sorted

    logger.addstmt(appendToMsg(ExprLiteral.String(')\\n')))

    # and actually print the log message
    logger.addstmt(StmtExpr(ExprCall(
        ExprVar('fputs'),
        args=[ ExprCall(ExprSelect(msgvar, '.', 'c_str')), outfvar ])))

    cls.addstmt(logger)

    return cls

##--------------------------------------------------

def _generateCxxUnionStuff(ud):
    # This Union class basically consists of a type (enum) and a
    # union for storage.  The union can contain POD and non-POD
    # types.  Each type needs a copy ctor, assignment operator,
    # and dtor.
    #
    # Rather than templating this class and only providing
    # specializations for the types we support, which is slightly
    # "unsafe" in that C++ code can add additional specializations
    # without the IPDL compiler's knowledge, we instead explicitly
    # implement non-templated methods for each supported type.
    #
    # The one complication that arises is that C++, for arcane
    # reasons, does not allow the placement destructor of a
    # builtin type, like int, to be directly invoked.  So we need
    # to hack around this by internally typedef'ing all
    # constituent types.  Sigh.
    #
    # So, for each type, this "Union" class needs:
    # (private)
    #  - entry in the type enum
    #  - entry in the storage union
    #  - [type]ptr() method to get a type* from the underlying union
    #  - same as above to get a const type*
    #  - typedef to hack around placement delete limitations
    # (public)
    #  - placement delete case for dtor
    #  - copy ctor
    #  - case in generic copy ctor
    #  - operator= impl
    #  - case in generic operator=
    #  - operator [type&]
    #  - operator [const type&] const
    #  - [type&] get_[type]()
    #  - [const type&] get_[type]() const
    #
    cls = Class(ud.name, final=1)
    # const Union&, i.e., Union type with inparam semantics
    inClsType = Type(ud.name, const=1, ref=1)
    refClsType = Type(ud.name, ref=1)
    typetype = Type('Type')
    valuetype = Type('Value')
    mtypevar = ExprVar('mType')
    mvaluevar = ExprVar('mValue')
    maybedtorvar = ExprVar('MaybeDestroy')
    assertsanityvar = ExprVar('AssertSanity')
    tnonevar = ExprVar('T__None')
    tfirstvar = ExprVar('T__First')
    tlastvar = ExprVar('T__Last')

    def callAssertSanity(uvar=None, expectTypeVar=None):
        func = assertsanityvar
        args = [ ]
        if uvar is not None:
            func = ExprSelect(uvar, '.', assertsanityvar.name)
        if expectTypeVar is not None:
            args.append(expectTypeVar)
        return ExprCall(func, args=args)

    def callMaybeDestroy(newTypeVar):
        return ExprCall(maybedtorvar, args=[ newTypeVar ])

    def maybeReconstruct(memb, newTypeVar):
        ifdied = StmtIf(callMaybeDestroy(newTypeVar))
        ifdied.addifstmt(StmtExpr(memb.callPlacementCtor()))
        return ifdied

    # compute all the typedefs and forward decls we need to make
    usingTypedefs = [ ]
    forwarddeclstmts = [ ]
    class computeTypeDeps(ipdl.type.TypeVisitor):
        def __init__(self):  self.seen = set()

        def maybeTypedef(self, fqname, name):
            if fqname != name:
                usingTypedefs.append(Typedef(Type(fqname), name))
        
        def visitBuiltinCxxType(self, t):
            if t in self.seen: return
            self.seen.add(t)
            self.maybeTypedef(t.fullname(), t.name())

        def visitImportedCxxType(self, t):
            if t in self.seen: return
            self.seen.add(t)
            self.maybeTypedef(t.fullname(), t.name())

        def visitActorType(self, t):
            if t in self.seen: return
            self.seen.add(t)
            
            fqname, name = t.fullname(), t.name()

            self.maybeTypedef(_actorName(fqname, 'Parent'),
                              _actorName(name, 'Parent'))
            self.maybeTypedef(_actorName(fqname, 'Child'),
                              _actorName(name, 'Child'))

            forwarddeclstmts.extend([
                _makeForwardDecl(t.protocol, 'parent'), Whitespace.NL,
                _makeForwardDecl(t.protocol, 'child'), Whitespace.NL
            ])

        def visitUnionType(self, t):
            if t == ud.decl.type or t in self.seen: return
            self.seen.add(t)
            self.maybeTypedef(t.fullname(), t.name())

            return ipdl.type.TypeVisitor.visitUnionType(self, t)

        def visitArrayType(self, t):
            return ipdl.type.TypeVisitor.visitArrayType(self, t)

        def visitVoidType(self, v): assert 0
        def visitMessageType(self, v): assert 0
        def visitProtocolType(self, v): assert 0
        def visitStateType(self, v): assert 0

    gettypedeps = computeTypeDeps()
    for c in ud.components:
        c.ipdltype.accept(gettypedeps)


    # the |Type| enum, used to switch on the discunion's real type
    cls.addstmt(Label.PUBLIC)
    typeenum = TypeEnum(typetype.name)
    typeenum.addId(tnonevar.name, 0)
    firstid = ud.components[0].enum()
    typeenum.addId(firstid, 1)
    for c in ud.components[1:]:
        typeenum.addId(c.enum())
    typeenum.addId(tfirstvar.name, firstid)
    typeenum.addId(tlastvar.name, ud.components[-1].enum())
    cls.addstmts([ StmtDecl(Decl(typeenum,'')),
                   Whitespace.NL ])

    cls.addstmt(Label.PRIVATE)
    cls.addstmts(
        usingTypedefs
                # hacky typedef's that allow placement dtors of builtins
        + [ Typedef(c.bareType(), c.typedef()) for c in ud.components ])
    cls.addstmt(Whitespace.NL)

    # the C++ union the discunion use for storage
    valueunion = TypeUnion(valuetype.name)
    for c in ud.components:
        valueunion.addComponent(c.unionType(), c.name)
    cls.addstmts([ StmtDecl(Decl(valueunion,'')),
                       Whitespace.NL ])

    # for each constituent type T, add private accessors that
    # return a pointer to the Value union storage casted to |T*|
    # and |const T*|
    for c in ud.components:
        getptr = MethodDefn(MethodDecl(
            c.getPtrName(), params=[ ], ret=c.ptrToType()))
        getptr.addstmt(StmtReturn(c.ptrToSelfExpr()))

        getptrconst = MethodDefn(MethodDecl(
            c.getConstPtrName(), params=[ ], ret=c.constPtrToType(), const=1))
        getptrconst.addstmt(StmtReturn(c.constptrToSelfExpr()))

        cls.addstmts([ getptr, getptrconst ])
    cls.addstmt(Whitespace.NL)

    # add a helper method that invokes the placement dtor on the
    # current underlying value, only if |aNewType| is different
    # than the current type, and returns true if the underlying
    # value needs to be re-constructed
    newtypevar = ExprVar('aNewType')
    maybedtor = MethodDefn(MethodDecl(
        maybedtorvar.name,
        params=[ Decl(typetype, newtypevar.name) ],
        ret=Type.BOOL))
    # wasn't /actually/ dtor'd, but it needs to be re-constructed
    ifnone = StmtIf(ExprBinary(mtypevar, '==', tnonevar))
    ifnone.addifstmt(StmtReturn(ExprLiteral.TRUE))
    # same type, nothing to see here
    ifnochange = StmtIf(ExprBinary(mtypevar, '==', newtypevar))
    ifnochange.addifstmt(StmtReturn(ExprLiteral.FALSE))
    # need to destroy.  switch on underlying type
    dtorswitch = StmtSwitch(mtypevar)
    for c in ud.components:
        dtorswitch.addcase(
            CaseLabel(c.enum()),
            StmtBlock([ StmtExpr(c.callPlacementDtor()),
                        StmtBreak() ]))
    dtorswitch.addcase(
        DefaultLabel(),
        StmtBlock([ _runtimeAbort("not reached"), StmtBreak() ]))
    maybedtor.addstmts([
        ifnone,
        ifnochange,
        dtorswitch,
        StmtReturn(ExprLiteral.TRUE)
    ])
    cls.addstmts([ maybedtor, Whitespace.NL ])

    # add helper methods that ensure the discunion has a
    # valid type
    sanity = MethodDefn(MethodDecl(
        assertsanityvar.name, ret=Type('void'), const=1))
    sanity.addstmts([
        _abortIfFalse(ExprBinary(tfirstvar, '<=', mtypevar),
                      'invalid type tag'),
        _abortIfFalse(ExprBinary(mtypevar, '<=', tlastvar),
                      'invalid type tag') ])
    cls.addstmt(sanity)

    atypevar = ExprVar('aType')
    sanity2 = MethodDefn(
        MethodDecl(assertsanityvar.name,
                       params=[ Decl(typetype, atypevar.name) ],
                       ret=Type('void'),
                       const=1))
    sanity2.addstmts([
        StmtExpr(ExprCall(assertsanityvar)),
        _abortIfFalse(ExprBinary(mtypevar, '==', atypevar),
                      'unexpected type tag') ])
    cls.addstmts([ sanity2, Whitespace.NL ])

    ## ---- begin public methods -----

    # Union() default ctor
    cls.addstmts([
        Label.PUBLIC,
        ConstructorDefn(
            ConstructorDecl(ud.name),
            memberinits=[ ExprMemberInit(mtypevar, [ tnonevar ]) ]),
        Whitespace.NL
    ])

    # Union(const T&) copy ctors
    othervar = ExprVar('aOther')
    for c in ud.components:
        copyctor = ConstructorDefn(ConstructorDecl(
            ud.name, params=[ Decl(c.inType(), othervar.name) ]))
        copyctor.addstmts([
            StmtExpr(c.callPlacementCtor(othervar)),
            StmtExpr(ExprAssn(mtypevar, c.enumvar())) ])
        cls.addstmts([ copyctor, Whitespace.NL ])

    # Union(const Union&) copy ctor
    copyctor = ConstructorDefn(ConstructorDecl(
        ud.name, params=[ Decl(inClsType, othervar.name) ]))
    othertype = ud.callType(othervar)
    copyswitch = StmtSwitch(othertype)
    for c in ud.components:
        copyswitch.addcase(
            CaseLabel(c.enum()),
            StmtBlock([
                StmtExpr(c.callPlacementCtor(
                    ExprCall(ExprSelect(othervar,
                                        '.', c.getConstTypeName())))),
                StmtBreak()
            ]))
    copyswitch.addcase(
        DefaultLabel(),
        StmtBlock([ _runtimeAbort('unreached'), StmtReturn() ]))
    copyctor.addstmts([
        StmtExpr(callAssertSanity(uvar=othervar)),
        copyswitch,
        StmtExpr(ExprAssn(mtypevar, othertype))
    ])
    cls.addstmts([ copyctor, Whitespace.NL ])

    # ~Union()
    dtor = DestructorDefn(DestructorDecl(ud.name))
    dtor.addstmt(StmtExpr(callMaybeDestroy(tnonevar)))
    cls.addstmts([ dtor, Whitespace.NL ])

    # type()
    typemeth = MethodDefn(MethodDecl('type', ret=typetype, const=1))
    typemeth.addstmt(StmtReturn(mtypevar))
    cls.addstmts([ typemeth, Whitespace.NL ])

    # Union& operator=(const T&) methods
    rhsvar = ExprVar('aRhs')
    for c in ud.components:
        opeq = MethodDefn(MethodDecl(
            'operator=',
            params=[ Decl(c.inType(), rhsvar.name) ],
            ret=refClsType))
        opeq.addstmts([
            # might need to placement-delete old value first
            maybeReconstruct(c, c.enumvar()),
            StmtExpr(c.callOperatorEq(rhsvar)),
            StmtExpr(ExprAssn(mtypevar, c.enumvar())),
            StmtReturn(ExprDeref(ExprVar.THIS))
        ])
        cls.addstmts([ opeq, Whitespace.NL ])

    # Union& operator=(const Union&)
    opeq = MethodDefn(MethodDecl(
        'operator=',
        params=[ Decl(inClsType, rhsvar.name) ],
        ret=refClsType))
    rhstypevar = ExprVar('t')
    opeqswitch = StmtSwitch(rhstypevar)
    for c in ud.components:
        case = StmtBlock()
        case.addstmts([
            maybeReconstruct(c, rhstypevar),
            StmtExpr(c.callOperatorEq(
                ExprCall(ExprSelect(rhsvar, '.', c.getConstTypeName())))),
            StmtBreak()
        ])
        opeqswitch.addcase(CaseLabel(c.enum()), case)
    opeqswitch.addcase(
        DefaultLabel(),
        StmtBlock([ _runtimeAbort('unreached'), StmtBreak() ]))
    opeq.addstmts([
        StmtExpr(callAssertSanity(uvar=rhsvar)),
        StmtDecl(Decl(typetype, rhstypevar.name), init=ud.callType(rhsvar)),
        opeqswitch,
        StmtExpr(ExprAssn(mtypevar, rhstypevar)),
        StmtReturn(ExprDeref(ExprVar.THIS))
    ])
    cls.addstmts([ opeq, Whitespace.NL ])

    # accessors for each type: operator T&, operator const T&,
    # T& get(), const T& get()
    for c in ud.components:
        getValueVar = ExprVar(c.getTypeName())
        getConstValueVar = ExprVar(c.getConstTypeName())

        getvalue = MethodDefn(MethodDecl(getValueVar.name, ret=c.refType()))
        getvalue.addstmts([
            StmtExpr(callAssertSanity(expectTypeVar=c.enumvar())),
            StmtReturn(ExprDeref(c.callGetPtr()))
        ])

        getconstvalue = MethodDefn(MethodDecl(
            getConstValueVar.name, ret=c.constRefType(), const=1))
        getconstvalue.addstmts([
            StmtExpr(callAssertSanity(expectTypeVar=c.enumvar())),
            StmtReturn(ExprDeref(c.callGetConstPtr()))
        ])

        optype = MethodDefn(MethodDecl('', typeop=c.refType()))
        optype.addstmt(StmtReturn(ExprCall(getValueVar)))
        opconsttype = MethodDefn(MethodDecl(
            '', const=1, typeop=c.constRefType()))
        opconsttype.addstmt(StmtReturn(ExprCall(getConstValueVar)))

        cls.addstmts([ getvalue, getconstvalue,
                       optype, opconsttype,
                       Whitespace.NL ])

    # private vars
    cls.addstmts([
        Label.PRIVATE,
        StmtDecl(Decl(valuetype, mvaluevar.name)),
        StmtDecl(Decl(typetype, mtypevar.name))
    ])

    # serializer/deserializer
    fqUnionType = Type(ud.fqClassName())
    pickle = Class(name='ParamTraits', specializes=fqUnionType, struct=1)
    pickle.addstmts(
        [ Label.PRIVATE ]
        + usingTypedefs
        + [ Whitespace.NL,
            Label.PUBLIC,
            Typedef(fqUnionType, 'paramType'),
            Whitespace.NL
        ])

    writevar = ExprVar('WriteParam')
    msgvar = ExprVar('aMsg')
    paramvar = ExprVar('aParam')
    callparamtype = ud.callType(paramvar)

    # Write(Message*, paramType&)
    serialize = MethodDefn(MethodDecl(
        'Write',
        params=[ Decl(Type('Message', ptr=1), msgvar.name),
                 Decl(Type('paramType', const=1, ref=1), paramvar.name) ],
        static=1))
    serialize.addstmt(StmtExpr(
        ExprCall(
            writevar,
            args=[ ExprVar('aMsg'),
                   ExprCast(callparamtype, Type.INT, static=1) ])))

    writeswitch = StmtSwitch(callparamtype)
    for c in ud.components:
        case = StmtBlock()
        getvalue = ExprCall(ExprSelect(paramvar, '.', c.getConstTypeName()))

        if not c.special:
            case.addstmt(StmtExpr(
                ExprCall(writevar, args=[ msgvar, getvalue ])))
        elif c.ipdltype.isActor():
            # going to hell in a handbasket for this ...
            case.addstmt(StmtExpr(
                ExprCall(writevar,
                         args=[ msgvar,
                                ExprCast(getvalue, Type.INTPTR,
                                         reinterpret=1) ])))
        else:
            assert c.ipdltype.isArray() and c.ipdltype.basetype.isActor()
            # the devil made me do it!
            lenvar = ExprVar('len')
            case.addstmts([
                StmtDecl(Decl(Type.UINT32, lenvar.name),
                         init=ExprCall(ExprSelect(getvalue, '.', 'Length'))),
                StmtExpr(ExprCall(writevar, args=[ msgvar, lenvar ])),
                StmtExpr(ExprCall(
                    ExprSelect(msgvar, '->', 'WriteBytes'),
                    args=[
                        ExprCast(
                            ExprCall(ExprSelect(getvalue, '.', 'Elements')),
                            Type('void', const=1, ptr=1),
                            reinterpret=1),
                        ExprBinary(lenvar, '*', ExprSizeof(Type.INTPTR))
                    ]))
            ])
        case.addstmt(StmtBreak())

        writeswitch.addcase(CaseLabel('paramType::'+ c.enum()), case)
    writeswitch.addcase(
        DefaultLabel(),
        StmtBlock([ _runtimeAbort('unreached'), StmtReturn() ])
    )
    serialize.addstmt(writeswitch)

    # Read(const Message& msg, void** iter, paramType* out)
    itervar = ExprVar('aIter')
    deserialize = MethodDefn(MethodDecl(
        'Read',
        params=[ Decl(Type('Message', const=1, ptr=1), msgvar.name),
                 Decl(Type('void', ptrptr=1), itervar.name),
                 Decl(Type('paramType', ptr=1), paramvar.name) ],
        ret=Type.BOOL,
        static=1))

    typevar = ExprVar('type')
    readvar = ExprVar('ReadParam')
    fqTfirstVar = ExprVar('paramType::'+ tfirstvar.name)
    fqTlastVar = ExprVar('paramType::'+ tlastvar.name)
    failif = StmtIf(ExprNot(
        ExprBinary(
            ExprCall(readvar, args=[ msgvar, itervar, ExprAddrOf(typevar) ]),
            '&&', ExprBinary(ExprBinary(fqTfirstVar, '<=', typevar),
                             '&&', ExprBinary(typevar, '<=', fqTlastVar)))))
    failif.addifstmt(StmtReturn(ExprLiteral.FALSE))

    deserialize.addstmts([
        StmtDecl(Decl(Type.INT, typevar.name)),
        failif,
        Whitespace.NL
    ])

    derefunion = ExprDeref(paramvar)
    valvar = ExprVar('val')
    readswitch = StmtSwitch(typevar)
    for c in ud.components:
        case = StmtBlock()

        # special-case actor and actor[]
        readstmts = None
        if not c.special:
            failif = StmtIf(ExprNot(ExprCall(
                readvar, args=[ msgvar, itervar, ExprAddrOf(valvar) ])))
            failif.addifstmt(StmtReturn(ExprLiteral.FALSE))

            case.addstmts([
                StmtDecl(Decl(c.bareType(), valvar.name)),
                Whitespace.NL,
                failif,
                StmtExpr(ExprAssn(derefunion, valvar)),
                StmtReturn(ExprLiteral.TRUE)
            ])
        elif c.ipdltype.isActor():
            failif = StmtIf(ExprNot(ExprCall(
                readvar,
                args=[ msgvar, itervar,
                       ExprCast(ExprAddrOf(valvar),
                                Type('intptr_t', ptr=1),
                                reinterpret=1) ])))
            failif.addifstmt(StmtReturn(ExprLiteral.FALSE))

            case.addstmts([
                StmtDecl(Decl(c.otherSideBareType(), valvar.name)),
                Whitespace.NL,
                failif,
                StmtExpr(ExprAssn(derefunion, valvar)),
                StmtReturn(ExprLiteral.TRUE)
            ])
        else:
            assert c.ipdltype.isArray() and c.ipdltype.basetype.isActor()

            lenvar = ExprVar('len')
            failif = StmtIf(ExprNot(ExprCall(
                readvar, args=[ msgvar, itervar, ExprAddrOf(lenvar) ])))
            failif.addifstmt(StmtReturn(ExprLiteral.FALSE))

            ptrvar = ExprVar('ptr')
            failifarray = StmtIf(ExprNot(ExprCall(
                ExprSelect(msgvar, '->', 'ReadBytes'),
                args=[ itervar,
                       ExprCast(ExprAddrOf(ptrvar),
                                Type('char', const=1, ptrptr=1),
                                reinterpret=1),
                       ExprBinary(lenvar, '*', ExprSizeof(Type.INTPTR)) ])))
            failifarray.addifstmt(StmtReturn(ExprLiteral.FALSE))

            cxxtype = c.otherSideBareType()
            actorptr = deepcopy(cxxtype.T)
            actorptr.ptr = 0
            actorptr.ptrptr = 1

            actorconstptr = deepcopy(actorptr)
            actorconstptr.const = 1
            actorconstptr.ptrptr = 0
            actorconstptr.ptrconstptr = 1

            case.addstmts([
                StmtDecl(Decl(Type.UINT32, lenvar.name)),
                StmtDecl(Decl(Type('intptr_t', const=1, ptr=1), ptrvar.name)),
                StmtDecl(Decl(cxxtype, valvar.name)),
                Whitespace.NL,
                failif,
                failifarray,
                StmtExpr(ExprCall(
                    ExprSelect(valvar, '.', 'InsertElementsAt'),
                    args=[ ExprLiteral.ZERO,
                           ExprCast(
                               ExprCast(ptrvar, actorconstptr, reinterpret=1),
                               actorptr,
                               const=1),
                           lenvar ])),
                StmtExpr(ExprAssn(derefunion, valvar)),
                StmtReturn(ExprLiteral.TRUE)
            ])

        readswitch.addcase(CaseLabel('paramType::'+ c.enum()), case)

    defaultcase = StmtBlock()
    defaultcase.addstmts([
        _runtimeAbort('unreached'), StmtReturn(ExprLiteral.FALSE) ])
    readswitch.addcase(DefaultLabel(), defaultcase)

    deserialize.addstmts([
        readswitch,
        Whitespace.NL
    ])

    # Log(const paramType&, std::wstring* log)
    logvar = ExprVar('aLog')
    logger = MethodDefn(MethodDecl(
        'Log',
        params=[ Decl(Type('paramType', const=1, ref=1), 'aParam'),
                 Decl(Type('std::wstring', ptr=1), logvar.name) ],
        static=1))

    # TODO: real implementation
    logger.addstmt(StmtExpr(ExprCall(
        ExprSelect(logvar, '->', 'append'),
        args=[ ExprLiteral.WString('('+ ud.name +')') ])))

    pickle.addstmts([
        serialize,
        Whitespace.NL,
        deserialize,
        Whitespace.NL,
        logger
    ])

    return (
        [
            Whitespace("""
//-----------------------------------------------------------------------------
// Definition of the IPDL type |union %s|
//
"""% (ud.name))
        ]
        + forwarddeclstmts
        + [
            _putInNamespaces(cls, ud.namespaces),
            Whitespace("""
//
// serializer/deserializer
//
"""),
            _putInNamespaces(pickle, [ Namespace('IPC') ]),
            Whitespace.NL
        ])


##-----------------------------------------------------------------------------

class _FindFriends(ipdl.ast.Visitor):
    def __init__(self):
        self.mytype = None              # ProtocolType
        self.vtype = None               # ProtocolType
        self.friends = set()            # set<ProtocolType>
        self.visited = set()            # set<ProtocolType>

    def findFriends(self, ptype):
        self.mytype = ptype
        self.walkUpTheProtocolTree(ptype)
        self.walkDownTheProtocolTree(ptype)
        return self.friends

    # TODO could make this into a _iterProtocolTreeHelper ...
    def walkUpTheProtocolTree(self, ptype):
        if not ptype.isManaged():
            return
        mtype = ptype.manager
        self.visit(mtype)
        self.walkUpTheProtocolTree(mtype)

    def walkDownTheProtocolTree(self, ptype):
        if not ptype.isManager():
            return
        for mtype in ptype.manages:
            self.visit(mtype)
            self.walkDownTheProtocolTree(mtype)

    def visit(self, ptype):
        if ptype in self.visited:
            return
        self.visited.add(ptype)

        savedptype = self.vtype
        self.vtype = ptype
        ptype._p.accept(self)
        self.vtype = savedptype

    def visitMessageDecl(self, md):
        for it in self.iterActorParams(md):
            if it.protocol == self.mytype:
                self.friends.add(self.vtype)

    def iterActorParams(self, md):
        for param in md.inParams:
            for actor in ipdl.type.iteractortypes(param.type):
                yield actor
        for ret in md.outParams:
            for actor in ipdl.type.iteractortypes(ret.type):
                yield actor


class _Result:
    Type = Type('Result')

    Processed = ExprVar('MsgProcessed')
    NotKnown = ExprVar('MsgNotKnown')
    NotAllowed = ExprVar('MsgNotAllowed')
    PayloadError = ExprVar('MsgPayloadError')
    RouteError = ExprVar('MsgRouteError')
    ValuError = ExprVar('MsgValueError') # [sic]

class _GenerateProtocolActorHeader(ipdl.ast.Visitor):
    def __init__(self, myside):
        self.side = myside              # "parent" or "child"
        self.prettyside = myside.title()
        self.clsname = None
        self.protocol = None
        self.file = None
        self.ns = None
        self.cls = None
        self.includedActorTypedefs = [ ]

    def lower(self, tu, clsname, cxxHeaderFile):
        self.clsname = clsname
        self.file = cxxHeaderFile
        tu.accept(self)

    def visitTranslationUnit(self, tu):
        f = self.file

        f.addthing(Whitespace('''//
// Automatically generated by ipdlc.
// Edit at your own risk
//

'''))
        f.addthings(_includeGuardStart(f))
        f.addthings([
            Whitespace.NL,
            CppDirective(
                'include',
                '"'+ _protocolHeaderBaseFilename(tu.protocol) +'.h"') ])

        self.protocol = tu.protocol

        for pinc in tu.protocolIncludes:
            pinc.accept(self)

        tu.protocol.accept(self)

        f.addthing(Whitespace.NL)
        f.addthings(_includeGuardEnd(f))


    def visitProtocolInclude(self, pi):
        ip = pi.tu.protocol

        if self.protocol.decl.type.isManagerOf(ip.decl.type):
            self.file.addthing(
                CppDirective(
                    'include',
                    '"%s%s.h"'% (_protocolHeaderBaseFilename(ip),
                                 self.prettyside)))
        else:
            self.file.addthing(_makeForwardDecl(ip.decl.type, self.prettyside))

        if ip.decl.fullname is not None:
            self.includedActorTypedefs.append(Typedef(
                Type(_actorName(ip.decl.fullname, self.prettyside)),
                _actorName(ip.decl.shortname, self.prettyside)))


    def visitProtocol(self, p):
        self.file.addthings([
            CppDirective('ifdef', 'DEBUG'),
            CppDirective('include', '"prenv.h"'),
            CppDirective('endif', '// DEBUG')
        ])

        self.protocol = p

        # FIXME: all actors impl Iface for now
        if p.decl.type.isManager() or 1:
            self.file.addthing(CppDirective('include', '"base/id_map.h"'))

        self.file.addthings([
            CppDirective('include', '"'+ p.channelHeaderFile() +'"'),
            Whitespace.NL ])

        # construct the namespace into which we'll stick all our decls
        if 0 == len(p.namespaces):
            self.ns = self.file
        else:
            self.ns = Namespace(p.namespaces[-1].name)
            outerns = _putInNamespaces(self.ns, p.namespaces[:-1])
            self.file.addthing(outerns)
        self.ns.addstmts([ Whitespace.NL, Whitespace.NL ])

        inherits = [ Inherit(Type(p.fqListenerName())) ]
        if p.decl.type.isManager():
            inherits.append(Inherit(p.managerCxxType()))
        self.cls = Class(self.clsname, inherits=inherits, abstract=True)

        friends = _FindFriends().findFriends(p.decl.type)
        if p.decl.type.isManaged():
            friends.add(p.decl.type.manager)

        for friend in friends:
            self.file.addthings([
                Whitespace.NL,
                _makeForwardDecl(friend, self.prettyside),
                Whitespace.NL
            ])
            self.cls.addstmts([
                FriendClassDecl(_actorName(friend.fullname(),
                                           self.prettyside)),
                Whitespace.NL ])

        self.cls.addstmt(Label.PROTECTED)
        for typedef in p.cxxTypedefs():
            self.cls.addstmt(typedef)
        for typedef in self.includedActorTypedefs:
            self.cls.addstmt(typedef)
        self.cls.addstmt(Whitespace.NL)

        # interface methods that the concrete subclass has to impl
        for md in p.messageDecls:
            isctor, isdtor = md.decl.type.isCtor(), md.decl.type.isDtor()
            if isctor:
                self.cls.addstmt(StmtDecl(MethodDecl(
                    md.allocMethod().name,
                    params=md.makeCxxParams(paramsems='in', returnsems='out',
                                            side=self.side,
                                            implicit=0),
                    ret=md.actorDecl().bareType(self.side),
                    virtual=1, pure=1)))
            elif isdtor:
                self.cls.addstmt(StmtDecl(MethodDecl(
                    md.deallocMethod().name,
                    params=md.makeCxxParams(paramsems='in', returnsems='out',
                                            side=self.side,
                                            implicit=1),
                    ret=Type.BOOL,
                    virtual=1, pure=1)))

            if self.receivesMessage(md):
                # generate Recv/Answer* interface
                recvDecl = MethodDecl(
                    md.recvMethod().name,
                    params=md.makeCxxParams(paramsems='in', returnsems='out',
                                            side=self.side, implicit=1),
                    ret=Type.BOOL, virtual=1)

                if isctor or isdtor:
                    defaultRecv = MethodDefn(recvDecl)
                    defaultRecv.addstmt(StmtReturn(ExprLiteral.TRUE))
                    self.cls.addstmt(defaultRecv)
                else:
                    recvDecl.pure = 1
                    self.cls.addstmt(StmtDecl(recvDecl))

        self.cls.addstmt(Whitespace.NL)

        self.cls.addstmts([
            Label.PRIVATE,
            Typedef(Type('IPC::Message'), 'Message'),
            Typedef(Type(p.channelName()), 'Channel'),
            Typedef(Type(p.fqListenerName()), 'ChannelListener'),
            Typedef(Type('base::ProcessHandle'), 'ProcessHandle'),
            Whitespace.NL,
        ])

        self.cls.addstmt(Label.PUBLIC)
        # Actor()
        ctor = ConstructorDefn(ConstructorDecl(self.clsname))
        if p.decl.type.isToplevel():
            ctor.memberinits = [
                ExprMemberInit(p.channelVar(), [
                    ExprCall(ExprVar('ALLOW_THIS_IN_INITIALIZER_LIST'),
                             [ ExprVar.THIS ]) ]),
                ExprMemberInit(p.lastActorIdVar(), [ ExprLiteral.ZERO ])
            ]
        else:
            ctor.memberinits = [
                ExprMemberInit(p.idVar(), [ ExprLiteral.ZERO ]) ]

        ctor.addstmt(StmtExpr(ExprCall(ExprVar('MOZ_COUNT_CTOR'),
                                       [ ExprVar(self.clsname) ])))
        self.cls.addstmts([ ctor, Whitespace.NL ])

        # ~Actor()
        dtor = DestructorDefn(
            DestructorDecl(self.clsname, virtual=True))
        dtor.addstmt(StmtExpr(ExprCall(ExprVar('MOZ_COUNT_DTOR'),
                                               [ ExprVar(self.clsname) ])))

        self.cls.addstmts([ dtor, Whitespace.NL ])

        if p.decl.type.isToplevel():
            # Open()
            aTransportVar = ExprVar('aTransport')
            aThreadVar = ExprVar('aThread')
            processvar = ExprVar('aOtherProcess')
            openmeth = MethodDefn(
                MethodDecl(
                    'Open',
                    params=[ Decl(Type('Channel::Transport', ptr=True),
                                      aTransportVar.name),
                             Decl(Type('ProcessHandle'), processvar.name),
                             Decl(Type('MessageLoop', ptr=True),
                                      aThreadVar.name +' = 0') ],
                    ret=Type.BOOL))

            openmeth.addstmts([
                StmtExpr(ExprAssn(p.otherProcessVar(), processvar)),
                StmtReturn(ExprCall(ExprSelect(p.channelVar(), '.', 'Open'),
                                    [ aTransportVar, aThreadVar ]))
            ])
            self.cls.addstmts([
                openmeth,
                Whitespace.NL ])

            # Close()
            closemeth = MethodDefn(MethodDecl('Close'))
            closemeth.addstmt(StmtExpr(
                ExprCall(ExprSelect(p.channelVar(), '.', 'Close'))))
            self.cls.addstmts([ closemeth, Whitespace.NL ])

        ## OnMessageReceived()/OnCallReceived()

        # save these away for use in message handler case stmts
        msgvar = ExprVar('msg')
        self.msgvar = msgvar
        replyvar = ExprVar('reply')
        self.replyvar = replyvar
        
        msgtype = ExprCall(ExprSelect(msgvar, '.', 'type'), [ ])
        self.asyncSwitch = StmtSwitch(msgtype)
        if p.decl.type.toplevel().talksSync():
            self.syncSwitch = StmtSwitch(msgtype)
            if p.decl.type.toplevel().talksRpc():
                self.rpcSwitch = StmtSwitch(msgtype)

        # implement Send*() methods and add dispatcher cases to
        # message switch()es
        for md in p.messageDecls:
            self.visitMessageDecl(md)

        # add default cases
        default = StmtBlock()
        default.addstmt(StmtReturn(_Result.NotKnown))
        self.asyncSwitch.addcase(DefaultLabel(), default)
        if p.decl.type.toplevel().talksSync():
            self.syncSwitch.addcase(DefaultLabel(), default)
            if p.decl.type.toplevel().talksRpc():
                self.rpcSwitch.addcase(DefaultLabel(), default)


        def makeHandlerMethod(name, switch, hasReply, dispatches=0):
            params = [ Decl(Type('Message', const=1, ref=1), msgvar.name) ]
            if hasReply:
                params.append(Decl(Type('Message', ref=1, ptr=1),
                                   replyvar.name))
            
            method = MethodDefn(MethodDecl(name, virtual=True,
                                           params=params, ret=_Result.Type))
            if dispatches:
                routevar = ExprVar('__route')
                routedecl = StmtDecl(
                    Decl(_actorIdType(), routevar.name),
                    init=ExprCall(ExprSelect(msgvar, '.', 'routing_id')))

                routeif = StmtIf(ExprBinary(
                    ExprVar('MSG_ROUTING_CONTROL'), '!=', routevar))
                routedvar = ExprVar('__routed')
                routeif.ifb.addstmt(
                    StmtDecl(Decl(Type('ChannelListener', ptr=1),
                                  routedvar.name),
                             _lookupListener(routevar)))
                failif = StmtIf(ExprPrefixUnop(routedvar, '!'))
                failif.ifb.addstmt(StmtReturn(_Result.RouteError))
                routeif.ifb.addstmt(failif)

                routeif.ifb.addstmt(StmtReturn(ExprCall(
                    ExprSelect(routedvar, '->', name),
                    args=[ ExprVar(p.name) for p in params ])))

                method.addstmts([ routedecl, routeif, Whitespace.NL ])

            # bug 509581: don't generate the switch stmt if there
            # is only the default case; MSVC doesn't like that
            if switch.nr_cases > 1:
                method.addstmt(switch)
            else:
                method.addstmt(StmtReturn(_Result.NotKnown))

            return method

        dispatches = (p.decl.type.isToplevel() and p.decl.type.isManager())
        self.cls.addstmts([
            makeHandlerMethod('OnMessageReceived', self.asyncSwitch,
                              hasReply=0, dispatches=dispatches),
            Whitespace.NL
        ])
        if p.decl.type.toplevel().talksSync():
            self.cls.addstmts([
                makeHandlerMethod('OnMessageReceived', self.syncSwitch,
                                  hasReply=1, dispatches=dispatches),
                Whitespace.NL
            ])
            if p.decl.type.toplevel().talksRpc():
                self.cls.addstmts([
                    makeHandlerMethod('OnCallReceived', self.rpcSwitch,
                                      hasReply=1, dispatches=dispatches),
                    Whitespace.NL
                ])

        # FIXME: only manager protocols and non-manager protocols with
        # union types need Lookup().  we'll give it to all for the
        # time being (simpler)
        if 1 or p.decl.type.isManager():
            self.cls.addstmts(self.implementManagerIface())

        # private: members and methods
        self.cls.addstmts([ Label.PRIVATE,
                            StmtDecl(Decl(p.channelType(), 'mChannel')) ])
        if p.decl.type.isToplevel():
            self.cls.addstmts([
                StmtDecl(Decl(Type('IDMap', T=Type('ChannelListener')),
                              p.actorMapVar().name)),
                StmtDecl(Decl(_actorIdType(), p.lastActorIdVar().name)),
                StmtDecl(Decl(Type('ProcessHandle'),
                              p.otherProcessVar().name))
            ])
        elif p.decl.type.isManaged():
            self.cls.addstmts([
                StmtDecl(Decl(_actorIdType(), p.idVar().name)),
                StmtDecl(Decl(p.managerCxxType(ptr=1), p.managerVar().name))
            ])

        self.ns.addstmts([ self.cls, Whitespace.NL, Whitespace.NL ])

        # generate skeleton implementation of abstract actor class
        self.file.addthing(CppDirective('if', '0'))

        genskeleton = _GenerateSkeletonImpl()
        genskeleton.fromclass(self.cls)
        self.file.addthings(genskeleton.stuff)

        self.file.addthing(CppDirective('endif', '// if 0'))


    def implementManagerIface(self):
        p = self.protocol
        routedvar = ExprVar('aRouted')
        idvar = ExprVar('aId')
        listenertype = Type('ChannelListener', ptr=1)

        register = MethodDefn(MethodDecl(
            p.registerMethod().name,
            params=[ Decl(listenertype, routedvar.name) ],
            ret=_actorIdType(), virtual=1))
        registerid = MethodDefn(MethodDecl(
            p.registerIDMethod().name,
            params=[ Decl(listenertype, routedvar.name),
                     Decl(_actorIdType(), idvar.name) ],
            ret=_actorIdType(),
            virtual=1))
        lookup = MethodDefn(MethodDecl(
            p.lookupIDMethod().name,
            params=[ Decl(_actorIdType(), idvar.name) ],
            ret=listenertype, virtual=1))
        unregister = MethodDefn(MethodDecl(
            p.unregisterMethod().name,
            params=[ Decl(_actorIdType(), idvar.name) ],
            virtual=1))
        otherprocess = MethodDefn(MethodDecl(
            p.otherProcessMethod().name,
            ret=Type('ProcessHandle'),
            virtual=1))

        if p.decl.type.isToplevel():
            tmpvar = ExprVar('tmp')
            register.addstmts([
                StmtDecl(Decl(_actorIdType(), tmpvar.name),
                         p.nextActorIdExpr(self.side)),
                StmtExpr(ExprCall(
                    ExprSelect(p.actorMapVar(), '.', 'AddWithID'),
                    [ routedvar, tmpvar ])),
                StmtReturn(tmpvar)
            ])
            registerid.addstmts([
                StmtExpr(
                    ExprCall(ExprSelect(p.actorMapVar(), '.', 'AddWithID'),
                             [ routedvar, idvar ])),
                StmtReturn(idvar)
            ])
            lookup.addstmt(StmtReturn(
                ExprCall(ExprSelect(p.actorMapVar(), '.', 'Lookup'),
                         [ idvar ])))
            unregister.addstmt(StmtReturn(
                ExprCall(ExprSelect(p.actorMapVar(), '.', 'Remove'),
                         [ idvar ])))
            otherprocess.addstmt(StmtReturn(p.otherProcessVar()))
        # delegate registration to manager
        else:
            register.addstmt(StmtReturn(ExprCall(
                ExprSelect(p.managerVar(), '->', p.registerMethod().name),
                [ routedvar ])))
            registerid.addstmt(StmtReturn(ExprCall(
                ExprSelect(p.managerVar(), '->', p.registerIDMethod().name),
                [ routedvar, idvar ])))
            lookup.addstmt(StmtReturn(ExprCall(
                ExprSelect(p.managerVar(), '->', p.lookupIDMethod().name),
                [ idvar ])))
            unregister.addstmt(StmtReturn(ExprCall(
                ExprSelect(p.managerVar(), '->', p.unregisterMethod().name),
                [ idvar ])))
            otherprocess.addstmt(StmtReturn(ExprCall(
                ExprSelect(p.managerVar(), '->',
                           p.otherProcessMethod().name))))

        return [ register,
                 registerid,
                 lookup,
                 unregister,
                 otherprocess,
                 Whitespace.NL ]



    ##-------------------------------------------------------------------------
    ## The next few functions are the crux of the IPDL code generator.
    ## They generate code for all the nasty work of message
    ## serialization/deserialization and dispatching handlers for
    ## received messages.
    ##
    def visitMessageDecl(self, md):
        isctor = md.decl.type.isCtor()
        isdtor = md.decl.type.isDtor()
        sems = md.decl.type.sendSemantics
        sendmethod = None
        helpermethod = None
        recvlbl, recvcase = None, None

        def addRecvCase(lbl, case):
            if sems is ipdl.ast.ASYNC:
                self.asyncSwitch.addcase(lbl, case)
            elif sems is ipdl.ast.SYNC:
                self.syncSwitch.addcase(lbl, case)
            elif sems is ipdl.ast.RPC:
                self.rpcSwitch.addcase(lbl, case)
            else: assert 0

        if self.sendsMessage(md):
            isasync = (sems is ipdl.ast.ASYNC)

            if isctor:
                self.cls.addstmts([ self.genHelperCtor(md), Whitespace.NL ])

            if isctor and isasync:
                sendmethod, (recvlbl, recvcase) = self.genAsyncCtor(md)
            elif isctor:
                sendmethod = self.genBlockingCtorMethod(md)
            elif isdtor and isasync:
                sendmethod, (recvlbl, recvcase) = self.genAsyncDtor(md)
            elif isdtor:
                sendmethod = self.genBlockingDtorMethod(md)
            elif isasync:
                sendmethod = self.genAsyncSendMethod(md)
            else:
                sendmethod = self.genBlockingSendMethod(md)
        if sendmethod is not None:
            self.cls.addstmts([ sendmethod, Whitespace.NL ])
        if recvcase is not None:
            addRecvCase(recvlbl, recvcase)
            recvlbl, recvcase = None, None

        if self.receivesMessage(md):
            if isctor:
                recvlbl, recvcase = self.genCtorRecvCase(md)
            elif isdtor:
                recvlbl, recvcase = self.genDtorRecvCase(md)
            else:
                recvlbl, recvcase = self.genRecvCase(md)
            addRecvCase(recvlbl, recvcase)


    def genAsyncCtor(self, md):
        actor = md.actorDecl()
        method = MethodDefn(self.makeSendMethodDecl(md))
        method.addstmts(self.ctorPrologue(md) + [ Whitespace.NL ])

        msgvar, stmts = self.makeMessage(md)
        sendok, sendstmts = self.sendAsync(md, msgvar)
        method.addstmts(
            stmts
            + sendstmts
            + self.failCtorIf(md, ExprNot(sendok))
            + [ StmtReturn(actor.var()) ])

        lbl = CaseLabel(md.pqReplyId())
        case = StmtBlock()
        case.addstmt(StmtReturn(_Result.Processed))
        # TODO not really sure what to do with async ctor "replies" yet.
        # destroy actor if there was an error?  tricky ...

        return method, (lbl, case)


    def genBlockingCtorMethod(self, md):
        actor = md.actorDecl()
        method = MethodDefn(self.makeSendMethodDecl(md))
        method.addstmts(self.ctorPrologue(md) + [ Whitespace.NL ])

        msgvar, stmts = self.makeMessage(md)

        replyvar = self.replyvar
        sendok, sendstmts = self.sendBlocking(md, msgvar, replyvar)
        method.addstmts(
            stmts
            + [ Whitespace.NL,
                StmtDecl(Decl(Type('Message'), replyvar.name)) ]
            + sendstmts
            + self.failCtorIf(md, ExprNot(sendok)))

        readok, stmts = self.deserializeReply(
            md, ExprAddrOf(replyvar), self.side)
        method.addstmts(
            stmts
            + self.failCtorIf(md, ExprNot(readok))
            + [ StmtReturn(actor.var()) ])

        return method


    def ctorPrologue(self, md, errcode=ExprLiteral.NULL, idexpr=None):
        actorvar = md.actorDecl().var()

        if idexpr is None:
            idexpr = ExprCall(self.protocol.registerMethod(),
                              args=[ actorvar ])
        else:
            idexpr = ExprCall(self.protocol.registerIDMethod(),
                              args=[ actorvar, idexpr ])

        return [
            self.failIfNullActor(actorvar, errcode),
            StmtExpr(ExprAssn(_actorId(actorvar), idexpr)),
            StmtExpr(ExprAssn(_actorManager(actorvar), ExprVar.THIS)),
            StmtExpr(ExprAssn(_actorChannel(actorvar),
                              self.protocol.channelForSubactor())),
        ]

    def failCtorIf(self, md, cond):
        actorvar = md.actorDecl().var()
        failif = StmtIf(cond)
        failif.addifstmts(self.unregisterActor(actorvar)
                          + [ StmtExpr(ExprDelete(actorvar)),
                              StmtReturn(ExprLiteral.NULL) ])
        return [ failif ]

    def genHelperCtor(self, md):
        helperdecl = self.makeSendMethodDecl(md)
        helperdecl.params = helperdecl.params[1:]
        helper = MethodDefn(helperdecl)

        callctor = self.callAllocActor(md, retsems='out')
        helper.addstmt(StmtReturn(ExprCall(
            ExprVar(helperdecl.name), args=[ callctor ] + callctor.args)))
        return helper


    def genAsyncDtor(self, md):
        actor = md.actorDecl()
        method = MethodDefn(self.makeSendMethodDecl(md))

        method.addstmts(self.dtorPrologue(actor.var()))

        msgvar, stmts = self.makeMessage(md)
        sendok, sendstmts = self.sendAsync(md, msgvar)
        method.addstmts(
            stmts
            + sendstmts
            + [ Whitespace.NL ]
            + self.dtorEpilogue(md, actor.var(), retsems='out')
            + [ StmtReturn(sendok) ])

        lbl = CaseLabel(md.pqReplyId())
        case = StmtBlock()
        case.addstmt(StmtReturn(_Result.Processed))
        # TODO if the dtor is "inherently racy", keep the actor alive
        # until the other side acks

        return method, (lbl, case)


    def genBlockingDtorMethod(self, md):
        actor = md.actorDecl()
        method = MethodDefn(self.makeSendMethodDecl(md))

        method.addstmts(self.dtorPrologue(actor.var()))

        msgvar, stmts = self.makeMessage(md)

        replyvar = self.replyvar
        sendok, sendstmts = self.sendBlocking(md, msgvar, replyvar)
        method.addstmts(
            stmts
            + [ Whitespace.NL,
                StmtDecl(Decl(Type('Message'), replyvar.name)) ]
            + sendstmts)

        readok, destmts = self.deserializeReply(
            md, ExprAddrOf(replyvar), self.side)
        ifsendok = StmtIf(sendok)
        ifsendok.addifstmts(destmts)
        ifsendok.addifstmts([ Whitespace.NL,
                              StmtExpr(ExprAssn(sendok, readok, '&=')) ])

        method.addstmts(
            [ ifsendok ]
            + self.dtorEpilogue(md, actor.var(), retsems='out')
            + [ Whitespace.NL, StmtReturn(sendok) ])

        return method

    def dtorPrologue(self, actorexpr):
        return [ self.failIfNullActor(actorexpr), Whitespace.NL ]

    def dtorEpilogue(self, md, actorexpr, retsems):
        return (self.unregisterActor(actorexpr)
                + [ StmtExpr(self.callDeallocActor(md, retsems)) ])

    def genAsyncSendMethod(self, md):
        method = MethodDefn(self.makeSendMethodDecl(md))
        msgvar, stmts = self.makeMessage(md)
        sendok, sendstmts = self.sendAsync(md, msgvar)
        method.addstmts(stmts
                        +[ Whitespace.NL ]
                        + sendstmts
                        +[ StmtReturn(sendok) ])
        return method


    def genBlockingSendMethod(self, md):
        method = MethodDefn(self.makeSendMethodDecl(md))

        msgvar, serstmts = self.makeMessage(md)
        replyvar = self.replyvar

        sendok, sendstmts = self.sendBlocking(md, msgvar, replyvar)
        failif = StmtIf(ExprNot(sendok))
        failif.addifstmt(StmtReturn(ExprLiteral.FALSE))

        readok, desstmts = self.deserializeReply(
            md, ExprAddrOf(replyvar), self.side)

        method.addstmts(
            serstmts
            + [ Whitespace.NL,
                StmtDecl(Decl(Type('Message'), replyvar.name)) ]
            + sendstmts
            + [ failif ]
            + desstmts
            + [ Whitespace.NL,
                StmtReturn(readok) ])

        return method


    def genCtorRecvCase(self, md):
        lbl = CaseLabel(md.pqMsgId())
        case = StmtBlock()
        actorvar = md.actorDecl().var()

        actorhandle, readok, stmts = self.deserializeMessage(md, self.side)
        failif = StmtIf(ExprNot(readok))
        failif.addifstmt(StmtReturn(_Result.PayloadError))

        case.addstmts(
            stmts
            + [ failif, Whitespace.NL ]
            + [ StmtDecl(Decl(r.bareType(self.side), r.var().name))
                for r in md.returns ]
            # alloc the actor, register it under the foreign ID
            + [ StmtExpr(ExprAssn(
                actorvar,
                self.callAllocActor(md, retsems='in'))) ]
            + self.ctorPrologue(md, errcode=_Result.ValuError,
                                idexpr=_actorHId(actorhandle))
            + [ Whitespace.NL ]
            + self.invokeRecvHandler(md)
            + self.makeReply(md)
            + [ Whitespace.NL,
                StmtReturn(_Result.Processed) ])

        return lbl, case


    def genDtorRecvCase(self, md):
        lbl = CaseLabel(md.pqMsgId())
        case = StmtBlock()

        readok, stmts = self.deserializeMessage(md, self.side)
        failif = StmtIf(ExprNot(readok))
        failif.addifstmt(StmtReturn(_Result.PayloadError))

        case.addstmts(
            stmts
            + [ failif, Whitespace.NL ]
            + [ StmtDecl(Decl(r.bareType(self.side), r.var().name))
                for r in md.returns ]
            + self.invokeRecvHandler(md)
            + [ Whitespace.NL ]
            + self.dtorEpilogue(md, md.actorDecl().var(), retsems='in')
            + self.makeReply(md)
            + [ Whitespace.NL,
                StmtReturn(_Result.Processed) ])
        
        return lbl, case


    def genRecvCase(self, md):
        lbl = CaseLabel(md.pqMsgId())
        case = StmtBlock()

        readok, stmts = self.deserializeMessage(md, self.side)
        failif = StmtIf(ExprNot(readok))
        failif.addifstmt(StmtReturn(_Result.PayloadError))

        case.addstmts(
            stmts
            + [ failif, Whitespace.NL ]
            + [ StmtDecl(Decl(r.bareType(self.side), r.var().name))
                for r in md.returns ]
            + self.invokeRecvHandler(md)
            + [ Whitespace.NL ]
            + self.makeReply(md)
            + [ StmtReturn(_Result.Processed) ])

        return lbl, case


    # helper methods

    def failIfNullActor(self, actorExpr, retOnNull=ExprLiteral.FALSE):
        failif = StmtIf(ExprNot(actorExpr))
        failif.addifstmt(StmtReturn(retOnNull))
        return failif

    def unregisterActor(self, actorexpr):
        return [ StmtExpr(ExprCall(self.protocol.unregisterMethod(),
                                   args=[ _actorId(actorexpr) ])) ]


    def makeMessage(self, md):
        msgvar = self.msgvar
        stmts = [ StmtDecl(Decl(Type(md.pqMsgClass(), ptr=1), msgvar.name)),
                  Whitespace.NL ]
        msgCtorArgs = [ ]        

        for param in md.params:
            arg, sstmts = param.serialize(param.var(), self.side)
            msgCtorArgs.append(arg)
            stmts.extend(sstmts)

        stmts.extend([
            StmtExpr(ExprAssn(
                msgvar,
                ExprNew(Type(md.pqMsgClass()), args=msgCtorArgs))) ]
            + self.setMessageFlags(md, msgvar, reply=0))

        return msgvar, stmts


    def makeReply(self, md):
        # TODO special cases for async ctor/dtor replies
        if md.decl.type.isAsync():
            return [ ]

        replyvar = self.replyvar
        stmts = [ ]
        replyCtorArgs = [ ]
        for ret in md.returns:
            arg, sstmts = ret.serialize(ret.var(), self.side)
            replyCtorArgs.append(arg)
            stmts.extend(sstmts)

        stmts.extend([
            StmtExpr(ExprAssn(
                replyvar,
                ExprNew(Type(md.pqReplyClass()), args=replyCtorArgs))) ]
            + self.setMessageFlags(md, replyvar, reply=1)
            +[ self.logMessage(md, md.replyCast(replyvar), 'Sending reply ') ])
        
        return stmts


    def setMessageFlags(self, md, var, reply):
        stmts = [ StmtExpr(ExprCall(
            ExprSelect(var, '->', 'set_routing_id'),
            args=[ self.protocol.routingId() ])) ]

        if md.decl.type.isSync():
            stmts.append(StmtExpr(ExprCall(
                ExprSelect(var, '->', 'set_sync'))))
        elif md.decl.type.isRpc():
            stmts.append(StmtExpr(ExprCall(
                ExprSelect(var, '->', 'set_rpc'))))

        if reply:
            stmts.append(StmtExpr(ExprCall(
                ExprSelect(var, '->', 'set_reply'))))

        return stmts + [ Whitespace.NL ]


    def deserializeMessage(self, md, side):
        msgvar = self.msgvar
        isctor = md.decl.type.isCtor()
        vars = [ ]
        readvars = [ ]
        stmts = [
            self.logMessage(md, md.msgCast(ExprAddrOf(msgvar)),
                            'Received '),
            Whitespace.NL
        ]
        for param in md.params:
            var, declstmts = param.makeDeserializedDecls(self.side)
            vars.append(var)
            stmts.extend(declstmts)

            fake, readvar, tempdeclstmts = param.makePipeDecls(var)
            readvars.append(readvar)
            stmts.extend(tempdeclstmts)

        okvar = ExprVar('__readok')
        stmts.append(
            StmtDecl(Decl(Type.BOOL, okvar.name),
                     ExprCall(ExprVar(md.pqMsgClass() +'::Read'),
                              args=[ ExprAddrOf(msgvar) ]
                              + [ ExprAddrOf(p) for p in readvars ])))

        ifok = StmtIf(okvar)
        for i, param in enumerate(md.params):
            # skip deserializing the "implicit" actor for ctor
            # in-messages; the actor doesn't exist yet
            if isctor and i is 0: continue
            ifok.addifstmts(param.deserialize(readvars[i], side, sems='in'))
        if len(ifok.ifb.stmts):
            stmts.extend([ Whitespace.NL, ifok ])

        if isctor:
            # return the raw actor handle so that its ID can be used
            # to construct the "real" actor
            return readvars[0], okvar, stmts
        return okvar, stmts


    def deserializeReply(self, md, replyexpr, side):
        readexprs = [ ]
        stmts = [ Whitespace.NL,
                  self.logMessage(md, md.replyCast(replyexpr),
                                  'Received reply '),
                  Whitespace.NL ]
        for ret in md.returns:
            fake, readvar, declstmts = ret.makePipeDecls(ret.var())
            if fake:
                readexprs.append(ExprAddrOf(readvar))
            else:
                readexprs.append(readvar)
            stmts.extend(declstmts)

        okvar = ExprVar('__readok')
        stmts.append(
            StmtDecl(Decl(Type.BOOL, okvar.name),
                     ExprCall(ExprVar(md.pqReplyClass() +'::Read'),
                              args=[ replyexpr ]
                              + readexprs)))

        ifok = StmtIf(okvar)
        for i, ret in enumerate(md.returns):
            ifok.addifstmts(ret.deserialize(
                ExprDeref(readexprs[i]), side, sems='out'))
        if len(ifok.ifb.stmts):
            stmts.extend([ Whitespace.NL, ifok ])

        return okvar, stmts


    def sendAsync(self, md, msgexpr):
        sendok = ExprVar('__sendok')
        return (
            sendok,
            [ Whitespace.NL,
              self.logMessage(md, msgexpr, 'Sending '),
              Whitespace.NL,
              StmtDecl(Decl(Type.BOOL, sendok.name),
                       init=ExprCall(
                           ExprSelect(self.protocol.channelVar(),
                                      self.protocol.channelSel(), 'Send'),
                           args=[ msgexpr ]))
            ])

    def sendBlocking(self, md, msgexpr, replyexpr):
        sendok = ExprVar('__sendok')
        return (
            sendok,
            [ Whitespace.NL,
              self.logMessage(md, msgexpr, 'Sending '),
              Whitespace.NL,
              StmtDecl(Decl(Type.BOOL, sendok.name),
                       init=ExprCall(ExprSelect(self.protocol.channelVar(),
                                                self.protocol.channelSel(),
                                                _sendPrefix(md.decl.type)),
                                     args=[ msgexpr,
                                            ExprAddrOf(replyexpr) ]))
            ])
                                            

    def callAllocActor(self, md, retsems):
        return ExprCall(
            md.allocMethod(),
            args=md.makeCxxArgs(params=1, retsems=retsems, retcallsems='out',
                                implicit=0))

    def callDeallocActor(self, md, retsems):
        return ExprCall(
            md.deallocMethod(),
            args=md.makeCxxArgs(params=1, retsems=retsems, retcallsems='out'))

    def invokeRecvHandler(self, md):
        failif = StmtIf(ExprNot(
            ExprCall(md.recvMethod(),
                     args=md.makeCxxArgs(params=1,
                                         retsems='in', retcallsems='out'))))
        failif.addifstmt(StmtReturn(_Result.ValuError))
        return [ failif ]

    def makeSendMethodDecl(self, md):
        implicit = md.decl.type.hasImplicitActorParam()
        decl = MethodDecl(
            md.sendMethod().name,
            params=md.makeCxxParams(paramsems='in', returnsems='out',
                                    side=self.side, implicit=implicit),
            ret=Type.BOOL)
        if md.decl.type.isCtor():
            decl.ret = md.actorDecl().bareType(self.side)
        return decl

    def logMessage(self, md, msgptr, pfx):
        return _ifLogging([
            StmtExpr(ExprCall(
                ExprSelect(msgptr, '->', 'Log'),
                args=[ ExprLiteral.String('['+ self.protocol.name +'] '+ pfx),
                       ExprVar('stderr') ])) ])


class _GenerateProtocolParentHeader(_GenerateProtocolActorHeader):
    def __init__(self):
        _GenerateProtocolActorHeader.__init__(self, 'parent')

    def sendsMessage(self, md):
        return not md.decl.type.isIn()

    def receivesMessage(self, md):
        return md.decl.type.isInout() or md.decl.type.isIn()

class _GenerateProtocolChildHeader(_GenerateProtocolActorHeader):
    def __init__(self):
        _GenerateProtocolActorHeader.__init__(self, 'child')

    def sendsMessage(self, md):
        return not md.decl.type.isOut()

    def receivesMessage(self, md):
        return md.decl.type.isInout() or md.decl.type.isOut()


##-----------------------------------------------------------------------------
## Bonus (inessential) passes
##

class _GenerateSkeletonImpl(Visitor):
    def __init__(self, name='ActorImpl'):
        self.name = name
        self.stuff = [ ]
        self.cls = None
        self.methodimpls = [ ]

    def fromclass(self, cls):
        cls.accept(self)
        self.stuff.append(Whitespace('''
//-----------------------------------------------------------------------------
// Skeleton implementation of abstract actor class

'''))
        self.stuff.append(Whitespace('// Header file contents\n'))
        self.stuff.append(self.cls)

        self.stuff.append(Whitespace.NL)
        self.stuff.append(Whitespace('\n// C++ file contents\n'))
        self.stuff.extend(self.methodimpls)

    def visitClass(self, cls):
        self.cls = Class(self.name, inherits=[ Inherit(Type(cls.name)) ])
        Visitor.visitClass(self, cls)

    def visitMethodDecl(self, md):
        if not md.pure:
            return
        decl = deepcopy(md)
        decl.pure = 0
        impl = MethodDefn(MethodDecl(self.implname(md.name),
                                             params=md.params,
                                             ret=md.ret))
        if md.ret.ptr:
            impl.addstmt(StmtReturn(ExprLiteral.ZERO))
        else:
            impl.addstmt(StmtReturn(ExprVar('false')))

        self.cls.addstmts([ StmtDecl(decl), Whitespace.NL ])
        self.addmethodimpl(impl)

    def visitConstructorDecl(self, cd):
        self.cls.addstmt(StmtDecl(ConstructorDecl(self.name)))
        ctor = ConstructorDefn(ConstructorDecl(self.implname(self.name)))
        ctor.addstmt(StmtExpr(ExprCall(ExprVar( 'MOZ_COUNT_CTOR'),
                                               [ ExprVar(self.name) ])))
        self.addmethodimpl(ctor)
        
    def visitDestructorDecl(self, dd):
        self.cls.addstmt(
            StmtDecl(DestructorDecl(self.name, virtual=1)))
        # FIXME/cjones: hack!
        dtor = DestructorDefn(ConstructorDecl(self.implname('~' +self.name)))
        dtor.addstmt(StmtExpr(ExprCall(ExprVar( 'MOZ_COUNT_DTOR'),
                                               [ ExprVar(self.name) ])))
        self.addmethodimpl(dtor)

    def addmethodimpl(self, impl):
        self.methodimpls.append(impl)
        self.methodimpls.append(Whitespace.NL)

    def implname(self, method):
        return self.name +'::'+ method
