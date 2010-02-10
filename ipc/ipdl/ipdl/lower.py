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
        '''returns |[ header: File ], [ cpp : File ]| representing the
lowered form of |tu|'''
        # annotate the AST with IPDL/C++ IR-type stuff used later
        tu.accept(_DecorateWithCxxStuff())

        pname = tu.protocol.name

        pheader = File(pname +'.h')
        _GenerateProtocolHeader().lower(tu, pheader)

        parentheader, parentcpp = File(pname +'Parent.h'), File(pname +'Parent.cpp')
        _GenerateProtocolParentCode().lower(
            tu, pname+'Parent', parentheader, parentcpp)

        childheader, childcpp = File(pname +'Child.h'), File(pname +'Child.cpp')
        _GenerateProtocolChildCode().lower(
            tu, pname+'Child', childheader, childcpp)

        return [ pheader, parentheader, childheader ], [ parentcpp, childcpp ]


##-----------------------------------------------------------------------------
## Helper code
##

_NULL_ACTOR_ID = ExprLiteral.ZERO
_FREED_ACTOR_ID = ExprLiteral.ONE

class _struct: pass

def _protocolHeaderName(p, side=''):
    if side: side = side.title()
    base = p.name + side

    
    pfx = '/'.join([ ns.name for ns in p.namespaces ])
    if pfx: return pfx +'/'+ base
    else:   return base

def _includeGuardMacroName(headerfile):
    return re.sub(r'[./]', '_', headerfile.name)

def _includeGuardStart(headerfile):
    guard = _includeGuardMacroName(headerfile)
    return [ CppDirective('ifndef', guard),
             CppDirective('define', guard)  ]

def _includeGuardEnd(headerfile):
    guard = _includeGuardMacroName(headerfile)
    return [ CppDirective('endif', '// ifndef '+ guard) ]

def _messageStartName(ptype):
    return ptype.name() +'MsgStart'

def _protocolId(ptype):
    return ExprVar(_messageStartName(ptype))

def _protocolIdType():
    return Type('int32')

def _actorName(pname, side):
    """|pname| is the protocol name. |side| is 'Parent' or 'Child'."""
    tag = side
    if not tag[0].isupper():  tag = side.title()
    return pname + tag

def _actorIdType():
    return Type('int32')

def _actorId(actor):
    return ExprSelect(actor, '->', 'mId')

def _actorHId(actorhandle):
    return ExprSelect(actorhandle, '.', 'mId')

def _actorChannel(actor):
    return ExprSelect(actor, '->', 'mChannel')

def _actorManager(actor):
    return ExprSelect(actor, '->', 'mManager')

def _getActorId(actorexpr, outid, actortype, errfn):
    # if (!actorexpr)
    #   #ifdef NULLABLE
    #     abort()
    #   #else
    #     outid = 0;
    #   #endif
    # else if (id == FREED)
    #     abort()
    # else
    #     outid = _actorId(actorexpr)
    ifnull = StmtIf(ExprNot(actorexpr))
    if not actortype.nullable:
        ifnull.addifstmts(
            errfn("NULL actor value passed to non-nullable param"))
    else:
        ifnull.addifstmt(StmtExpr(ExprAssn(outid, ExprLiteral.ZERO)))

    iffreed = StmtIf(ExprBinary(_FREED_ACTOR_ID, '==', _actorId(actorexpr)))
    ifnull.addelsestmt(iffreed)

    # this is always a hard-abort, because it means that some C++ code
    # has a live pointer to a freed actor, so we're playing Russian
    # roulette with invalid memory
    iffreed.addifstmt(_runtimeAbort("actor has been delete'd"))
    iffreed.addelsestmt(StmtExpr(ExprAssn(outid, _actorId(actorexpr))))

    return ifnull


def _lookupActor(idexpr, outactor, actortype, cxxactortype, errfn):
    # if (NULLID == idexpr)
    #   #ifndef NULLABLE
    #     abort()
    #   #else
    #     actor = 0;
    #   #endif
    # else if (FREEDID == idexpr)
    #     abort()
    # else {
    #     actor = (cxxactortype*)_lookupListener(idexpr);
    #     // bad actor ID.  always an error
    #     if (!actor) abort();
    # }
    ifzero = StmtIf(ExprBinary(_NULL_ACTOR_ID, '==', idexpr))
    if not actortype.nullable:
        ifzero.addifstmts(errfn("NULL actor ID for non-nullable param"))
    else:
        ifzero.addifstmt(StmtExpr(ExprAssn(outactor, ExprLiteral.NULL)))

    iffreed = StmtIf(ExprBinary(_FREED_ACTOR_ID, '==', idexpr))
    ifzero.addelsestmt(iffreed)

    iffreed.addifstmts(errfn("received FREED actor ID, evidence that the other side is malfunctioning"))
    iffreed.addelsestmt(
        StmtExpr(ExprAssn(
            outactor,
            ExprCast(_lookupListener(idexpr), cxxactortype, static=1))))

    ifnotactor = StmtIf(ExprNot(outactor))
    ifnotactor.addifstmts(errfn("invalid actor ID, evidence that the other side is malfunctioning"))
    iffreed.addelsestmt(ifnotactor)

    return ifzero


def _lookupActorHandle(handle, outactor, actortype, cxxactortype, errfn):
    return _lookupActor(_actorHId(handle), outactor, actortype, cxxactortype,
                        errfn)

def _lookupListener(idexpr):
    return ExprCall(ExprVar('Lookup'), args=[ idexpr ])

def _shmemType(ptr=0):
    return Type('Shmem', ptr=ptr)

def _rawShmemType(ptr=0):
    return Type('Shmem::SharedMemory', ptr=ptr)

def _shmemIdType():
    return Type('Shmem::id_t')

def _shmemHandleType():
    return Type('Shmem::SharedMemoryHandle')

def _shmemBackstagePass():
    return ExprCall(ExprVar(
        'Shmem::IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead'))

def _shmemCtor(rawmem, idexpr):
    return ExprCall(ExprVar('Shmem'),
                    args=[ _shmemBackstagePass(), rawmem, idexpr ])

def _shmemId(shmemexpr):
    return ExprCall(ExprSelect(shmemexpr, '.', 'Id'),
                    args=[ _shmemBackstagePass() ])

def _shmemAlloc(size):
    # starts out UNprotected
    return ExprCall(ExprVar('Shmem::Alloc'),
                    args=[ _shmemBackstagePass(), size ])

def _shmemOpenExisting(size, handle):
    # starts out protected
    return ExprCall(ExprVar('Shmem::OpenExisting'),
                    args=[ _shmemBackstagePass(),
                           # true => protect
                           handle, size, ExprLiteral.TRUE ])

def _shmemForget(shmemexpr):
    return ExprCall(ExprSelect(shmemexpr, '.', 'forget'),
                    args=[ _shmemBackstagePass() ])

def _shmemRevokeRights(shmemexpr):
    return ExprCall(ExprSelect(shmemexpr, '.', 'RevokeRights'),
                    args=[ _shmemBackstagePass() ])

def _shmemCreatedMsgVar():
    return ExprVar('mozilla::ipc::__internal__ipdl__ShmemCreated')

def _lookupShmem(idexpr):
    return ExprCall(ExprVar('LookupShmem'), args=[ idexpr ])

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

def _autoptr(T):
    return Type('nsAutoPtr', T=T)

def _autoptrForget(expr):
    return ExprCall(ExprSelect(expr, '.', 'forget'))

def _cxxArrayType(basetype, const=0, ref=0):
    return Type('nsTArray', T=basetype, const=const, ref=ref)

def _callCxxArrayLength(arr):
    return ExprCall(ExprSelect(arr, '.', 'Length'))

def _callCxxArraySetLength(arr, lenexpr):
    return ExprCall(ExprSelect(arr, '.', 'SetLength'),
                    args=[ lenexpr ])

def _callCxxArrayInsertSorted(arr, elt):
    return ExprCall(ExprSelect(arr, '.', 'InsertElementSorted'),
                    args=[ elt ])

def _callCxxArrayRemoveSorted(arr, elt):
    return ExprCall(ExprSelect(arr, '.', 'RemoveElementSorted'),
                    args=[ elt ])

def _callCxxArrayClear(arr):
    return ExprCall(ExprSelect(arr, '.', 'Clear'))

def _cxxArrayHasElementSorted(arr, elt):
    return ExprBinary(
        ExprVar('nsTArray_base::NoIndex'), '!=',
        ExprCall(ExprSelect(arr, '.', 'BinaryIndexOf'), args=[ elt ]))

def _otherSide(side):
    if side == 'child':  return 'parent'
    if side == 'parent':  return 'child'
    assert 0

def _ifLogging(stmts):
    iflogging = StmtIf(ExprCall(ExprVar('mozilla::ipc::LoggingEnabled')))
    iflogging.addifstmts(stmts)
    return iflogging

# XXX we need to remove these and install proper error handling
def _printErrorMessage(msg):
    if isinstance(msg, str):
        msg = ExprLiteral.String(msg)
    return StmtExpr(
        ExprCall(ExprVar('NS_ERROR'), args=[ msg ]))

def _fatalError(msg):
    return StmtExpr(
        ExprCall(ExprVar('FatalError'), args=[ ExprLiteral.String(msg) ]))

def _killProcess(pid):
    return ExprCall(
        ExprVar('base::KillProcess'),
        args=[ pid,
               # XXX this is meaningless on POSIX
               ExprVar('base::PROCESS_END_KILLED_BY_USER'),
               ExprLiteral.FALSE ])

# Results that IPDL-generated code returns back to *Channel code.
# Users never see these
class _Result:
    @staticmethod
    def Type():
        return Type('Result')

    Processed = ExprVar('MsgProcessed')
    NotKnown = ExprVar('MsgNotKnown')
    NotAllowed = ExprVar('MsgNotAllowed')
    PayloadError = ExprVar('MsgPayloadError')
    RouteError = ExprVar('MsgRouteError')
    ValuError = ExprVar('MsgValueError') # [sic]

# these |errfn*| are functions that generate code to be executed on an
# error, such as "bad actor ID".  each is given a Python string
# containing a description of the error

# used in user-facing Send*() methods
def errfnSend(msg, errcode=ExprLiteral.FALSE):
    return [
        _fatalError(msg),
        StmtReturn(errcode)
    ]

def errfnSendCtor(msg):  return errfnSend(msg, errcode=ExprLiteral.NULL)

# TODO should this error handling be strengthened for dtors?
def errfnSendDtor(msg):
    return [
        _printErrorMessage(msg),
        StmtReturn(ExprLiteral.FALSE)
    ]

# used in |OnMessage*()| handlers that hand in-messages off to Recv*()
# interface methods
def errfnRecv(msg, errcode=_Result.ValuError):
    return [
        _fatalError(msg),
        StmtReturn(errcode)
    ]

def _destroyMethod():
    return ExprVar('ActorDestroy')

class _DestroyReason:
    @staticmethod
    def Type():  return Type('ActorDestroyReason')

    Deletion = ExprVar('Deletion')
    AncestorDeletion = ExprVar('AncestorDeletion')
    NormalShutdown = ExprVar('NormalShutdown')
    AbnormalShutdown = ExprVar('AbnormalShutdown')

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

    def visitShmemType(self, s):
        return Type(s.name())

    def visitProtocolType(self, p): assert 0
    def visitMessageType(self, m): assert 0
    def visitVoidType(self, v): assert 0
    def visitStateType(self, st): assert 0

def _bareCxxType(ipdltype, side):
    return ipdltype.accept(_ConvertToCxxType(side))

def _allocMethod(ptype):
    return ExprVar('Alloc'+ ptype.name())

def _deallocMethod(ptype):
    return ExprVar('Dealloc'+ ptype.name())

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

    def visitShmemType(self, s):
        return Type(s.name())

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
        return _bareCxxType(self.ipdltype, side)

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
        if self.ipdltype.isIPDL() and self.ipdltype.isShmem():
            t.ref = 1
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

    def serialize(self, expr, side, errfn):
        if not self.speciallySerialized(self.ipdltype):
            return expr, [ ]
        # XXX could use TypeVisitor, but it doesn't feel right here
        _, sexpr, stmts = self._serialize(self.ipdltype, expr, side, errfn)
        return sexpr, stmts

    def _serialize(self, etype, expr, side, errfn):
        '''Serialize |expr| of type |etype|, which has some actor type
buried in it.  Return |pipetype, serializedExpr, serializationStmts|.'''
        assert etype.isIPDL()           # only IPDL types may contain actors

        if etype.isActor():
            return self._serializeActor(etype, expr, errfn)
        elif etype.isArray():
            return self._serializeArray(etype, expr, side, errfn)
        elif etype.isUnion():
            return self._serializeUnion(etype, expr, side, errfn)
        elif etype.isShmem() or etype.isChmod():
            return self._serializeShmem(etype, expr, side, errfn)
        else: assert 0

    def speciallySerialized(self, type):
        return ipdl.type.hasactor(type) or ipdl.type.hasshmem(type)

    def _serializeActor(self, actortype, expr, errfn):
        actorhandlevar = ExprVar(self._nextuid('handle'))
        pipetype = Type('ActorHandle')

        stmts = [
            Whitespace('// serializing actor type\n', indent=1),
            StmtDecl(Decl(pipetype, actorhandlevar.name)),
            _getActorId(expr, _actorHId(actorhandlevar), actortype, errfn),
            Whitespace.NL
        ]
        return pipetype, actorhandlevar, stmts


    def _serializeArray(self, arraytype, expr, side, errfn):
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
            arraytype.basetype, ithOldElt, side, errfn)

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


    def _serializeUnion(self, uniontype, expr, side, errfn):
        def insaneActorCast(actor, actortype, cxxactortype):
            idvar = ExprVar(self._nextuid('actorid'))
            return (
                [ StmtDecl(Decl(_actorIdType(), idvar.name)),
                  _getActorId(actor, idvar, actortype, errfn),
                ],
                ExprCast(ExprCast(idvar, Type.INTPTR, static=1),
                         cxxactortype,
                         reinterpret=1)
                )

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
                    getidstmts, castexpr = insaneActorCast(
                        getvalue, ct, c.bareType())
                    case.addstmts(
                        getidstmts
                        + [ StmtExpr(ExprAssn(serunionvar, castexpr)) ])

            elif ct.isArray() and ct.basetype.isActor():
                if c.side != side:
                    case.addstmt(_runtimeAbort('wrong side!'))
                else:
                    # no more apologies
                    cxxactortype = ct.basetype.accept(
                        _ConvertToCxxType(c.side))
                    lenvar = ExprVar(self._nextuid('len'))
                    newarrvar = ExprVar(self._nextuid('idarray'))

                    ivar = ExprVar(self._nextuid('i'))
                    ithOldElt = ExprIndex(getvalue, ivar)
                    ithNewElt = ExprIndex(newarrvar, ivar)
                    loop = StmtFor(init=ExprAssn(Decl(Type.UINT32, ivar.name),
                                          ExprLiteral.ZERO),
                                   cond=ExprBinary(ivar, '<', lenvar),
                                   update=ExprPrefixUnop(ivar, '++'))
                    # loop body
                    getidstmts, castexpr = insaneActorCast(
                        ithOldElt, ct.basetype, cxxactortype)
                    loop.addstmts(
                        getidstmts
                        + [ StmtExpr(ExprAssn(ithNewElt, castexpr)) ])

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
                _, newexpr, sstmts = self._serialize(ct, getvalue, side,
                                                     errfn)
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


    def _serializeShmem(self, shmemtype, expr, side, errfn):
        pipetype = _shmemType()
        pipevar = ExprVar(self._nextuid('serShmem'))
        stmts = [
            Whitespace('// serializing shmem type\n', indent=1),
            StmtDecl(Decl(_shmemType(), pipevar.name),
                     init=ExprCall(pipetype, args=[ expr ])),
            StmtExpr(_shmemRevokeRights(expr)),
            StmtExpr(_shmemForget(expr)),
        ]
        return pipetype, pipevar, stmts


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

    def speciallyDeserialized(self, type):
        return ipdl.type.hasactor(type) or ipdl.type.hasshmem(type)

    def deserialize(self, expr, side, sems, errfn):
        """|expr| is a pointer the return type."""
        if not self.speciallyDeserialized(self.ipdltype):
            return [ ]
        if sems == 'in':
            toexpr = self.var()
        elif sems == 'out':
            toexpr = ExprDeref(self.var())
        else: assert 0
        _, stmts = self._deserialize(
            expr, self.ipdltype, toexpr, side, errfn)
        return stmts

    def _deserialize(self, pipeExpr, targetType, targetExpr, side, errfn):
        if not self.speciallyDeserialized(targetType):
            return targetType, [ ]
        elif targetType.isActor():
            return self._deserializeActor(
                pipeExpr, targetType, targetExpr, side, errfn)
        elif targetType.isArray():
            return self._deserializeArray(
                pipeExpr, targetType, targetExpr, side, errfn)
        elif targetType.isUnion():
            return self._deserializeUnion(
                pipeExpr, targetType, targetExpr, side, errfn)
        elif targetType.isShmem():
            return self._deserializeShmem(
                pipeExpr, targetType, targetExpr, side, errfn)
        else: assert 0

    def _deserializeActor(self, actorhandle, actortype, outactor, side,
                          errfn):
        cxxtype = actortype.accept(_ConvertToCxxType(side))
        return (
            cxxtype,
            [ Whitespace('// deserializing actor type\n', indent=1),
              _lookupActorHandle(actorhandle, outactor, actortype, cxxtype,
                                 errfn)
            ])


    def _deserializeArray(self, pipearray, arraytype, outarray, side,
                          errfn):
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
            ithOldElt, arraytype.basetype, ithNewElt, side, errfn)
        forloop.addstmts(forstmts)

        stmts.append(forloop)

        return cxxArrayType, stmts


    def _deserializeUnion(self, pipeunion, uniontype, outunion, side,
                          errfn):
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
            if not self.speciallyDeserialized(ct):
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
                    outactorvar = ExprVar(self._nextuid('actor'))
                    actorcxxtype = c.bareType()
                    case.addstmts([
                        StmtDecl(Decl(_actorIdType(), idvar.name),
                                 actorIdCast(getvalue)),
                        StmtDecl(Decl(actorcxxtype, outactorvar.name)),
                        _lookupActor(idvar, outactorvar, ct, actorcxxtype,
                                     errfn),
                        StmtExpr(ExprAssn(outunion, outactorvar))
                    ])
            elif ct.isArray() and ct.basetype.isActor():
                if c.side != side:
                    case.addstmt(_runtimeAbort('wrong side!'))
                else:
                    idvar = ExprVar(self._nextuid('id'))
                    arrvar = ExprVar(self._nextuid('arr'))
                    ivar = ExprVar(self._nextuid('i'))
                    ithElt = ExprIndex(arrvar, ivar)
                    actortype = ct.basetype
                    actorcxxtype = ct.basetype.accept(_ConvertToCxxType(side))

                    loop = StmtFor(
                        init=ExprAssn(Decl(Type.UINT32, ivar.name),
                                      ExprLiteral.ZERO),
                        cond=ExprBinary(ivar, '<',
                                        _callCxxArrayLength(arrvar)),
                        update=ExprPrefixUnop(ivar, '++'))
                    loop.addstmts([
                        StmtDecl(Decl(_actorIdType(), idvar.name),
                                 actorIdCast(ithElt)),
                        _lookupActor(idvar, ithElt, actortype, actorcxxtype,
                                     errfn),
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
                    getvalue, ct, tempvar, side, errfn)
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


    def _deserializeShmem(self, pipeshmem, shmemtype, outshmem, side,
                          errfn):
        # Shmem::id_t id = inshmem.mId
        # Shmem::shmem_t* raw = Lookup(id)
        # if (raw)
        #   outshmem = Shmem(raw, id)
        idvar = ExprVar(self._nextuid('shmemid'))
        rawvar = ExprVar(self._nextuid('rawshmem'))
        iffound = StmtIf(rawvar)
        iffound.addifstmt(StmtExpr(ExprAssn(
            outshmem, _shmemCtor(rawvar, idvar))))

        cxxShmemType = _shmemType()
        stmts = [
            Whitespace('// deserializing shmem type\n', indent=1),
            StmtDecl(Decl(_shmemIdType(), idvar.name),
                     init=_shmemId(pipeshmem)),
            StmtDecl(Decl(_rawShmemType(ptr=1), rawvar.name),
                     init=_lookupShmem(idvar)),
            iffound
        ]
        return cxxShmemType, stmts


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
        return self.name
    
    def recvMethod(self):
        name = _recvPrefix(self.decl.type) + self.baseName()
        if self.decl.type.isCtor():
            name += 'Constructor'
        return ExprVar(name)

    def sendMethod(self):
        name = _sendPrefix(self.decl.type) + self.baseName()
        if self.decl.type.isCtor():
            name += 'Constructor'
        return ExprVar(name)

    def hasReply(self):
        return (self.decl.type.hasReply()
                or self.decl.type.isCtor()
                or self.decl.type.isDtor())

    def msgClass(self):
        return 'Msg_%s'% (self.decl.progname)

    def pqMsgClass(self):
        return '%s::%s'% (self.namespace, self.msgClass())

    def msgCast(self, msgexpr):
        return ExprCast(msgexpr, Type(self.pqMsgClass(), const=1, ptr=1),
                        static=1)

    def msgId(self):  return self.msgClass()+ '__ID'
    def pqMsgId(self):
        return '%s::%s'% (self.namespace, self.msgId())

    def replyClass(self):
        return 'Reply_%s'% (self.decl.progname)

    def pqReplyClass(self):
        return '%s::%s'% (self.namespace, self.replyClass())

    def replyCast(self, replyexpr):
        return ExprCast(replyexpr, Type(self.pqReplyClass(), const=1, ptr=1),
                        static=1)

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

    def managerInterfaceType(self, ptr=0):
        return Type('mozilla::ipc::IProtocolManager',
                    ptr=ptr,
                    T=Type(self.fqListenerName()))

    def _ipdlmgrtype(self):
        assert 1 == len(self.decl.type.managers)
        for mgr in self.decl.type.managers:  return mgr

    def managerActorType(self, side, ptr=0):
        return Type(_actorName(self._ipdlmgrtype().name(), side),
                    ptr=ptr)

    def managerMethod(self, actorThis=None):
        _ = self._ipdlmgrtype()
        if actorThis is not None:
            return ExprSelect(actorThis, '->', 'Manager')
        return ExprVar('Manager');

    # FIXME/bug 525181: implement
    def stateMethod(self):
        return ExprVar('state');

    def registerMethod(self):
        return ExprVar('Register')

    def registerIDMethod(self):
        return ExprVar('RegisterID')

    def lookupIDMethod(self):
        return ExprVar('Lookup')

    def unregisterMethod(self, actorThis=None):
        if actorThis is not None:
            return ExprSelect(actorThis, '->', 'Unregister')
        return ExprVar('Unregister')

    def removeManageeMethod(self):
        return ExprVar('RemoveManagee')

    def otherProcessMethod(self):
        return ExprVar('OtherProcess')

    def shouldContinueFromTimeoutVar(self):
        assert self.decl.type.isToplevel()
        return ExprVar('ShouldContinueFromReplyTimeout')

    def nextActorIdExpr(self, side):
        assert self.decl.type.isToplevel()
        if side is 'parent':   op = '++'
        elif side is 'child':  op = '--'
        else: assert 0
        return ExprPrefixUnop(self.lastActorIdVar(), op)

    def actorIdInit(self, side):
        assert self.decl.type.isToplevel()

        # parents go up from FREED, children go down from NULL
        if side is 'parent':  return _FREED_ACTOR_ID
        elif side is 'child': return _NULL_ACTOR_ID
        else: assert 0

    # an actor's C++ private variables
    def lastActorIdVar(self):
        assert self.decl.type.isToplevel()
        return ExprVar('mLastRouteId')

    def actorMapVar(self):
        assert self.decl.type.isToplevel()
        return ExprVar('mActorMap')

    def channelVar(self, actorThis=None):
        if actorThis is not None:
            return ExprSelect(actorThis, '->', 'mChannel')
        return ExprVar('mChannel')

    def channelForSubactor(self):
        if self.decl.type.isToplevel():
            return ExprAddrOf(self.channelVar())
        return self.channelVar()

    def routingId(self, actorThis=None):
        if self.decl.type.isToplevel():
            return ExprVar('MSG_ROUTING_CONTROL')
        if actorThis is not None:
            return ExprSelect(actorThis, '->', self.idVar().name)
        return self.idVar()

    def idVar(self):
        assert not self.decl.type.isToplevel()
        return ExprVar('mId')

    def managerVar(self, thisexpr=None):
        assert thisexpr is not None or not self.decl.type.isToplevel()
        mvar = ExprVar('mManager')
        if thisexpr is not None:
            mvar = ExprSelect(thisexpr, '->', mvar.name)
        return mvar

    def otherProcessVar(self):
        assert self.decl.type.isToplevel()
        return ExprVar('mOtherProcess')

    def managedCxxType(self, actortype, side):
        assert self.decl.type.isManagerOf(actortype)
        return Type(_actorName(actortype.name(), side), ptr=1)

    def managedMethod(self, actortype, side):
        assert self.decl.type.isManagerOf(actortype)
        return ExprVar('Managed'+  _actorName(actortype.name(), side))

    def managedVar(self, actortype, side):
        assert self.decl.type.isManagerOf(actortype)
        return ExprVar('mManaged'+ _actorName(actortype.name(), side))

    def managedVarType(self, actortype, side, const=0, ref=0):
        assert self.decl.type.isManagerOf(actortype)
        return _cxxArrayType(self.managedCxxType(actortype, side),
                             const=const, ref=ref)

    def managerArrayExpr(self, thisvar, side):
        """The member var my manager keeps of actors of my type."""
        assert self.decl.type.isManaged()
        return ExprSelect(
            ExprCall(self.managerMethod(thisvar)),
            '->', 'mManaged'+ _actorName(self.decl.type.name(), side))

    # shmem stuff
    def shmemMapVar(self):
        assert self.usesShmem()
        return ExprVar('mShmemMap')

    def lastShmemIdVar(self):
        assert self.usesShmem()
        return ExprVar('mLastShmemId')

    def shmemIdInit(self, side):
        assert self.usesShmem()
        # use the same scheme for shmem IDs as actor IDs
        if side is 'parent':  return _FREED_ACTOR_ID
        elif side is 'child': return _NULL_ACTOR_ID
        else: assert 0

    def nextShmemIdExpr(self, side):
        assert self.usesShmem()
        if side is 'parent':   op = '++'
        elif side is 'child':  op = '--'
        return ExprPrefixUnop(self.lastShmemIdVar(), op)

    def lookupShmemVar(self):
        assert self.usesShmem()
        return ExprVar('LookupShmem')

    def registerShmemVar(self):
        assert self.usesShmem()
        return ExprVar('RegisterShmem')

    def registerShmemIdVar(self):
        assert self.usesShmem()
        return ExprVar('RegisterShmemId')

    def unregisterShmemVar(self):
        assert self.usesShmem()
        return ExprVar('UnregisterShmem')

    def usesShmem(self):
        for md in self.messageDecls:
            for param in md.inParams:
                if ipdl.type.hasshmem(param.type):
                    return True
            for ret in md.outParams:
                if ipdl.type.hasshmem(ret.type):
                    return True
        return False

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
        self.typedefs = [ 
            Typedef(Type('mozilla::ipc::ActorHandle'), 'ActorHandle')
        ]
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
        msgstart = _messageStartName(self.protocol.decl.type) +' << 10'
        msgenum.addId(self.protocol.name + 'Start', msgstart)
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
            StmtDecl(Decl(Type.VOIDPTR, itervar.name),
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
        assertsanityvar.name, ret=Type.VOID, const=1))
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
                       ret=Type.VOID,
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

    def findFriends(self, ptype):
        self.mytype = ptype
        self.walkDownTheProtocolTree(ptype.toplevel())
        return self.friends

    # TODO could make this into a _iterProtocolTreeHelper ...
    def walkDownTheProtocolTree(self, ptype):
        if ptype != self.mytype:
            # don't want to |friend| ourself!
            self.visit(ptype)
        for mtype in ptype.manages:
            self.walkDownTheProtocolTree(mtype)

    def visit(self, ptype):
        # |vtype| is the type currently being visited
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


class _GenerateProtocolActorCode(ipdl.ast.Visitor):
    def __init__(self, myside):
        self.side = myside              # "parent" or "child"
        self.prettyside = myside.title()
        self.clsname = None
        self.protocol = None
        self.hdrfile = None
        self.cppfile = None
        self.ns = None
        self.cls = None
        self.includedActorTypedefs = [ ]
        self.protocolCxxIncludes = [ ]

    def lower(self, tu, clsname, cxxHeaderFile, cxxFile):
        self.clsname = clsname
        self.hdrfile = cxxHeaderFile
        self.cppfile = cxxFile
        tu.accept(self)

    def standardTypedefs(self):
        return [
            Typedef(Type('IPC::Message'), 'Message'),
            Typedef(Type(self.protocol.channelName()), 'Channel'),
            Typedef(Type(self.protocol.fqListenerName()), 'ChannelListener'),
            Typedef(Type('base::ProcessHandle'), 'ProcessHandle')
        ]


    def visitTranslationUnit(self, tu):
        self.protocol = tu.protocol

        hf = self.hdrfile
        cf = self.cppfile

        disclaimer = Whitespace('''//
// Automatically generated by ipdlc.
// Edit at your own risk
//

''')
        # make the C++ header
        hf.addthings(
            [ disclaimer ]
            + _includeGuardStart(hf)
            +[
                Whitespace.NL,
                CppDirective(
                    'include',
                    '"'+ _protocolHeaderName(tu.protocol) +'.h"')
            ])

        for pinc in tu.protocolIncludes:
            pinc.accept(self)

        # this generates the actor's full impl in self.cls
        tu.protocol.accept(self)

        clsdecl, clsdefn = _ClassDeclDefn().split(self.cls)

        # XXX damn C++ ... return types in the method defn aren't in
        # class scope
        for stmt in clsdefn.stmts:
            if isinstance(stmt, MethodDefn):
                if stmt.decl.ret and stmt.decl.ret.name == 'Result':
                    stmt.decl.ret.name = clsdecl.name +'::'+ stmt.decl.ret.name

        def makeNamespace(p, file):
            if 0 == len(p.namespaces):
                return file
            ns = Namespace(p.namespaces[-1].name)
            outerns = _putInNamespaces(ns, p.namespaces[:-1])
            file.addthing(outerns)
            return ns

        hdrns = makeNamespace(self.protocol, self.hdrfile)
        hdrns.addstmts([
            Whitespace.NL,
            Whitespace.NL,
            clsdecl,
            Whitespace.NL,
            Whitespace.NL
        ])

        self.hdrfile.addthings(
            ([
                Whitespace.NL,
                CppDirective('if', '0') ])
            + _GenerateSkeletonImpl(
                _actorName(self.protocol.name, self.side)[1:],
                self.protocol.namespaces).fromclass(self.cls)
            +([
                CppDirective('endif', '// if 0'),
                Whitespace.NL ])
            + _includeGuardEnd(hf))

        # make the .cpp file
        cf.addthings([
            disclaimer,
            Whitespace.NL,
            CppDirective(
                'include',
                '"'+ _protocolHeaderName(self.protocol, self.side) +'.h"')
        ])
             
        if self.protocol.decl.type.isToplevel():
            cf.addthings([
                CppDirective('ifdef', 'MOZ_CRASHREPORTER'),
                CppDirective('  include', '"nsXULAppAPI.h"'),
                CppDirective('endif')
            ])

        cf.addthings((
            [ Whitespace.NL ]
            + self.protocolCxxIncludes
            + [ Whitespace.NL ]
            + self.standardTypedefs()
            + self.includedActorTypedefs
            + tu.protocol.decl.cxxtypedefs
            + [ Whitespace.NL ]))

        cppns = makeNamespace(self.protocol, cf)
        cppns.addstmts([
            Whitespace.NL,
            Whitespace.NL,
            clsdefn,
            Whitespace.NL,
            Whitespace.NL
        ])


    def visitProtocolInclude(self, pi):
        ip = pi.tu.protocol

        self.hdrfile.addthings([
            _makeForwardDecl(ip.decl.type, self.side),
            Whitespace.NL
        ])
        self.protocolCxxIncludes.append(
            CppDirective(
                'include',
                '"%s.h"'% (_protocolHeaderName(ip, self.side))))

        if ip.decl.fullname is not None:
            self.includedActorTypedefs.append(Typedef(
                Type(_actorName(ip.decl.fullname, self.prettyside)),
                _actorName(ip.decl.shortname, self.prettyside)))


    def visitProtocol(self, p):
        self.hdrfile.addthings([
            CppDirective('ifdef', 'DEBUG'),
            CppDirective('include', '"prenv.h"'),
            CppDirective('endif', '// DEBUG')
        ])

        self.protocol = p
        ptype = p.decl.type
        toplevel = p.decl.type.toplevel()

        # FIXME: all actors impl Iface for now
        if ptype.isManager() or 1:
            self.hdrfile.addthing(CppDirective('include', '"base/id_map.h"'))

        self.hdrfile.addthings([
            CppDirective('include', '"'+ p.channelHeaderFile() +'"'),
            Whitespace.NL ])

        self.cls = Class(
            self.clsname,
            inherits=[ Inherit(Type(p.fqListenerName()), viz='protected'),
                       Inherit(p.managerInterfaceType(), viz='protected') ],
            abstract=True)

        friends = _FindFriends().findFriends(ptype)
        if ptype.isManaged():
            friends.update(ptype.managers)

        # |friend| managed actors so that they can call our Dealloc*()
        friends.update(ptype.manages)

        for friend in friends:
            self.hdrfile.addthings([
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

            if self.receivesMessage(md):
                # generate Recv/Answer* interface
                implicit = (not isdtor)
                recvDecl = MethodDecl(
                    md.recvMethod().name,
                    params=md.makeCxxParams(paramsems='in', returnsems='out',
                                            side=self.side, implicit=implicit),
                    ret=Type.BOOL, virtual=1)

                if isctor or isdtor:
                    defaultRecv = MethodDefn(recvDecl)
                    defaultRecv.addstmt(StmtReturn(ExprLiteral.TRUE))
                    self.cls.addstmt(defaultRecv)
                else:
                    recvDecl.pure = 1
                    self.cls.addstmt(StmtDecl(recvDecl))

        for md in p.messageDecls:
            managed = md.decl.type.constructedType()
            if not ptype.isManagerOf(managed):
                continue

            # add the Alloc/Dealloc interface for managed actors
            actortype = md.actorDecl().bareType(self.side)
            
            self.cls.addstmt(StmtDecl(MethodDecl(
                _allocMethod(managed).name,
                params=md.makeCxxParams(side=self.side, implicit=0),
                ret=actortype,
                virtual=1, pure=1)))

            self.cls.addstmt(StmtDecl(MethodDecl(
                _deallocMethod(managed).name,
                params=[ Decl(actortype, 'actor') ],
                ret=Type.BOOL,
                virtual=1, pure=1)))

        # optional ActorDestroy() method; default is no-op
        self.cls.addstmts([
            Whitespace.NL,
            MethodDefn(MethodDecl(
                _destroyMethod().name,
                params=[ Decl(_DestroyReason.Type(), 'why') ],
                virtual=1)),
            Whitespace.NL
        ])

        if ptype.isToplevel():
            # bool ShouldContinueFromReplyTimeout(); default to |true|
            shouldcontinue = MethodDefn(
                MethodDecl(p.shouldContinueFromTimeoutVar().name,
                           ret=Type.BOOL, virtual=1))
            shouldcontinue.addstmt(StmtReturn(ExprLiteral.TRUE))
            self.cls.addstmts([ shouldcontinue, Whitespace.NL ])

        self.cls.addstmts((
            [ Label.PRIVATE ]
            + self.standardTypedefs()
            + [ Whitespace.NL ]
        ))

        self.cls.addstmt(Label.PUBLIC)
        # Actor()
        ctor = ConstructorDefn(ConstructorDecl(self.clsname))
        if ptype.isToplevel():
            ctor.memberinits = [
                ExprMemberInit(p.channelVar(), [
                    ExprCall(ExprVar('ALLOW_THIS_IN_INITIALIZER_LIST'),
                             [ ExprVar.THIS ]) ]),
                ExprMemberInit(p.lastActorIdVar(),
                               [ p.actorIdInit(self.side) ])
            ]
        else:
            ctor.memberinits = [
                ExprMemberInit(p.idVar(), [ ExprLiteral.ZERO ]) ]

        if p.usesShmem():
            ctor.memberinits.append(
                ExprMemberInit(p.lastShmemIdVar(),
                               [ p.shmemIdInit(self.side) ]))

        ctor.addstmt(StmtExpr(ExprCall(ExprVar('MOZ_COUNT_CTOR'),
                                       [ ExprVar(self.clsname) ])))
        self.cls.addstmts([ ctor, Whitespace.NL ])

        # ~Actor()
        dtor = DestructorDefn(
            DestructorDecl(self.clsname, virtual=True))
        dtor.addstmt(StmtExpr(ExprCall(ExprVar('MOZ_COUNT_DTOR'),
                                               [ ExprVar(self.clsname) ])))

        self.cls.addstmts([ dtor, Whitespace.NL ])

        if ptype.isToplevel():
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
                             Param(Type('MessageLoop', ptr=True),
                                   aThreadVar.name,
                                   default=ExprLiteral.NULL) ],
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

            if ptype.talksSync() or ptype.talksRpc():
                # SetReplyTimeoutMs()
                timeoutvar = ExprVar('aTimeoutMs')
                settimeout = MethodDefn(MethodDecl(
                    'SetReplyTimeoutMs',
                    params=[ Decl(Type.INT32, timeoutvar.name) ]))
                settimeout.addstmt(StmtExpr(
                    ExprCall(
                        ExprSelect(p.channelVar(), '.', 'SetReplyTimeoutMs'),
                        args=[ timeoutvar ])))
                self.cls.addstmts([ settimeout, Whitespace.NL ])

        if not ptype.isToplevel():
            if 1 == len(p.managers):
                ## manager()
                managertype = p.managerActorType(self.side, ptr=1)
                managermeth = MethodDefn(MethodDecl(
                    p.managerMethod().name, ret=managertype))
                managermeth.addstmt(StmtReturn(
                    ExprCast(p.managerVar(), managertype, static=1)))

                self.cls.addstmts([ managermeth, Whitespace.NL ])

        ## managed[T]()
        for managed in ptype.manages:
            arrvar = ExprVar('aArr')
            meth = MethodDefn(MethodDecl(
                p.managedMethod(managed, self.side).name,
                params=[ Decl(p.managedVarType(managed, self.side, ref=1),
                              arrvar.name) ],
                const=1))
            meth.addstmt(StmtExpr(ExprAssn(
                arrvar, p.managedVar(managed, self.side))))
            self.cls.addstmts([ meth, Whitespace.NL ])

        ## OnMessageReceived()/OnCallReceived()

        # save these away for use in message handler case stmts
        msgvar = ExprVar('msg')
        self.msgvar = msgvar
        replyvar = ExprVar('reply')
        self.replyvar = replyvar
        
        msgtype = ExprCall(ExprSelect(msgvar, '.', 'type'), [ ])
        self.asyncSwitch = StmtSwitch(msgtype)
        if toplevel.talksSync():
            self.syncSwitch = StmtSwitch(msgtype)
            if toplevel.talksRpc():
                self.rpcSwitch = StmtSwitch(msgtype)

        # implement Send*() methods and add dispatcher cases to
        # message switch()es
        for md in p.messageDecls:
            self.visitMessageDecl(md)

        # "hidden" message that passes shmem mappings from one process
        # to the other
        if p.usesShmem():
            self.asyncSwitch.addcase(
                CaseLabel('SHMEM_CREATED_MESSAGE_TYPE'),
                self.genShmemCreatedHandler())

        # add default cases
        default = StmtBlock()
        default.addstmt(StmtReturn(_Result.NotKnown))
        self.asyncSwitch.addcase(DefaultLabel(), default)
        if toplevel.talksSync():
            self.syncSwitch.addcase(DefaultLabel(), default)
            if toplevel.talksRpc():
                self.rpcSwitch.addcase(DefaultLabel(), default)


        def makeHandlerMethod(name, switch, hasReply, dispatches=0):
            params = [ Decl(Type('Message', const=1, ref=1), msgvar.name) ]
            if hasReply:
                params.append(Decl(Type('Message', ref=1, ptr=1),
                                   replyvar.name))
            
            method = MethodDefn(MethodDecl(name, virtual=True,
                                           params=params, ret=_Result.Type()))
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

        dispatches = (ptype.isToplevel() and ptype.isManager())
        self.cls.addstmts([
            makeHandlerMethod('OnMessageReceived', self.asyncSwitch,
                              hasReply=0, dispatches=dispatches),
            Whitespace.NL
        ])
        if toplevel.talksSync():
            self.cls.addstmts([
                makeHandlerMethod('OnMessageReceived', self.syncSwitch,
                                  hasReply=1, dispatches=dispatches),
                Whitespace.NL
            ])
            if toplevel.talksRpc():
                self.cls.addstmts([
                    makeHandlerMethod('OnCallReceived', self.rpcSwitch,
                                      hasReply=1, dispatches=dispatches),
                    Whitespace.NL
                ])

        destroysubtreevar = ExprVar('DestroySubtree')
        deallocsubtreevar = ExprVar('DeallocSubtree')

        # OnReplyTimeout()
        if toplevel.talksSync() or toplevel.talksRpc():
            ontimeout = MethodDefn(
                MethodDecl('OnReplyTimeout', ret=Type.BOOL))

            if ptype.isToplevel():
                ontimeout.addstmt(StmtReturn(
                    ExprCall(p.shouldContinueFromTimeoutVar())))
            else:
                ontimeout.addstmts([
                    _runtimeAbort("`OnReplyTimeout' called on non-toplevel actor"),
                    StmtReturn(ExprLiteral.FALSE)
                ])

            self.cls.addstmts([ ontimeout, Whitespace.NL ])

        # OnChannelClose()
        onclose = MethodDefn(MethodDecl('OnChannelClose'))
        if ptype.isToplevel():
            onclose.addstmts([
                StmtExpr(ExprCall(destroysubtreevar,
                                  args=[ _DestroyReason.NormalShutdown ])),
                StmtExpr(ExprCall(deallocsubtreevar))
            ])
        else:
            onclose.addstmt(
                _runtimeAbort("`OnClose' called on non-toplevel actor"))
        self.cls.addstmts([ onclose, Whitespace.NL ])

        # OnChannelError()
        onerror = MethodDefn(MethodDecl('OnChannelError'))
        if ptype.isToplevel():
            onerror.addstmts([
                StmtExpr(ExprCall(destroysubtreevar,
                                  args=[ _DestroyReason.AbnormalShutdown ])),
                StmtExpr(ExprCall(deallocsubtreevar))
            ])
        else:
            onerror.addstmt(
                _runtimeAbort("`OnError' called on non-toplevel actor"))
        self.cls.addstmts([ onerror, Whitespace.NL ])

        # FIXME/bug 535053: only manager protocols and non-manager
        # protocols with union types need Lookup().  we'll give it to
        # all for the time being (simpler)
        if 1 or ptype.isManager():
            self.cls.addstmts(self.implementManagerIface())

        if p.usesShmem():
            self.cls.addstmts(self.makeShmemIface())

        if ptype.isToplevel() and self.side is 'parent':
            ## bool GetMinidump(nsIFile** dump)
            self.cls.addstmt(Label.PROTECTED)

            otherpidvar = ExprVar('OtherSidePID')
            otherpid = MethodDefn(MethodDecl(
                otherpidvar.name, params=[ ],
                ret=Type('base::ProcessId'),
                const=1))
            otherpid.addstmts([
                StmtReturn(ExprCall(
                    ExprVar('base::GetProcId'),
                    args=[ p.otherProcessVar() ])),
            ])

            dumpvar = ExprVar('aDump')
            getdump = MethodDefn(MethodDecl(
                'GetMinidump',
                params=[ Decl(Type('nsIFile', ptrptr=1), dumpvar.name) ],
                ret=Type.BOOL,
                const=1))
            getdump.addstmts([
                CppDirective('ifdef', 'MOZ_CRASHREPORTER'),
                StmtReturn(ExprCall(
                    ExprVar('XRE_GetMinidumpForChild'),
                    args=[ ExprCall(otherpidvar), dumpvar ])),
                CppDirective('else'),
                StmtReturn(ExprLiteral.FALSE),
                CppDirective('endif')
            ])
            self.cls.addstmts([ otherpid, Whitespace.NL,
                                getdump, Whitespace.NL ])

        if (ptype.isToplevel() and self.side is 'parent'
            and ptype.talksRpc()):
            # offer BlockChild() and UnblockChild().
            # See ipc/glue/RPCChannel.h
            blockchild = MethodDefn(MethodDecl(
                'BlockChild', ret=Type.BOOL))
            blockchild.addstmt(StmtReturn(ExprCall(
                ExprSelect(p.channelVar(), '.', 'BlockChild'))))

            unblockchild = MethodDefn(MethodDecl(
                'UnblockChild', ret=Type.BOOL))
            unblockchild.addstmt(StmtReturn(ExprCall(
                ExprSelect(p.channelVar(), '.', 'UnblockChild'))))

            self.cls.addstmts([ blockchild, unblockchild, Whitespace.NL ])

        ## private methods
        self.cls.addstmt(Label.PRIVATE)

        ## FatalError()       
        msgvar = ExprVar('msg')
        fatalerror = MethodDefn(MethodDecl(
            'FatalError',
            params=[ Decl(Type('char', const=1, ptrconst=1), msgvar.name) ],
            const=1))
        fatalerror.addstmts([
            _printErrorMessage('IPDL error:'),
            _printErrorMessage(msgvar),
            Whitespace.NL
        ])
        actorname = _actorName(p.name, self.side)
        if self.side is 'parent':
            # if the error happens on the parent side, the parent
            # kills off the child
            fatalerror.addstmts([
                _printErrorMessage(
                    '['+ actorname +'] killing child side as a result'),
                Whitespace.NL
            ])

            ifkill = StmtIf(ExprNot(
                _killProcess(ExprCall(p.otherProcessMethod()))))
            ifkill.addifstmt(
                _printErrorMessage("  may have failed to kill child!"))
            fatalerror.addstmt(ifkill)
        else:
            # and if it happens on the child side, the child commits
            # seppuko
            fatalerror.addstmt(
                _runtimeAbort('['+ actorname +'] abort()ing as a result'))
        self.cls.addstmts([ fatalerror, Whitespace.NL ])

        ## DestroySubtree(bool normal)
        whyvar = ExprVar('why')
        subtreewhyvar = ExprVar('subtreewhy')
        kidsvar = ExprVar('kids')
        ivar = ExprVar('i')
        ithkid = ExprIndex(kidsvar, ivar)

        destroysubtree = MethodDefn(MethodDecl(
            destroysubtreevar.name,
            params=[ Decl(_DestroyReason.Type(), whyvar.name) ]))

        if ptype.isManager():
            # only declare this for managers to avoid unused var warnings
            destroysubtree.addstmts([
                StmtDecl(
                    Decl(_DestroyReason.Type(), subtreewhyvar.name),
                    init=ExprConditional(
                        ExprBinary(_DestroyReason.Deletion, '==', whyvar),
                        _DestroyReason.AncestorDeletion, whyvar)),
                Whitespace.NL
            ])

        for managed in ptype.manages:
            foreachdestroy = StmtFor(
                init=Param(Type.UINT32, ivar.name, ExprLiteral.ZERO),
                cond=ExprBinary(ivar, '<', _callCxxArrayLength(kidsvar)),
                update=ExprPrefixUnop(ivar, '++'))
            foreachdestroy.addstmt(StmtExpr(ExprCall(
                ExprSelect(ithkid, '->', destroysubtreevar.name),
                args=[ subtreewhyvar ])))

            block = StmtBlock()
            block.addstmts([
                Whitespace(
                    '// Recursively shutting down %s kids\n'% (managed.name()),
                    indent=1),
                StmtDecl(
                    Decl(p.managedVarType(managed, self.side), kidsvar.name),
                    init=p.managedVar(managed, self.side)),
                foreachdestroy,
            ])
            destroysubtree.addstmt(block)
        # finally, destroy "us"
        destroysubtree.addstmt(StmtExpr(
            ExprCall(_destroyMethod(), args=[ whyvar ])))
        
        self.cls.addstmts([ destroysubtree, Whitespace.NL ])

        ## DeallocSubtree()
        deallocsubtree = MethodDefn(MethodDecl(deallocsubtreevar.name))
        for managed in ptype.manages:
            foreachrecurse = StmtFor(
                init=Param(Type.UINT32, ivar.name, ExprLiteral.ZERO),
                cond=ExprBinary(ivar, '<', _callCxxArrayLength(kidsvar)),
                update=ExprPrefixUnop(ivar, '++'))
            foreachrecurse.addstmt(StmtExpr(ExprCall(
                ExprSelect(ithkid, '->', deallocsubtreevar.name))))

            foreachdealloc = StmtFor(
                init=Param(Type.UINT32, ivar.name, ExprLiteral.ZERO),
                cond=ExprBinary(ivar, '<', _callCxxArrayLength(kidsvar)),
                update=ExprPrefixUnop(ivar, '++'))
            foreachdealloc.addstmts([
                StmtExpr(ExprCall(_deallocMethod(managed),
                                  args=[ ithkid ]))
            ])

            block = StmtBlock()
            block.addstmts([
                Whitespace(
                    '// Recursively deleting %s kids\n'% (managed.name()),
                    indent=1),
                StmtDecl(
                    Decl(p.managedVarType(managed, self.side, ref=1),
                         kidsvar.name),
                    init=p.managedVar(managed, self.side)),
                foreachrecurse,
                Whitespace.NL,
                # no need to copy |kids| here; we're the ones deleting
                # stragglers, no outside C++ is being invoked (except
                # Dealloc(subactor))
                foreachdealloc,
                StmtExpr(_callCxxArrayClear(p.managedVar(managed, self.side))),

            ])
            deallocsubtree.addstmt(block)
        # don't delete outselves: either the manager will do it, or
        # we're toplevel
        self.cls.addstmts([ deallocsubtree, Whitespace.NL ])
        
        ## private members
        self.cls.addstmt(StmtDecl(Decl(p.channelType(), 'mChannel')))
        if ptype.isToplevel():
            self.cls.addstmts([
                StmtDecl(Decl(Type('IDMap', T=Type('ChannelListener')),
                              p.actorMapVar().name)),
                StmtDecl(Decl(_actorIdType(), p.lastActorIdVar().name)),
                StmtDecl(Decl(Type('ProcessHandle'),
                              p.otherProcessVar().name))
            ])
        elif ptype.isManaged():
            self.cls.addstmts([
                StmtDecl(Decl(_actorIdType(), p.idVar().name)),
                StmtDecl(Decl(p.managerInterfaceType(ptr=1),
                              p.managerVar().name))
            ])
        if p.usesShmem():
            self.cls.addstmts([
                StmtDecl(Decl(Type('IDMap', T=_rawShmemType()),
                              p.shmemMapVar().name)),
                StmtDecl(Decl(_shmemIdType(), p.lastShmemIdVar().name))
            ])

        for managed in ptype.manages:
            self.cls.addstmts([
                Whitespace('// Sorted by pointer value\n', indent=1),
                StmtDecl(Decl(
                    p.managedVarType(managed, self.side),
                    p.managedVar(managed, self.side).name)) ])

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
            const=1,
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
        else:
            # delegate registration to manager
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

        # all protocols share the "same" RemoveManagee() implementation
        pvar = ExprVar('aProtocolId')
        listenervar = ExprVar('aListener')
        removemanagee = MethodDefn(MethodDecl(
            p.removeManageeMethod().name,
            params=[ Decl(_protocolIdType(), pvar.name),
                     Decl(listenertype, listenervar.name) ],
            virtual=1))

        switchontype = StmtSwitch(pvar)
        for managee in p.managesStmts:
            case = StmtBlock()
            actorvar = ExprVar('actor')
            manageeipdltype = managee.decl.type
            manageecxxtype = _bareCxxType(ipdl.type.ActorType(manageeipdltype),
                                       self.side)
            manageearray = p.managedVar(manageeipdltype, self.side)

            case.addstmts([
                StmtDecl(Decl(manageecxxtype, actorvar.name),
                         ExprCast(listenervar, manageecxxtype, static=1)),
                _abortIfFalse(
                    _cxxArrayHasElementSorted(manageearray, actorvar),
                    "actor not managed by this!"),
                Whitespace.NL,
                StmtExpr(_callCxxArrayRemoveSorted(manageearray, actorvar)),
                StmtExpr(ExprCall(_deallocMethod(manageeipdltype),
                                  args=[ actorvar ])),
                StmtReturn()
            ])
            switchontype.addcase(CaseLabel(_protocolId(manageeipdltype).name),
                                 case)

        default = StmtBlock()
        default.addstmts([ _runtimeAbort('unreached'), StmtReturn() ])
        switchontype.addcase(DefaultLabel(), default)

        removemanagee.addstmt(switchontype)

        return [ register,
                 registerid,
                 lookup,
                 unregister,
                 removemanagee,
                 otherprocess,
                 Whitespace.NL ]


    def makeShmemIface(self):
        p = self.protocol
        idvar = ExprVar('aId')

        # bool AllocShmem(size_t size, Shmem* outmem):
        #   nsAutoPtr<shmem_t> shmem(Shmem::Alloc(size));
        #   if (!shmem)
        #     return false
        #   shmemhandle_t handle;
        #   if (!shmem->ShareToProcess(subprocess, &handle))
        #     return false;
        #   Shmem::id_t id = RegisterShmem(shmem);
        #   Message* msg = new __internal__ipdl__ShmemCreated(
        #      mRoutingId, handle, id, size);
        #   if (!Send(msg))
        #     return false;
        #   *aMem = Shmem(shmem, id);
        #   return true;
        sizevar = ExprVar('aSize')
        memvar = ExprVar('aMem')
        allocShmem = MethodDefn(MethodDecl(
            'AllocShmem',
            params=[ Decl(Type.SIZE, sizevar.name),
                     Decl(_shmemType(ptr=1), memvar.name) ],
            ret=Type.BOOL))

        rawvar = ExprVar('rawmem')
        allocShmem.addstmt(StmtDecl(
            Decl(_autoptr(_rawShmemType()), rawvar.name),
            initargs=[ _shmemAlloc(sizevar) ]))
        failif = StmtIf(ExprNot(rawvar))
        failif.addifstmt(StmtReturn(ExprLiteral.FALSE))
        allocShmem.addstmt(failif)

        handlevar = ExprVar('handle')
        allocShmem.addstmt(StmtDecl(
            Decl(_shmemHandleType(), handlevar.name)))
        failif = StmtIf(ExprNot(ExprCall(
            ExprSelect(rawvar, '->', 'ShareToProcess'),
            args=[ ExprCall(p.otherProcessMethod()),
                   ExprAddrOf(handlevar) ])))
        failif.addifstmt(StmtReturn(ExprLiteral.FALSE))
        allocShmem.addstmt(failif)

        allocShmem.addstmt(StmtDecl(
            Decl(_shmemIdType(), idvar.name),
            ExprCall(p.registerShmemVar(), args=[ rawvar ])))

        msgvar = ExprVar('msg')
        allocShmem.addstmt(StmtDecl(
            Decl(Type('Message', ptr=1), msgvar.name),
            ExprNew(Type(_shmemCreatedMsgVar().name),
                    args=[ p.routingId(), handlevar, idvar, sizevar ])))

        failif = StmtIf(ExprNot(ExprCall(
            ExprSelect(p.channelVar(), p.channelSel(), 'Send'),
            args=[ msgvar ])))
        failif.addifstmts([
            StmtExpr(ExprCall(p.unregisterShmemVar(), args=[ idvar ])),
            StmtReturn(ExprLiteral.FALSE)
        ])
        allocShmem.addstmt(failif)

        allocShmem.addstmts([
            StmtExpr(ExprAssn(
                ExprDeref(memvar), _shmemCtor(_autoptrForget(rawvar), idvar))),
            StmtReturn(ExprLiteral.TRUE)
        ])

        # TODO: DeallocShmem().  not needed until actors outlast their
        # shmem mappings.
        
        # This code is pretty similar to |implementManagerIface()|
        lookupShmem = MethodDefn(MethodDecl(
            p.lookupShmemVar().name,
            params=[ Decl(_shmemIdType(), idvar.name) ],
            ret=_rawShmemType(ptr=1)))
        lookupShmem.addstmt(StmtReturn(ExprCall(
            ExprSelect(p.shmemMapVar(), '.', 'Lookup'),
            args=[ idvar ])))

        mapvar = ExprVar('aMap')
        tmpvar = ExprVar('tmp')
        registerShmem = MethodDefn(MethodDecl(
            p.registerShmemVar().name,
            params=[ Decl(_rawShmemType(ptr=1), mapvar.name) ],
            ret=_shmemIdType()))
        registerShmem.addstmts([
            StmtDecl(Decl(_shmemIdType(), tmpvar.name),
                     p.nextShmemIdExpr(self.side)),
            StmtExpr(ExprCall(ExprSelect(p.shmemMapVar(), '.', 'AddWithID'),
                              [ mapvar, tmpvar ])),
            StmtReturn(tmpvar)
        ])

        registerShmemById = MethodDefn(MethodDecl(
            p.registerShmemIdVar().name,
            params=[ Decl(_rawShmemType(ptr=1), mapvar.name),
                     Decl(_shmemIdType(), idvar.name) ],
            ret=_shmemIdType()))
        registerShmemById.addstmts([
            StmtExpr(ExprCall(ExprSelect(p.shmemMapVar(), '.', 'AddWithID'),
                              [ mapvar, idvar ])),
            StmtReturn(idvar)
        ])

        unregisterShmem = MethodDefn(MethodDecl(
            p.unregisterShmemVar().name,
            params=[ Decl(_shmemIdType(), idvar.name) ]))
        unregisterShmem.addstmts([
            StmtExpr(ExprCall(ExprSelect(p.shmemMapVar(), '.', 'Remove'),
                              args=[ idvar ]))
        ])

        return [
            Whitespace('// Methods for managing shmem\n', indent=1),
            allocShmem,
            Whitespace.NL,
            Label.PRIVATE,
            lookupShmem,
            registerShmem,
            registerShmemById,
            unregisterShmem,
            Whitespace.NL
        ]

    def genShmemCreatedHandler(self):
        case = StmtBlock()                                          

        handlevar = ExprVar('handle')
        idvar = ExprVar('id')
        sizevar = ExprVar('size')
        rawvar = ExprVar('rawmem')
        failif = StmtIf(ExprNot(ExprCall(
            ExprVar(_shmemCreatedMsgVar().name +'::Read'),
            args=[ ExprAddrOf(self.msgvar),
                   ExprAddrOf(handlevar),
                   ExprAddrOf(idvar),
                   ExprAddrOf(sizevar) ])))
        failif.addifstmt(StmtReturn(_Result.PayloadError))

        case.addstmts([
            StmtDecl(Decl(_shmemHandleType(), handlevar.name)),
            StmtDecl(Decl(_shmemIdType(), idvar.name)),
            StmtDecl(Decl(Type.SIZE, sizevar.name)),
            Whitespace.NL,
            failif,
            Whitespace.NL,
            StmtDecl(Decl(_autoptr(_rawShmemType()), rawvar.name),
                     initargs=[ _shmemOpenExisting(sizevar, handlevar) ])
        ])

        failif = StmtIf(ExprNot(rawvar))
        failif.addifstmt(StmtReturn(_Result.ValuError))

        case.addstmts([
            failif,
            StmtExpr(ExprCall(self.protocol.registerShmemIdVar(),
                              args=[ _autoptrForget(rawvar), idvar ])),
            Whitespace.NL,
            StmtReturn(_Result.Processed)
        ])

        return case


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

        # XXX figure out what to do here
        if isdtor and md.decl.type.constructedType().isToplevel():
            sendmethod = None
                
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

            # XXX figure out what to do here
            if isdtor and md.decl.type.constructedType().isToplevel():
                return

            addRecvCase(recvlbl, recvcase)


    def genAsyncCtor(self, md):
        actor = md.actorDecl()
        method = MethodDefn(self.makeSendMethodDecl(md))
        method.addstmts(self.ctorPrologue(md) + [ Whitespace.NL ])

        msgvar, stmts = self.makeMessage(md, errfnSendCtor)
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

        msgvar, stmts = self.makeMessage(md, errfnSendCtor)

        replyvar = self.replyvar
        sendok, sendstmts = self.sendBlocking(md, msgvar, replyvar)
        method.addstmts(
            stmts
            + [ Whitespace.NL,
                StmtDecl(Decl(Type('Message'), replyvar.name)) ]
            + sendstmts
            + self.failCtorIf(md, ExprNot(sendok)))

        readok, stmts = self.deserializeReply(
            md, ExprAddrOf(replyvar), self.side, errfnSendCtor)
        method.addstmts(
            stmts
            + self.failCtorIf(md, ExprNot(readok))
            + [ StmtReturn(actor.var()) ])

        return method


    def ctorPrologue(self, md, errfn=ExprLiteral.NULL, idexpr=None):
        actorvar = md.actorDecl().var()

        if idexpr is None:
            idexpr = ExprCall(self.protocol.registerMethod(),
                              args=[ actorvar ])
        else:
            idexpr = ExprCall(self.protocol.registerIDMethod(),
                              args=[ actorvar, idexpr ])

        return [
            self.failIfNullActor(actorvar, errfn),
            StmtExpr(ExprAssn(_actorId(actorvar), idexpr)),
            StmtExpr(ExprAssn(_actorManager(actorvar), ExprVar.THIS)),
            StmtExpr(ExprAssn(_actorChannel(actorvar),
                              self.protocol.channelForSubactor())),
            StmtExpr(_callCxxArrayInsertSorted(
                self.protocol.managedVar(md.decl.type.constructedType(),
                                         self.side),
                actorvar))
        ]

    def failCtorIf(self, md, cond):
        actorvar = md.actorDecl().var()
        failif = StmtIf(cond)
        failif.addifstmts(
            self.unregisterActor(actorvar)
            + [ StmtExpr(self.callRemoveActor(
                    actorvar,
                    ipdltype=md.decl.type.constructedType())),
                StmtReturn(ExprLiteral.NULL),
            ])
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
        actorvar = actor.var()
        method = MethodDefn(self.makeDtorMethodDecl(md))

        method.addstmts(self.dtorPrologue(actor.var()))
        method.addstmts(self.dtorPrologue(actorvar))

        msgvar, stmts = self.makeMessage(md, errfnSendDtor, actorvar)
        sendok, sendstmts = self.sendAsync(md, msgvar, actorvar)
        method.addstmts(
            stmts
            + sendstmts
            + [ Whitespace.NL ]
            + self.dtorEpilogue(md, actor.var())
            + [ StmtReturn(sendok) ])

        lbl = CaseLabel(md.pqReplyId())
        case = StmtBlock()
        case.addstmt(StmtReturn(_Result.Processed))
        # TODO if the dtor is "inherently racy", keep the actor alive
        # until the other side acks

        return method, (lbl, case)


    def genBlockingDtorMethod(self, md):
        actor = md.actorDecl()
        actorvar = actor.var()
        method = MethodDefn(self.makeDtorMethodDecl(md))

        method.addstmts(self.dtorPrologue(actorvar))

        msgvar, stmts = self.makeMessage(md, errfnSendDtor, actorvar)

        replyvar = self.replyvar
        sendok, sendstmts = self.sendBlocking(md, msgvar, replyvar, actorvar)
        method.addstmts(
            stmts
            + [ Whitespace.NL,
                StmtDecl(Decl(Type('Message'), replyvar.name)) ]
            + sendstmts)

        readok, destmts = self.deserializeReply(
            md, ExprAddrOf(replyvar), self.side, errfnSend)
        ifsendok = StmtIf(sendok)
        ifsendok.addifstmts(destmts)
        ifsendok.addifstmts([ Whitespace.NL,
                              StmtExpr(ExprAssn(sendok, readok, '&=')) ])

        method.addstmts(
            [ ifsendok ]
            + self.dtorEpilogue(md, actor.var())
            + [ Whitespace.NL, StmtReturn(sendok) ])

        return method

    def dtorPrologue(self, actorexpr):
        return [ self.failIfNullActor(actorexpr), Whitespace.NL ]

    def dtorEpilogue(self, md, actorexpr):
        return (self.unregisterActor(actorexpr)
                + [ StmtExpr(self.callActorDestroy(actorexpr)),
                    StmtExpr(self.callDeallocSubtree(md, actorexpr)),
                    StmtExpr(self.callRemoveActor(
                        actorexpr,
                        manager=self.protocol.managerVar(actorexpr)))
                  ])

    def genAsyncSendMethod(self, md):
        method = MethodDefn(self.makeSendMethodDecl(md))
        msgvar, stmts = self.makeMessage(md, errfnSend)
        sendok, sendstmts = self.sendAsync(md, msgvar)
        method.addstmts(stmts
                        +[ Whitespace.NL ]
                        + sendstmts
                        +[ StmtReturn(sendok) ])
        return method


    def genBlockingSendMethod(self, md, fromActor=None):
        method = MethodDefn(self.makeSendMethodDecl(md))

        msgvar, serstmts = self.makeMessage(md, errfnSend, fromActor)
        replyvar = self.replyvar

        sendok, sendstmts = self.sendBlocking(md, msgvar, replyvar)
        failif = StmtIf(ExprNot(sendok))
        failif.addifstmt(StmtReturn(ExprLiteral.FALSE))

        readok, desstmts = self.deserializeReply(
            md, ExprAddrOf(replyvar), self.side, errfnSend)

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

        actorhandle, readok, stmts = self.deserializeMessage(md, self.side,
                                                             errfnRecv)
        failif = StmtIf(ExprNot(readok))
        failif.addifstmt(StmtReturn(_Result.PayloadError))

        idvar, saveIdStmts = self.saveActorId(md)
        case.addstmts(
            stmts
            + [ failif, Whitespace.NL ]
            + [ StmtDecl(Decl(r.bareType(self.side), r.var().name))
                for r in md.returns ]
            # alloc the actor, register it under the foreign ID
            + [ StmtExpr(ExprAssn(
                actorvar,
                self.callAllocActor(md, retsems='in'))) ]
            + self.ctorPrologue(md, errfn=_Result.ValuError,
                                idexpr=_actorHId(actorhandle))
            + [ Whitespace.NL ]
            + saveIdStmts
            + self.invokeRecvHandler(md)
            + self.makeReply(md, errfnRecv, idvar)
            + [ Whitespace.NL,
                StmtReturn(_Result.Processed) ])

        return lbl, case


    def genDtorRecvCase(self, md):
        lbl = CaseLabel(md.pqMsgId())
        case = StmtBlock()

        readok, stmts = self.deserializeMessage(md, self.side, errfnRecv)
        failif = StmtIf(ExprNot(readok))
        failif.addifstmt(StmtReturn(_Result.PayloadError))

        idvar, saveIdStmts = self.saveActorId(md)
        case.addstmts(
            stmts
            + [ failif, Whitespace.NL ]
            + [ StmtDecl(Decl(r.bareType(self.side), r.var().name))
                for r in md.returns ]
            + self.invokeRecvHandler(md, implicit=0)
            + [ Whitespace.NL ]
            + saveIdStmts
            + self.dtorEpilogue(md, md.actorDecl().var())
            + [ Whitespace.NL ]
            + self.makeReply(md, errfnRecv, routingId=idvar)
            + [ Whitespace.NL,
                StmtReturn(_Result.Processed) ])
        
        return lbl, case


    def genRecvCase(self, md):
        lbl = CaseLabel(md.pqMsgId())
        case = StmtBlock()

        readok, stmts = self.deserializeMessage(md, self.side, errfn=errfnRecv)
        failif = StmtIf(ExprNot(readok))
        failif.addifstmt(StmtReturn(_Result.PayloadError))

        idvar, saveIdStmts = self.saveActorId(md)
        case.addstmts(
            stmts
            + [ failif, Whitespace.NL ]
            + [ StmtDecl(Decl(r.bareType(self.side), r.var().name))
                for r in md.returns ]
            + saveIdStmts
            + self.invokeRecvHandler(md)
            + [ Whitespace.NL ]
            + self.makeReply(md, errfnRecv, routingId=idvar)
            + [ StmtReturn(_Result.Processed) ])

        return lbl, case


    # helper methods

    def failIfNullActor(self, actorExpr, retOnNull=ExprLiteral.FALSE):
        failif = StmtIf(ExprNot(actorExpr))
        failif.addifstmt(StmtReturn(retOnNull))
        return failif

    def unregisterActor(self, actorexpr):
        return [ StmtExpr(ExprCall(self.protocol.unregisterMethod(actorexpr),
                                   args=[ _actorId(actorexpr) ])),
                 StmtExpr(ExprAssn(_actorId(actorexpr), _FREED_ACTOR_ID)) ]

    def makeMessage(self, md, errfn, fromActor=None):
        msgvar = self.msgvar
        stmts = [ StmtDecl(Decl(Type(md.pqMsgClass(), ptr=1), msgvar.name)),
                  Whitespace.NL ]
        msgCtorArgs = [ ]

        for param in md.params:
            arg, sstmts = param.serialize(param.var(), self.side, errfn)
            msgCtorArgs.append(arg)
            stmts.extend(sstmts)

        routingId = self.protocol.routingId(fromActor)
        stmts.extend([
            StmtExpr(ExprAssn(
                msgvar,
                ExprNew(Type(md.pqMsgClass()), args=msgCtorArgs))) ]
            + self.setMessageFlags(md, msgvar, reply=0, routingId=routingId))

        return msgvar, stmts


    def makeReply(self, md, errfn, routingId):
        # TODO special cases for async ctor/dtor replies
        if not md.decl.type.hasReply():
            return [ ]

        replyvar = self.replyvar
        stmts = [ ]
        replyCtorArgs = [ ]
        for ret in md.returns:
            arg, sstmts = ret.serialize(ret.var(), self.side, errfn)
            replyCtorArgs.append(arg)
            stmts.extend(sstmts)

        stmts.extend([
            StmtExpr(ExprAssn(
                replyvar,
                ExprNew(Type(md.pqReplyClass()), args=replyCtorArgs))) ]
            + self.setMessageFlags(md, replyvar, reply=1, routingId=routingId)
            +[ self.logMessage(md, md.replyCast(replyvar), 'Sending reply ') ])
        
        return stmts


    def setMessageFlags(self, md, var, reply, routingId=None):
        if routingId is None:
            routingId = self.protocol.routingId()
        
        stmts = [ StmtExpr(ExprCall(
            ExprSelect(var, '->', 'set_routing_id'),
            args=[ routingId ])) ]

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


    def deserializeMessage(self, md, side, errfn):
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
            ifok.addifstmts(param.deserialize(readvars[i], side, sems='in',
                                              errfn=errfn))
        if len(ifok.ifb.stmts):
            stmts.extend([ Whitespace.NL, ifok ])

        if isctor:
            # return the raw actor handle so that its ID can be used
            # to construct the "real" actor
            return readvars[0], okvar, stmts
        return okvar, stmts


    def deserializeReply(self, md, replyexpr, side, errfn):
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
                ExprDeref(readexprs[i]), side, sems='out',
                errfn=errfn))
        if len(ifok.ifb.stmts):
            stmts.extend([ Whitespace.NL, ifok ])

        return okvar, stmts


    def sendAsync(self, md, msgexpr, actor=None):
        sendok = ExprVar('__sendok')
        return (
            sendok,
            [ Whitespace.NL,
              self.logMessage(md, msgexpr, 'Sending '),
              Whitespace.NL,
              StmtDecl(Decl(Type.BOOL, sendok.name),
                       init=ExprCall(
                           ExprSelect(self.protocol.channelVar(actor),
                                      self.protocol.channelSel(), 'Send'),
                           args=[ msgexpr ]))
            ])

    def sendBlocking(self, md, msgexpr, replyexpr, actor=None):
        sendok = ExprVar('__sendok')
        return (
            sendok,
            [ Whitespace.NL,
              self.logMessage(md, msgexpr, 'Sending '),
              Whitespace.NL,
              StmtDecl(
                  Decl(Type.BOOL, sendok.name),
                  init=ExprCall(ExprSelect(self.protocol.channelVar(actor),
                                           self.protocol.channelSel(),
                                           _sendPrefix(md.decl.type)),
                                args=[ msgexpr, ExprAddrOf(replyexpr) ]))
            ])

    def callAllocActor(self, md, retsems):
        return ExprCall(
            _allocMethod(md.decl.type.constructedType()),
            args=md.makeCxxArgs(params=1, retsems=retsems, retcallsems='out',
                                implicit=0))

    def callActorDestroy(self, actorexpr, why=_DestroyReason.Deletion):
        return ExprCall(ExprSelect(actorexpr, '->', 'DestroySubtree'),
                        args=[ why ])

    def callRemoveActor(self, actorexpr, manager=None, ipdltype=None):
        if ipdltype is None: ipdltype = self.protocol.decl.type

        if not ipdltype.isManaged():
            return Whitespace('// unmanaged protocol')

        removefunc = self.protocol.removeManageeMethod()
        if manager is not None:
            removefunc = ExprSelect(manager, '->', removefunc.name)

        return ExprCall(removefunc,
                        args=[ _protocolId(ipdltype),
                               actorexpr ])

    def callDeallocSubtree(self, md, actorexpr):
        return ExprCall(ExprSelect(actorexpr, '->', 'DeallocSubtree'))

    def invokeRecvHandler(self, md, implicit=1):
        failif = StmtIf(ExprNot(
            ExprCall(md.recvMethod(),
                     args=md.makeCxxArgs(params=1,
                                         retsems='in', retcallsems='out',
                                         implicit=implicit))))
        failif.addifstmt(StmtReturn(_Result.ValuError))
        return [ failif ]

    def makeDtorMethodDecl(self, md):
        decl = self.makeSendMethodDecl(md)
        decl.static = 1
        return decl

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
        actorname = _actorName(self.protocol.name, self.side)
        return _ifLogging([
            StmtExpr(ExprCall(
                ExprSelect(msgptr, '->', 'Log'),
                args=[ ExprLiteral.String('['+ actorname +'] '+ pfx),
                       ExprVar('stderr') ])) ])

    def saveActorId(self, md):
        idvar = ExprVar('__id')
        if md.decl.type.hasReply():
            # only save the ID if we're actually going to use it, to
            # avoid unused-variable warnings
            saveIdStmts = [ StmtDecl(Decl(_actorIdType(), idvar.name),
                                     self.protocol.routingId()) ]
        else:
            saveIdStmts = [ ]
        return idvar, saveIdStmts


class _GenerateProtocolParentCode(_GenerateProtocolActorCode):
    def __init__(self):
        _GenerateProtocolActorCode.__init__(self, 'parent')

    def sendsMessage(self, md):
        return not md.decl.type.isIn()

    def receivesMessage(self, md):
        return md.decl.type.isInout() or md.decl.type.isIn()

class _GenerateProtocolChildCode(_GenerateProtocolActorCode):
    def __init__(self):
        _GenerateProtocolActorCode.__init__(self, 'child')

    def sendsMessage(self, md):
        return not md.decl.type.isOut()

    def receivesMessage(self, md):
        return md.decl.type.isInout() or md.decl.type.isOut()


##-----------------------------------------------------------------------------
## Utility passes
##

class _ClassDeclDefn:
    def split(self, cls):
        """Warning: destructively splits |cls|!"""
        defns = Block()

        for i, stmt in enumerate(cls.stmts):
            if isinstance(stmt, MethodDefn):
                decl, defn = self.splitMethodDefn(stmt, cls.name)
                cls.stmts[i] = StmtDecl(decl)
                defns.addstmts([ defn, Whitespace.NL ])

        return cls, defns

    def splitMethodDefn(self, md, clsname):
        saveddecl = deepcopy(md.decl)
        md.decl.name = (clsname +'::'+ md.decl.name)
        md.decl.virtual = 0
        md.decl.static = 0
        for param in md.decl.params:
            if isinstance(param, Param):
                param.default = None
        return saveddecl, md


# XXX this is tantalizingly similar to _SplitDeclDefn, but just
# different enough that I don't see the need to define
# _GenerateSkeleton in terms of that
class _GenerateSkeletonImpl(Visitor):
    def __init__(self, name, namespaces):
        self.name = name
        self.cls = None
        self.namespaces = namespaces
        self.methodimpls = Block()

    def fromclass(self, cls):
        cls.accept(self)

        nsclass = _putInNamespaces(self.cls, self.namespaces)
        nsmethodimpls = _putInNamespaces(self.methodimpls, self.namespaces)

        return [
            Whitespace('''
//-----------------------------------------------------------------------------
// Skeleton implementation of abstract actor class

'''),
            Whitespace('// Header file contents\n'),
            nsclass,
            Whitespace.NL,
            Whitespace('\n// C++ file contents\n'),
            nsmethodimpls
        ]


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
        self.methodimpls.addstmts([ impl, Whitespace.NL ])

    def implname(self, method):
        return self.name +'::'+ method
