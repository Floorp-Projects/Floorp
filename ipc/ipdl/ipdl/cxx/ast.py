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

import copy, sys

class Visitor:
    def defaultVisit(self, node):
        raise Exception, "INTERNAL ERROR: no visitor for node type `%s'"% (
            node.__class__.__name__)

    def visitWhitespace(self, ws):
        pass

    def visitFile(self, f):
        for thing in f.stuff:
            thing.accept(self)

    def visitCppDirective(self, ppd):
        pass

    def visitBlock(self, block):
        for stmt in block.stmts:
            stmt.accept(self)

    def visitNamespace(self, ns):
        self.visitBlock(ns)

    def visitType(self, type):
        pass

    def visitTypeEnum(self, enum):
        pass

    def visitTypedef(self, tdef):
        tdef.fromtype.accept(self)
        tdef.totype.accept(self)

    def visitDecl(self, decl):
        decl.type.accept(self)

    def visitClass(self, cls):
        for inherit in cls.inherits:
            inherit.accept(self)
        self.visitBlock(cls)

    def visitInherit(self, inh):
        pass

    def visitFriendClassDecl(self, fcd):
        pass

    def visitMethodDecl(self, meth):
        for param in meth.params:
            param.accept(self)
        if meth.ret is not None:
            meth.ret.accept(self)

    def visitMethodDefn(self, meth):
        meth.decl.accept(self)
        self.visitBlock(meth)

    def visitConstructorDecl(self, ctor):
        self.visitMethodDecl(ctor)

    def visitConstructorDefn(self, cd):
        cd.decl.accept(self)
        for init in cd.memberinits:
            init.accept(self)
        self.visitBlock(cd)

    def visitDestructorDecl(self, dtor):
        self.visitMethodDecl(dtor)

    def visitDestructorDefn(self, dd):
        dd.decl.accept(self)
        self.visitBlock(dd)

    def visitExprLiteral(self, l):
        pass

    def visitExprVar(self, v):
        pass

    def visitExprPrefixUnop(self, e):
        e.expr.accept(self)

    def visitExprBinary(self, e):
        e.left.accept(self)
        e.right.accept(self)

    def visitExprAddrOf(self, eao):
        self.visitExprPrefixUnop(eao)

    def visitExprDeref(self, ed):
        self.visitExprPrefixUnop(ed)

    def visitExprCast(self, ec):
        ec.expr.accept(self)

    def visitExprSelect(self, es):
        es.obj.accept(self)

    def visitExprAssn(self, ea):
        ea.lhs.accept(self)
        ea.rhs.accept(self)

    def visitExprCall(self, ec):
        ec.func.accept(self)
        for arg in ec.args:
            arg.accept(self)

    def visitExprNew(self, en):
        self.visitExprCall(en)

    def visitExprDelete(self, ed):
        ed.obj.accept(self)

    def visitExprMemberInit(self, minit):
        self.visitExprCall(minit)

    def visitStmtBlock(self, sb):
        self.visitBlock(sb)

    def visitStmtDecl(self, sd):
        sd.decl.accept(self)

    def visitLabel(self, label):
        pass

    def visitCaseLabel(self, case):
        pass

    def visitDefaultLabel(self, dl):
        pass

    def visitStmtIf(self, si):
        si.cond.accept(self)
        si.ifb.accept(self)
        if si.elseb is not None:
            si.elseb.accept(self)

    def visitStmtSwitch(self, ss):
        ss.expr.accept(self)
        self.visitBlock(ss)

    def visitStmtExpr(self, se):
        se.expr.accept(self)

    def visitStmtReturn(self, sr):
        if sr.expr is not None:
            sr.expr.accept(self)

##------------------------------
class Node:
    def __init__(self):
        pass

    def accept(self, visitor):
        visit = getattr(visitor, 'visit'+ self.__class__.__name__, None)
        if visit is None:
            return getattr(visitor, 'defaultVisit')(self)
        return visit(self)

class Whitespace(Node):
    # yes, this is silly.  but we need to stick comments in the
    # generated code without resorting to more serious hacks
    def __init__(self, ws):
        Node.__init__(self)
        self.ws = ws
Whitespace.NL = Whitespace('\n')

class File(Node):
    def __init__(self, filename):
        Node.__init__(self)
        self.filename = filename
        # array of stuff in the file --- stmts and preprocessor thingies
        self.stuff = [ ]

    def addthing(self, thing):
        self.stuff.append(thing)

    # "look like" a Block so code doesn't have to care whether they're
    # in global scope or not
    def addstmt(self, stmt):
        self.stuff.append(stmt)

class CppDirective(Node):
    '''represents |#[directive] [rest]|, where |rest| is any string'''
    def __init__(self, directive, rest):
        Node.__init__(self)
        self.directive = directive
        self.rest = rest

class Block(Node):
    def __init__(self):
        Node.__init__(self)
        self.stmts = [ ]

    def addstmt(self, stmt):
        self.stmts.append(stmt)

##------------------------------
# type and decl thingies
class Namespace(Block):
    def __init__(self, name):
        Block.__init__(self)
        self.name = name

class Type(Node):
    def __init__(self, name, const=False, ptr=False, ref=False):
        Node.__init__(self)
        self.name = name
        self.const = const
        self.ptr = ptr
        self.ref = ref
        # XXX could get serious here with recursive types, but shouldn't 
        # need that for this codegen
    def __deepcopy__(self, memo):
        return Type(self.name, self.const, self.ptr, self.ref)

class TypeEnum(Node):
    def __init__(self, name=None):
        '''name can be None'''
        Node.__init__(self)
        self.name = name
        self.idnums = [ ]    # pairs of ('Foo', [num]) or ('Foo', None)

    def addId(self, id, num=None):
        self.idnums.append((id, num))

class Typedef(Node):
    def __init__(self, fromtype, totype):
        Node.__init__(self)
        self.fromtype = fromtype
        self.totype = totype

class Decl(Node):
    '''represents |Foo bar|, e.g. in a function signature'''
    def __init__(self, type, name):
        Node.__init__(self)
        self.type = type
        self.name = name
    def __deepcopy__(self, memo):
        return Decl(copy.deepcopy(self.type, memo), self.name)

##------------------------------
# class stuff
class Class(Block):
    def __init__(self, name, inherits=[ ],
                 interface=False, abstract=False, final=False):
        assert not (interface and abstract)
        assert not (abstract and final)
        assert not (interface and final)

        Block.__init__(self)
        self.name = name
        self.inherits = inherits
        self.interface = interface
        self.abstract = abstract
        self.final = final

class Inherit(Node):
    def __init__(self, name, viz='public'):
        Node.__init__(self)
        self.name = name
        self.viz = viz

class FriendClassDecl(Node):
    def __init__(self, friend):
        Node.__init__(self)
        self.friend = friend

class MethodDecl(Node):
    def __init__(self, name, params=[ ], ret=Type('void'),
                 virtual=False, const=False, pure=False, static=False):
        assert not (virtual and static)
        assert not pure or virtual # pure => virtual

        Node.__init__(self)
        self.name = name
        self.params = params
        self.ret = ret
        self.virtual = virtual
        self.const = const
        self.pure = pure
        self.static = static
    def __deepcopy__(self, memo):
        return MethodDecl(self.name,
                          copy.deepcopy(self.params, memo),
                          copy.deepcopy(self.ret, memo),
                          self.virtual,
                          self.const,
                          self.pure)

class MethodDefn(Block):
    def __init__(self, decl):
        Block.__init__(self)
        self.decl = decl

class ConstructorDecl(MethodDecl):
    def __init__(self, name, params=[ ]):
        MethodDecl.__init__(self, name, params=params, ret=None)

class ConstructorDefn(MethodDefn):
    def __init__(self, decl, memberinits=[ ]):
        MethodDefn.__init__(self, decl)
        self.memberinits = memberinits

class DestructorDecl(MethodDecl):
    def __init__(self, name, virtual=False):
        MethodDecl.__init__(self, name, params=[ ], ret=None,
                            virtual=virtual)
class DestructorDefn(MethodDefn):
    def __init__(self, decl):  MethodDefn.__init__(self, decl)

##------------------------------
# expressions
class ExprLiteral(Node):
    def __init__(self, value, type):
        '''|type| is a Python format specifier; 'd' for example'''
        Node.__init__(self)
        self.value = value
        self.type = type

    @staticmethod
    def Int(i):  return ExprLiteral(i, 'd')

    @staticmethod
    def String(s):  return ExprLiteral('"'+ s +'"', 's')

    @staticmethod
    def WString(s):  return ExprLiteral('L"'+ s +'"', 's')

    def __str__(self):
        return ('%'+ self.type)% (self.value)
ExprLiteral.ZERO = ExprLiteral.Int(0)

class ExprVar(Node):
    def __init__(self, name):
        Node.__init__(self)
        self.name = name

class ExprPrefixUnop(Node):
    def __init__(self, expr, op):
        self.expr = expr
        self.op = op

class ExprAddrOf(ExprPrefixUnop):
    def __init__(self, expr):
        ExprPrefixUnop.__init__(self, expr, '&')

class ExprDeref(ExprPrefixUnop):
    def __init__(self, expr):
        ExprPrefixUnop.__init__(self, expr, '*')

class ExprCast(Node):
    def __init__(self, expr, type,
                 dynamic=0, static=0, reinterpret=0, const=0, C=0):
        assert 1 == reduce(lambda a, x: a+x, [ dynamic, static, reinterpret, const, C ])

        Node.__init__(self)
        self.expr = expr
        self.type = type
        self.dynamic = dynamic
        self.static = static
        self.reinterpret = reinterpret
        self.const = const
        self.C = C

class ExprBinary(Node):
    def __init__(self, left, op, right):
        self.left = left
        self.op = op
        self.right = right

class ExprSelect(Node):
    def __init__(self, obj, op, field):
        Node.__init__(self)
        self.obj = obj
        self.op = op
        self.field = field

class ExprAssn(Node):
    def __init__(self, lhs, rhs, op='='):
        Node.__init__(self)
        self.lhs = lhs
        self.op = op
        self.rhs = rhs

class ExprCall(Node):
    def __init__(self, func, args=[ ]):
        Node.__init__(self)
        self.func = func
        self.args = args

class ExprNew(ExprCall):
    # XXX taking some poetic license ...
    def __init__(self, type, args=[ ]):
        ExprCall.__init__(self, ExprVar(type.name), args)

class ExprDelete(Node):
    def __init__(self, obj):
        Node.__init__(self)
        self.obj = obj

class ExprMemberInit(ExprCall):
    def __init__(self, member, args=[ ]):
        ExprCall.__init__(self, member, args)

##------------------------------
# statements etc.
class StmtBlock(Block):
    def __init__(self):
        Block.__init__(self)

class StmtDecl(Node):
    def __init__(self, decl):
        Node.__init__(self)
        self.decl = decl

class Label(Node):
    def __init__(self, name):
        Node.__init__(self)
        self.name = name

class CaseLabel(Node):
    def __init__(self, name):
        Node.__init__(self)
        self.name = name

class DefaultLabel(Node):
    def __init__(self):
        Node.__init__(self)

class StmtIf(Node):
    def __init__(self, cond):
        Node.__init__(self)
        self.cond = cond
        self.ifb = Block()
        self.elseb = None
    def addifstmt(self, stmt):
        self.ifb.addstmt(stmt)
    def addelsestmt(self, stmt):
        if self.elseb is None: self.elseb = Block()
        self.elseb.addstmt(stmt)

class StmtSwitch(Block):
    def __init__(self, expr):
        Block.__init__(self)
        self.expr = expr

    def addcase(self, case, block):
        '''NOTE: |case| is not checked for uniqueness'''
        self.addstmt(case)
        self.addstmt(block)

class StmtExpr(Node):
    def __init__(self, expr):
        Node.__init__(self)
        self.expr = expr

class StmtReturn(Node):
    def __init__(self, expr=None):
        Node.__init__(self)
        self.expr = expr
