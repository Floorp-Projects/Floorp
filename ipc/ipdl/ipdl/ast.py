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

class Visitor:
    def defaultVisit(self, node):
        raise Exception, "INTERNAL ERROR: no visitor for node type `%s'"% (
            node.__class__.__name__)

    def visitTranslationUnit(self, tu):
        for cxxInc in tu.cxxIncludes:
            cxxInc.accept(self)
        for protoInc in tu.protocolIncludes:
            protoInc.accept(self)
        for using in tu.using:
            using.accept(self)
        tu.protocol.accept(self)

    def visitCxxInclude(self, inc):
        pass

    def visitProtocolInclude(self, inc):
        # Note: we don't visit the child AST here, because that needs delicate
        # and pass-specific handling
        pass

    def visitUsingStmt(self, using):
        pass

    def visitProtocol(self, p):
        for namespace in p.namespaces:
            namespace.accept(self)
        if p.manager is not None:
            p.manager.accept(self)
        for managed in p.managesStmts:
            managed.accept(self)
        for msgDecl in p.messageDecls:
            msgDecl.accept(self)
        for transitionStmt in p.transitionStmts:
            transitionStmt.accept(self)

    def visitNamespace(self, ns):
        pass

    def visitManagerStmt(self, mgr):
        pass

    def visitManagesStmt(self, mgs):
        pass

    def visitMessageDecl(self, md):
        for inParam in md.inParams:
            inParam.accept(self)
        for outParam in md.outParams:
            outParam.accept(self)

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

class _struct():
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

class TranslationUnit(Node):
    def __init__(self):
        Node.__init__(self)
        self.filename = None
        self.cxxIncludes = [ ]
        self.protocolIncludes = [ ]
        self.using = [ ]
        self.protocol = None

    def addCxxInclude(self, cxxInclude): self.cxxIncludes.append(cxxInclude)
    def addProtocolInclude(self, pInc): self.protocolIncludes.append(pInc)
    def addUsingStmt(self, using): self.using.append(using)

    def setProtocol(self, protocol): self.protocol = protocol

class CxxInclude(Node):
    def __init__(self, loc, cxxFile):
        Node.__init__(self, loc)
        self.file = cxxFile

class ProtocolInclude(Node):
    def __init__(self, loc, protocolFile):
        Node.__init__(self, loc)
        self.file = protocolFile

class UsingStmt(Node):
    def __init__(self, loc, cxxTypeSpec):
        Node.__init__(self, loc)
        self.type = cxxTypeSpec
        
# "singletons"
class ASYNC:
    pretty = 'Async'
class RPC:
    pretty = 'Rpc'
class SYNC:
    pretty = 'Sync'

class INOUT:
    pretty = 'InOut'
class IN:
    @staticmethod
    def pretty(ss): return _prettyTable['In'][ss.pretty]
class OUT:
    @staticmethod
    def pretty(ss): return _prettyTable['Out'][ss.pretty]

_prettyTable = {
    'In'  : { 'Async': 'AsyncRecv',
             'Sync': 'SyncRecv',
             'Rpc': 'RpcAnswer' },
    'Out' : { 'Async': 'AsyncSend',
              'Sync': 'SyncSend',
              'Rpc': 'RpcCall' }
    # inout doesn't make sense here
}


class Protocol(Node):
    def __init__(self, loc):
        Node.__init__(self, loc)
        self.name = None
        self.namespaces = [ ]
        self.sendSemantics = ASYNC
        self.managesStmts = [ ]
        self.messageDecls = [ ]
        self.transitionStmts = [ ]

    def addOuterNamespace(self, namespace):
        self.namespaces.insert(0, namespace)

    def addManagesStmts(self, managesStmts):
        self.managesStmts += managesStmts

    def addMessageDecls(self, messageDecls):
        self.messageDecls += messageDecls

    def addTransitionStmts(self, transStmts):
        self.transitionStmts += transStmts

class Namespace(Node):
    def __init__(self, loc, namespace):
        Node.__init__(self, loc)
        self.namespace = namespace

class ManagerStmt(Node):
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
        self.direction = None
        self.inParams = [ ]
        self.outParams = [ ]

    def addInParams(self, inParamsList):
        self.inParams += inParamsList

    def addOutParams(self, outParamsList):
        self.outParams += outParamsList

    def hasReply(self):
        return self.sendSemantics is SYNC or self.sendSemantics is RPC

class Param(Node):
    def __init__(self, loc, typespec, name):
        Node.__init__(self, loc)
        self.name = name
        self.typespec = typespec

class TypeSpec(Node):
    def __init__(self, loc, spec):
        Node.__init__(self, loc)
        self.spec = spec

    def basename(self):
        return self.spec.baseid

    def __str__(self):  return str(self.spec)

class QualifiedId:              # FIXME inherit from node?
    def __init__(self, loc, baseid, quals=[ ]):
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


