/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

package com.compilercompany.ecmascript;
import java.util.*;
import java.io.StringReader;

/**
 * ConstantEvaluator
 *
 * The ConstantEvaluator evaluates all expressions to either Undefined or
 * a definite constant value. Each statement evaluates to a CompletionValue
 * if it is a compile-time constant, or a CodeValue if it is not. StatementList
 * evaluates to a ListValue which contains a sequence of CompletionValues and
 * CodeValues. Each definition evaluates to either an EmptyStatement or an 
 * ExpressionStatement, depending on whether it contains a non-constant 
 * initializer.
 */

public class ConstantEvaluator extends Evaluator implements Tokens, Attributes {

    private static final boolean debug = false;

    // Expressions

    /**
     * IdentifierNode
     */

    Value evaluate( Context context, IdentifierNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating IdentifierNode = " + node);
        }

        return new ReferenceValue(null,used_namespaces,node.name);
    }

    /**
     * QualifiedIdentifierNode
     *
     * {qualifiers:QualifierListNode name:String}
     *
     * Semantics:
     * 1. Evaluate qualifier.
     * 2. Call GetValue(Result(1).
     * 3. If Result(2) is not a namespace, then throw a TypeError.
     * 4. Create a new Reference(null,Result2),name).
     * 5. Return Result(4).
     */

    Value evaluate( Context context, QualifiedIdentifierNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating QualifiedIdentifierNode = " + node);
        }

        Value qualifier = null;
        if( node.qualifier != null ) {
            qualifier = node.qualifier.evaluate(context,this);
            qualifier = qualifier.getValue(context);
        }

        if( qualifier != null && 
            !(NamespaceType.type.includes(qualifier) || ClassType.type.includes(qualifier)) ) {
            error(context,0,"Identifier qualifier must evaluate to a namespace or type.",node.qualifier.pos());
            return UndefinedValue.undefinedValue;
        } 

        if( NamespaceType.type.includes(qualifier) ) {
            return new ReferenceValue(null,qualifier,node.name);
        } else {
            return new ReferenceValue(qualifier,modifier_namespaces,node.name);
        }
    }

    /**
     * LiteralBooleanNode
     *
     * { value:boolean }
     *
     * Semantics:
     * 1 Return the literal boolean type value.
     */

    Value evaluate( Context context, LiteralBooleanNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating LiteralBooleanNode = " + node);
        }

        Value value;

        if( node.value == true ) {
            value = BooleanValue.trueValue;
        } else {
            value = BooleanValue.falseValue;
        }

        return value;
    }

    /**
     * LiteralNullNode
     *
     * {}
     */

    Value evaluate( Context context, LiteralNullNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating LiteralNullNode = " + node);
        }

        return NullValue.nullValue;
    }

    /**
     * LiteralNumberNode
     *
     * { value:double }
     */

    Value evaluate( Context context, LiteralNumberNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return new NumberValue(node.value);
    }

    /**
     * LiteralStringNode
     *
     * { value:String }
     */

    Value evaluate( Context context, LiteralStringNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating LiteralStringNode = " + node);
        }

        return new StringValue(node.value);
    }

    /**
     * LiteralUndefinedNode
     *
     * {}
     */

    Value evaluate( Context context, LiteralUndefinedNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return UndefinedValue.undefinedValue;
    }

    /**
     * LiteralRegExpNode
     */

    Value evaluate( Context context, LiteralRegExpNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating LiteralRegExpNode = " + node);
        }

        Value value;
        Type  type;

        type  = new RegExpType(node.value);
        value = new ObjectValue(type);

        return value;
    }

    /**
     * ThisExpressionNode
     */

    Value evaluate( Context context, ThisExpressionNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating ThisExpressionNode = " + node);
        }
        return UndefinedValue.undefinedValue;
    }

    /**
     *
     */

    Value evaluate( Context context, UnitExpressionNode node ) throws Exception {
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }
        return UndefinedValue.undefinedValue;
    }

    /**
     * ParenthesizedExpressionNode
     */

    Value evaluate( Context context, ParenthesizedExpressionNode node ) throws Exception {
        if( debug ) {
            Debugger.trace("evaluating ParenthesizedExpressionNode = " + node);
        }
        return node.expr.evaluate(context,this);
    }

    /**
     * ParenthesizedListExpressionNode
     */

    Value evaluate( Context context, ParenthesizedListExpressionNode node ) throws Exception {
        if( debug ) {
            Debugger.trace("evaluating ParenthesizedListExpressionNode = " + node);
        }
        return node.expr.evaluate(context,this);
    }

    /**
     * FunctionExpressionNode
     *
     * { name : Identifier, signature : FunctionSignature, body : StatementList }
     */

    Value evaluate( Context context, FunctionExpressionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating FunctionExpressionNode = " + node);
        }

        Value value;
        Type  type;

        type  = new FunctionType("anonymous");
        value = new ObjectValue(type);

        // ISSUE: What do we do with the name?
        //Scope local = context.getLocal();
        //Slot slot   = local.add(null,name);
        //slot.type   = type;
        //slot.value  = value;
        //node.slot = slot;

        context.pushScope(value);
        node.signature.evaluate(context,this);
        if( node.body != null ) {
            node.body.evaluate(context,this);
        }
        context.popScope();

        return value;
    }

    static final void testFunctionExpression() throws Exception {

        String[] input = { 
            "(function (){});",
            "(function (x){var a;});",
            "(function f(x,y){var a; const b;});",
        };

        String[] expected = { 
            "object(function(anonymous)){namespace(default)={}}",
            "object(function(anonymous)){namespace(_parameters_)={x={ attrs=0, type=any, value=undefined }}, namespace(default)={a={ attrs=0, type=any, value=undefined }}}",
            "object(function(anonymous)){namespace(_parameters_)={x={ attrs=0, type=any, value=undefined }, y={ attrs=0, type=any, value=undefined }}, namespace(default)={b={ attrs=0, type=any, value=undefined }, a={ attrs=0, type=any, value=undefined }}}",
        };

        testEvaluator("FunctionDefinition",input,expected);
    }

    /**
     * LiteralObjectNode
     */

    Value evaluate( Context context, LiteralObjectNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating LiteralObjectNode = " + node);
        }

        Value value;
        Type  type;

        type  = new ObjectType("");
        value = new ObjectValue(type);

        if( node.fieldlist != null ) {
            context.pushScope(value);
            node.fieldlist.evaluate(context,this);
            context.popScope();
        }

        return value;
    }

    static final void testObjectLiteral() throws Exception {

        String[] input = { 
            "{};",
            "{a:1,b:2,c:3};",
            "{'a':1,2:2,c:3};",
        };

        String[] expected = { 
            "object(){namespace(default)={}}",
            "object(){namespace(default)={b={ attrs=0, type=any, value=2 }, a={ attrs=0, type=any, value=1 }, c={ attrs=0, type=any, value=3 }}}",
            "object(){namespace(default)={a={ attrs=0, type=any, value=1 }, 2={ attrs=0, type=any, value=2 }, c={ attrs=0, type=any, value=3 }}}",
        };

        testEvaluator("ObjectLiteral",input,expected);
    }

    /**
     * LiteralFieldNode
     */

    Value evaluate( Context context, LiteralFieldNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating LiteralFieldNode = " + node);
        }

        String name;
        Scope  scope;
        Value  value;

        
        value = node.name.evaluate(context,this);
        if( value instanceof ReferenceValue ) {
            name = ((ReferenceValue)value).getName();
        } else {
            name = value.toString();
        }
        value = node.value.evaluate(context,this).getValue(context);

        Slot slot;
        scope = context.getLocal();
        slot = scope.add(null,name);
        slot.value = value;
        return value;
    }

    /**
     * LiteralArrayNode
     */

    Value evaluate( Context context, LiteralArrayNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating LiteralArrayNode = " + node);
        }

        Value value;
        Type  type;

        type = ArrayType.type;

        if( node.elementlist != null ) {
            value = node.elementlist.evaluate(context,this);
        } else {
            value = NullValue.nullValue;
        }

        return type.convert(context,value);
    }

    static final void testArrayLiteral() throws Exception {

        String[] input = { 
            "[];",
            "[1,2,3];",
            "['a',{a:1,b:2,c:3},3];",
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testEvaluator("ArrayLiteral",input,expected);
    }

    /**
     * PostfixExpressionNode
     *
     * { op:{incr,decr}, expr:PostfixExpressionNode }
     */

    Value evaluate( Context context, PostfixExpressionNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating PostfixExpressionNode = " + node);
        }

        Value value;

        value = node.expr.evaluate(context,this).getValue(context);
        
        if(node.op != 0) {
            value = NumberType.type.convert(context,value);

            // ACTION: do operation here
        }
        
        return value;
    }

    /**
     * NewExpressionNode
     *
     * { expr:NewSubexpressionNode, args:ListNode
     *
     * Constant Semantics:
     *  1. Evaluate args. (To resolve the type annotations.)
     *  2. Return undefined. (Because this expression cannot be a constant.)
     *
     * Run-time Semantics:
     *  1. Evaluate expr.
     *  2. Evaluate args.
     *  3. If Result(1) is a Type, go to step 12.
     *  4. If Result(1) is a FunctionType, go to step 9.
     *  5. If Result(1) is a Constructor, go to step 7.
     *  6. Throw TypeError.
     *  7. Call [[Construct]] method of Result(1).
     *  8. Return Result(7).
     *  9. Push a new Object onto the to of the operand stack.
     * 10. Call [[Call]] method of Result(1).
     * 11. Return Result(10).
     * 12. Get constructor of same name as type from Result(1).
     * 13. Call [[Construct]] method of Result(12).
     * 14. Return Result(13).
     */

    Value evaluate( Context context, NewExpressionNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating NewExpressionNode = " + node);
        }
        
        if( node.expr != null ) {
            node.expr.evaluate(context,this);
        }

        return UndefinedValue.undefinedValue;
    }

    static final void testNewExpression() throws Exception {

        String[] input = { 
            "new Number;",
            "new Number(x);",
            "class C {function C(){}}; new C(x);",
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testEvaluator("NewExpression",input,expected);
    }

    /**
     * MemberExpressionNode
     *
     * { base:NewSubexpressionNode name:IdentifierNode }
     */

    Value evaluate( Context context, MemberExpressionNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating MemberExpressionNode = " + node);
        }

        Value          value;
        Value          base;
        ReferenceValue ref;
        String         name;

        base  = node.base.evaluate(context,this).getValue(context);
        value = node.name.evaluate(context,this);

        if( value instanceof ReferenceValue ) {
            ref       = (ReferenceValue) value;
            ref.scope = base;
        } else {
            error(context,0,"Expecting reference expression after dot operator.",node.pos());
        }

        return value;
    }

    static final void testMemberExpression() throws Exception {

        String[] input = { 
            "o.x;",
        };

        String[] expected = { 
            "",
            "",
        };

        testEvaluator("MemberExpression",input,expected);
    }

    /**
     * IndexedMemberExpressionNode
     *
     * Syntax:
     * { base:NewSubexpressionNode args:ArgumentList }
     *
     * Semantics:
     * 1. Evaluate base.
     * 2. Call GetValue(Result(1)).
     * 3. Evaluate args.
     * 4. Call GetValue(Result(3)).
     * 5. Call ToString(Result(4)).
     * 6. If Result(1) or Result(5) is undefined, then return undefined.
     * 7. Return new ReferenceValue(Result(2),Result(5)).
     */

    Value evaluate( Context context, IndexedMemberExpressionNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating IndexedMemberExpressionNode = " + node);
        }

        return UndefinedValue.undefinedValue;
    }

    static final void testIndexedMemberExpression() throws Exception {

        String[] input = { 
            "o[x];",
        };

        String[] expected = { 
            "",
        };

        testEvaluator("IndexedMemberExpression",input,expected);
    }

    /**
     * ClassofExpressionNode
     *
     * { base }
     */

    Value evaluate( Context context, ClassofExpressionNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating ClassofExpressionNode = " + node);
        }

        Value value = node.base.evaluate(context,this);

        // If this is a reference value, then the type of the referenced
        // slot is returned. If it is a regular value, the type of the value
        // is returned.

        return value.getType(context);
    }

    /**
     * CoersionExpressionNode
     *
     * { expr, type }
     *
     * Semantics:
     * 1 Evaluate type.
     * 2 If Result(1) is not a compile-time constant type value, 
     *   post an error and return undefined.
     * 3 Evaluate expr.
     * 4 Call the coerce method of Result(1) with argument Result(3).
     * 5 Return Result(4).
     */

    Value evaluate( Context context, CoersionExpressionNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating CoersionExpressionNode = " + node);
        }

        Value ref;
        ref = node.type.evaluate(context,this);

        Value typeValue;
        typeValue = ref.getValue(context);
        Value expr = UndefinedValue.undefinedValue;

        if( !TypeType.type.includes(typeValue) ) {
            error(context,"A constant type expression expected on right hand side of coerce operator." +
                                typeValue,node.pos());
        } else {
            Value value;
            expr  = node.expr.evaluate(context,this);
            value = ((TypeValue)typeValue).coerce(context,expr);
        }

        return expr;
    }

    static final void testCoersionExpression() throws Exception {

        String[] input = { 
            "class T{} o@T;",
            "o@(T);",
        };

        String[] expected = { 
            "",
            "",
        };

        testEvaluator("CoersionExpression",input,expected);
    }

    /**
     * CallExpressionNode
     *
     * { member: args:ArgumentListNode }
     *
     * Semantics:
     * 1. Evaluate
     */

    Value evaluate( Context context, CallExpressionNode node ) throws Exception { 
        
        if( debug ) {
            Debugger.trace("evaluating CallExpressionNode = " + node);
        }

        Value callee;
        Value args = null;

        callee = node.member.evaluate(context,this).getValue(context);

        if( node.args != null ) {
            args  = node.args.evaluate(context,this);
        }

        if( !FunctionType.type.includes(callee) ) {
            return UndefinedValue.undefinedValue;
        }

        // This may be a pure function. In which case the result value is constant as
        // long as the arguments are constant.

        Value value;
        context.enterClass(callee);
        value = callee.call(context,args);
        context.exitClass();

        return value;
    }

    /**
     * UnaryExpressionNode
     */

    Value evaluate( Context context, UnaryExpressionNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating UnaryExpressionNode = " + node);
        }

        return UndefinedValue.undefinedValue;
    }

    /**
     * BinaryExpressionNode
     */

    Value evaluate( Context context, BinaryExpressionNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating BinaryExpressionNode = " + node);
        }
        return UndefinedValue.undefinedValue;
    }

    /**
     * ConditionalExpressionNode
     */

    Value evaluate( Context context, ConditionalExpressionNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating ConditionalExpressionNode = " + node);
        }

        return UndefinedValue.undefinedValue;
    }

    /**
     * AssignmentExpressionNode
     */

    Value evaluate( Context context, AssignmentExpressionNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating AssignmentExpressionNode = " + node);
        }

        Value lref = node.lhs.evaluate(context,this);

        Value value = UndefinedValue.undefinedValue;
        if( lref != UndefinedValue.undefinedValue ) {
            Slot slot = ((ReferenceValue)lref).getSlot(context);
            Debugger.trace("slot " + slot);
            if( slot != null ) {
                Value rref = node.rhs.evaluate(context,this);
                slot.value = rref.getValue(context);
            }
        }

        return value;
    }

    /**
     * ListNode
     */

    Value evaluate( Context context, ListNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating ListNode = " + node);
        }

        ListValue list;

        if( node.list != null ) {
            if( node.list instanceof ListNode ) {
                list = (ListValue) node.list.evaluate(context,this);
            } else {
                list = new ListValue();
                list.push(node.list.evaluate(context,this));
            }
        }
        else {
            list = new ListValue();
        }

        if( node.item != null ) {
            list.push(node.item.evaluate(context,this));
        }
        else {
            // do nothing.
        }

        return list;

    }

    // Statements

    // When evaluating statement nodes do:
    // 1. determine leaders.
    // 2. determine exits.
    // 3. determine edges.
    // 4. 


    /**
     * StatementListNode
     */

    Value evaluate( Context context, StatementListNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating StatementListNode = " + node);
        }

        ListValue list;

        if( node.list != null ) {
            if( node.list instanceof StatementListNode ) {
                list = (ListValue) node.list.evaluate(context,this);
            } else {
                list = new ListValue();
                list.push(node.list.evaluate(context,this));
            }
        }
        else {
            list = new ListValue();
        }

        if( node.item != null ) {

            Node              item = node.item;
            StatementListNode prev = (StatementListNode)node.list;

            // Add Edges.

            if( prev != null && prev.item.block != null && prev.item.isBranch() ) {
                //Debugger.trace("prev.item.block = " + prev.item.block + ", item = " + item);
                context.addEdge(prev.item.block,item.block);
            } 
            
            if( item.isBranch() ) {
                Node[] targets = item.getTargets();
                if( targets!=null ) { 
                    for(int i=0;i<targets.length;i++) {
                    Debugger.trace("item.block = " + item.block + ", targets["+i+"] = " + targets[i]);
                        context.addEdge(item.block,targets[i].block);
                    }
                }
            }

            if( item.block!=null ) {
                context.setBlock(item.block);
            }

            list.push(node.item.evaluate(context,this));
        }
        else {
            // do nothing.
        }

        return list;

    }

    /**
     * EmptyStatementNode
     */

    Value evaluate( Context context, EmptyStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating EmptyStatementNode = " + node);
        }

        return new CompletionValue(CompletionType.normalType,null,null); // <normal,empty,empty>
    }

    /**
     * ExpressionStatementNode
     *
     * Semantics:
     * 1. Evaluate expr.
     * 2. If Result(1) is not-a-constant, then return new CodeValue(node).
     * 3. else return new CompletionValue(normal,Result(1),empty)
     */

    Value evaluate( Context context, ExpressionStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating ExpressionStatementNode = " + node);
        }

        Value value;
        value = node.expr.evaluate(context,this).getValue(context);
        if( value == UndefinedValue.undefinedValue) {
            value = new CodeValue(node);
        } else {
            value = new CompletionValue(CompletionType.normalType,value,null);
        }
        
        return value;
    }

    /**
     * AnnotatedBlockNode
     *
     * Syntax:
     * { attributes:list statements:list }
     * 
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, AnnotatedBlockNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating AnnotatedBlockNode = " + node);
        }
        context.addEdge(node.block,node.statements.first().block);

        return UndefinedValue.undefinedValue;
    }

    /**
     * LabeledStatementNode
     * 
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, LabeledStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating LabeledStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * IfStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, IfStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        node.condition.evaluate(context,this);
        node.thenactions.evaluate(context,this);
        if(node.elseactions!=null)node.elseactions.evaluate(context,this);

        return new CodeValue(node);
    }

    /**
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, SwitchStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * DoStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, DoStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating DoStatementNode = " + node);
        }
        context.addEdge(node.block,node.statements.first().block);
        node.statements.evaluate(context,this);

        return new CodeValue(node);
    }

    /**
     * WhileStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, WhileStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating WhileStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * ForInStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, ForInStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * ForStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, ForStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating ForStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * WithStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, WithStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating WithStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * ContinueStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, ContinueStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating ContinueStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * BreakStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, BreakStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating BreakStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * ReturnStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, ReturnStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating ReturnStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * ThrowStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, ThrowStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating ThrowStatementNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * TryStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, TryStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * CatchClauseNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, CatchClauseNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating CatchClauseNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * FinallyClauseNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, FinallyClauseNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating FinallyClauseNode = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * ImportDefinitionNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, ImportDefinitionNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * ImportBindingNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, ImportBindingNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return new CodeValue(node);
    }

    /**
     * UseStatementNode
     *
     * Semantics:
     * 1. Return the current statement as a code value.
     */

    Value evaluate( Context context, UseStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        return new CodeValue(node);
    }

    /*
     * Definitions
     */


    /**
     * namespaces is used to store namespace attributes for the
     * current definition being evaluated.
     */

    private Vector  used_namespaces = new Vector();
    private Vector  modifier_namespaces;
    private Vector  parameters = new Vector();
    private Value   modifier_attributes;
    private Scope   modifier_scope;
    private boolean modifier_static;
    private boolean modifier_extend;
	private static Value namespace_parameters=null;

    static {
    try {
    	namespace_parameters = new ObjectValue(new NamespaceType("_parameters_"));
	} catch ( Exception x ) {
	    x.printStackTrace();
		Debugger.trace("oops, something is not right.");
    }
	}

    /**
     * AnnotatedDefinition
     *
     * Semantics:
     * 1 Evaluate attributes to get a ListValue containing the value of 
     *   each attribute.
     * 2 Evaluate the definition using the attributes as modifiers.
     */

    public Value evaluate( Context context, AnnotatedDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "evaluating AnnotatedDefinitionNode: " + node );
        }

        modifier_scope      = null;
        modifier_static     = false;
        modifier_extend     = false;
        modifier_namespaces = new Vector();

        Value value;
        Scope local = context.getLocal();

        // The following statement has the side-effect of building the
        // various attributes arrays in this evaluator object.

        Vector prev_namespaces = modifier_namespaces;
        modifier_namespaces = (Vector)prev_namespaces.clone();

        try {

            // The following statement has the side-effect of building the
            // various attributes arrays in this evaluator object.

            if( node.attributes != null ) {
                modifier_attributes = node.attributes.evaluate(context,this);
                if( modifier_attributes.getValue(context)==BooleanValue.falseValue ) {
                    return UndefinedValue.undefinedValue;
                }
            }

            value = node.definition.evaluate(context,this);

        } finally {
            modifier_namespaces=prev_namespaces;
            if( modifier_extend ) {
                context.exitClass();
            }
        }

        return value;
    }

    /**
     * AttributeListNode
     *
     * Semantics:
     * 1 For each attribute:
     * 1.0 If value is undefined, post error and exit current annotated definition.
     * 1.1 If kind is conditional and value is false, exit.
     * 1.2 If kind is scope, set context.scope to the corresponding scope object.
     * 1.3 If kind is namespace, add value to namespace set.
     * 1.4 If kind is visibility, add value to visibility modifiers.
     * 1.5 If kind is class, add value to class modifiers.
     * 1.6 If kind is member, add value to member modifiers.
     * 1.7 If kind is misc, add value to misc modifiers.
     * 1.8 If type is list, apply Step 1 to each member of list.
     */

    public Value evaluate( Context context, AttributeListNode node ) throws Exception { 

        if( debug ) { 
            Debugger.trace( "evaluating AttributeListNode: " + node );
        }

        Value val    = node.item.evaluate(context,this).getValue(context);
        Scope global = context.getGlobal();

        // Do the current attribute.

        if( UndefinedType.type.includes(val) ) {
            error(context,"The attribute is not a compile-time constant.",node.item.pos());
        } else if( val==BooleanValue.falseValue ) {
            return BooleanValue.falseValue; // This means stop processing definition.
        } else if( val == global.get(null,"local").value ) {
            modifier_scope = context.getLocal();                
        } else if( val == global.get(null,"global").value ) {
            modifier_scope = context.getGlobal();
        } else if( val == global.get(null,"static").value ) {
            modifier_static = true;
            modifier_scope  = context.getThisClass();
        } else if( val == global.get(null,"unit").value ) {
            modifier_extend = true;
            context.enterClass(global.get(null,"Unit").value);
        } else if( NamespaceType.type.includes(val) ) {
            modifier_namespaces.addElement(val);
        } else if(isVisibility(val)) {
        } else if(isClass(val)) {
        } else if(isMember(val)) {
        } else if(isMiscellaneous(val)) {
        } else if(isUser(val)) {
        } else if( node.item instanceof CallExpressionNode ) {
            CallExpressionNode callnode = (CallExpressionNode) node.item;
            val = callnode.member.evaluate(context,this).getValue(context);
            if( val == global.get(null,"extend").value ) {
                modifier_extend = true;
                Scope scope = callnode.args.evaluate(context,this).getValue(context);
                context.enterClass(scope); // ISSUE: don't forget to exit this scope.
                Debugger.trace("extend " + scope);
            }
        }

        ListValue list;

        if( node.list != null ) {
            list = (ListValue)node.list.evaluate(context,this);
        } else {
            list = new ListValue();
        }

        list.push(val);

        return list;
    }

    static boolean isConditional(Value val) {
        return BooleanType.type.includes(val);
    }

    static boolean isScope(Value val) {
        return false;
    }

    static boolean isNamespace(Value val) {
        return NamespaceType.type.includes(val);
    }

    static boolean isVisibility(Value val) {
        return false;
    }

    static boolean isClass(Value val) {
        return false;
    }

    static boolean isMember(Value val) {
        return false;
    }

    static boolean isMiscellaneous(Value val) {
        return false;
    }

    static boolean isUser(Value val) {
        return false;
    }


    /**
     *
     */

    Value evaluate( Context context, ExportDefinitionNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }
        return UndefinedValue.undefinedValue;
    }

    /**
     *
     */

    Value evaluate( Context context, ExportBindingNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }
        return UndefinedValue.undefinedValue;
    }

    /**
     * VariableDefinitionNode
     *
     * Semantics:
     * 1. Evaluate VariableBinding list.
     * 2. If kind is const, then verify that all values are constant.
     * 3. Return list.
     */

    Value evaluate( Context context, VariableDefinitionNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating VariableDefinitionNode = " + node);
        }

        Value value;

        if( modifier_extend && !modifier_static && node.kind!=const_token ) {
            error(context,"class extension is not static or const.",node.pos());
        }

        value = node.list.evaluate(context,this);

        ListValue list = new ListValue();

        Enumeration e = ((ListValue)value).elements();
        while(e.hasMoreElements()) {
            value = ((Value) e.nextElement()).getValue(context);
            if( value instanceof CodeValue ) {
                if( node.kind==const_token ) {
                    error(context,"const initializer is not a compile-time constant.",node.pos());
                }
                list.push(value);
            } else {
                // Do nothing.
            }
        }

        if( debug ) {
            Debugger.trace("evaluating VariableDefinitionNode list = " + list);
        }

        return list;
    }
/*
    static final void testVariableDefinition() throws Exception {

        String[] input = { 
            "var x; x;",
            "var y:Number; y;",
            "var y:T; y;",
            "class C; class C{}; var x, y:C; y;",
            "namespace V1; attribute v1=namespace(V1); class C; class C{} v1 var x:C; v1::x;",
            "var x = 1; const y = 2; var z = n;",
            "var x = 1,y = 2,z = n;",
            "const z = n;",
        };

        String[] expected = { 
            "{ attrs=0, type=type(any), value=undefined }",
            "{ attrs=0, type=type(number), value=undefined }",
            "Type expression does not evaluate to a type: undefined",
            "{ attrs=0, type=type(class_C), value=undefined }",
            "{ attrs=0, type=type(class_C), value=undefined }",
            "",
            "",
            "",
        };

        testEvaluator("VariableDefinition",input,expected);
    }
*/

    /**
     * VariableBindingNode
     *
     * Syntax:
     * {variable:TypedVariable, initializer:AssignmentExpression}
     *
     * Semantics:
     * 1. Evaluate identifier.
     * 2. Evaluate initializer.
     * 3. If initializer is constant, update slot value and return undefined.
     * 4. Otherwise, create a code value for initializer and return it.
     */

    Value evaluate( Context context, VariableBindingNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("evaluating VariableBindingNode = " + node);
        }

        node.variable.evaluate(context,this);

        Value value;
        if( node.initializer != null ) {
            value = node.initializer.evaluate(context,this).getValue(context);
            if( value == UndefinedValue.undefinedValue ) {
                value = new CodeValue(new ExpressionStatementNode(
                             new AssignmentExpressionNode(node.variable.identifier,assign_token,node.initializer)));
                node.variable.slot.value = value;
            } else {
                node.variable.slot.value = value;
                // return undefined, to mean that there is nothing more to do.
                value = UndefinedValue.undefinedValue;
            }
        } else {
            value = UndefinedValue.undefinedValue;
        }        

        return value;
    }

    /**
     * TypedVariableNode
     *
     * Semantics:
     * 1 Add to the designated scope, a slot for each namespace and name.
     * 2 Set each slot access methods according to the member modifiers.
     * 3 Set each slot's attrs field to the collection of remaining values.
     * 
     * 1 For each namespace modifier:
     * 1.1 Create a new slot.
     * 1.2 
     */

    Value evaluate( Context context, TypedVariableNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating TypedVariableNode = " + node);
        }

        String name;
        Value  type  = null;
        Scope  scope = modifier_scope==null?context.getLocal():modifier_scope;

        ReferenceValue ref;
         
        ref  = (ReferenceValue) node.identifier.evaluate(context,this);
        name = ref.getName();

        if( modifier_namespaces.size()==0 ) {
            Slot slot = scope.add(null,name); // ISSUE: should the namespace be public?
            node.slot = slot;
            node.slot.attrs = modifier_attributes;
        } else {
            Value namespace;
            Enumeration e = modifier_namespaces.elements();
            while( e.hasMoreElements() ) {
                namespace = (Value) e.nextElement();
                Slot slot = scope.add(namespace,name);
                node.slot = slot;
                node.slot.attrs = modifier_attributes;
            }
        }

        Value typeValue = ObjectType.type;
        if(node.type!=null) {
            typeValue = node.type.evaluate(context,this).getValue(context);
                // the value of the type expression is the type of the variable.
        }

        if( debug ) {
            //Debugger.trace("evaluating TypedVariableNode() with typeValue = " + typeValue);
        }

        if( !TypeType.type.includes(typeValue) ) {
            error(context,"Type expressions must have compile-time constant values: " + typeValue.type,node.pos());
        } else {
            node.slot.type = typeValue;
        }

        return ref;
    }

    /**
     * FunctionDefinition
     *
     * Semantics:
     * 1. Do constant evaluation of the function body.
     * 2. Save result in code slot.
     */

    public Value evaluate( Context context, FunctionDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "evaluating FunctionDefinitionNode: " + node );
        }

        Value value = node.decl.evaluate(context,this);

        if( node.body!=null) {
            context.addEdge(node.block,node.body.first().block);
            context.pushScope(value);
            Slot slot  = value.add(null,"_code_");
            slot.value = node.body.evaluate(context,this);
            slot.type  = ObjectType.type;
            slot.attrs = modifier_attributes; // ACTION: do attrs
            context.popScope();
        }

        return UndefinedValue.undefinedValue;
    }

/*
    static final void testFunctionDefinition() throws Exception {

        String[] input = { 
            "function f(){};",
            "function f(x){var a;};",
            "namespace V; attribute v=namespace(V);v function f(x,y){var a; const b;};",
            "function f(x:Integer):Number{var a;};",
        };

        String[] expected = { 
            "",
            "",
            "",
            "",
        };

        testEvaluator("FunctionDefinition",input,expected);
    }
*/

    /**
     * FunctionDeclaration
     *
     * Semantics:
     * 1. Push the function object on the scope stack.
     * 2. Do constant evaluation of the signature.
     */

    public Value evaluate( Context context, FunctionDeclarationNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "evaluating FunctionDeclarationNode: " + node );
        }

        ReferenceValue ref = (ReferenceValue) node.name.evaluate(context,this);
        String name = ref.getName();
        ref.scope = context.getLocal();

        // 1. Get the declaration for the function being defined.

        Value value = ref.getValue(context);

        if( value==UndefinedValue.undefinedValue ) {

            if( debug ) {
                Debugger.trace( "no function decl, inserting: " + name );
            }

            Type type;

            type  = new FunctionType(name);
            value = new ObjectValue(type);

            Scope scope = context.getLocal();

            if( modifier_namespaces.size()==0 ) {

                Slot slot = scope.add(null,name);
                slot.type  = FunctionType.type;
                slot.value = value;
                slot.block = context.getBlock();

                node.slot = slot;

            } else {
                Value namespace;
                Enumeration e = modifier_namespaces.elements();
                while( e.hasMoreElements() ) {
                    namespace = (Value) e.nextElement();
                    Slot slot;
                    slot = scope.add(namespace,name);
                    slot.type  = FunctionType.type;
                    slot.value = value;
                    slot.block = context.getBlock();

                    node.slot = slot;
                }
            }


            context.pushScope(value);
            node.signature.evaluate(context,this);
            context.popScope();

        }

        return value;
    }

/*
    static final void testFunctionDeclaration() throws Exception {

        String[] input = { 
            "function f();",
            "function f(x);",
            "function f(x:Integer,y:String):Number;",
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testEvaluator("FunctionDeclaration",input,expected);
    }
*/

    /**
     * FunctionNameNode
     */

    Value evaluate( Context context, FunctionNameNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("defining FunctionNameNode = " + node);
        }

        return node.name.evaluate(context,this); 
    }

    /**
     * FunctionSignature
     *
     * Semantics:
     * 1. Evaluate the parameter signature.
     * 2. Evaluate the result signature.
     * 3. If the result signature is missing, the result type is Any.
     * 4. If the result type value is not a type, post an error.
     * 5. Update the type field of the _result_ slot.
     */

    public Value evaluate( Context context, FunctionSignatureNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "evaluating FunctionSignatureNode: " + node );
        }

        Value params;
        if( node.parameter!=null) {
             params = node.parameter.evaluate(context,this);
        } else {
             params = UndefinedValue.undefinedValue;
        }

        Scope local = context.getLocal();
        node.slot = local.add(namespace_parameters,"_result_");

        Value result;
        if( node.result!=null ) {
            result = node.result.evaluate(context,this).getValue(context);
        } else {
            result = ObjectType.type;
        }

        if( !TypeType.type.includes(result) ) {
            error(context,"Result type expression does not evaluate to a type. result = " + result.type,node.pos());
        }

        node.slot.type  = (TypeValue) result;

        return result;
    }

    /**
     * ParameterNode
     *
     * Semantics:
     * 1. Evaluate typed identifier.
     */

    Value evaluate( Context context, ParameterNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace( "defining ParameterNode: " + node );
        }

        String name;
        Scope  local = context.getLocal();

        ReferenceValue ref;
         
        ref  = (ReferenceValue) node.identifier.evaluate(context,this);
        name = ref.getName();

        node.slot = local.add(namespace_parameters,name);
        //node.identifier.slot.block = context.getBlock();

        return ref;
    }

    /**
     * OptionalParameterNode
     *
     * Semantics:
     * 1. Evaluate typed identifier.
     */

    Value evaluate( Context context, OptionalParameterNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace( "defining OptionalParameterNode: " + node );
        }

        String name;
        Scope  local = context.getLocal();

        node.parameter.evaluate(context,this);
        node.initializer.evaluate(context,this);

        return UndefinedValue.undefinedValue;
    }

    /**
     * ClassDefinitionNode
     */

    Value evaluate( Context context, ClassDefinitionNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating ClassDefinitionNode = " + node);
        }

        // Get the declaration for the class being defined.

        ReferenceValue ref = (ReferenceValue) node.name.evaluate(context,this);
        Value      value  = ref.getValue(context);   // Looking up the name.

        if( value==UndefinedValue.undefinedValue ) {

            Node   decl;
            String name  = ref.getName();

            if( debug ) {
                Debugger.trace( "no class decl, inserting: " + name );
            }

            decl  = NodeFactory.ClassDeclaration(NodeFactory.Identifier(name));
            value = decl.evaluate(context,this).getValue(context);
        }

        // Store a reference to the slot in the node, for quick access
        // during the next pass.

        node.slot = ref.getSlot(context);

        // Push the new object type onto the scope stack, etc.

        context.enterClass(value);

        // Establish the inheritance relationship with the base class.
        
        if( node.inheritance.baseclass!=null ) { 
            node.inheritance.baseclass.evaluate(context,this);
        }

        // Do the statements. In particular, do all the definitions
        // contained in the statement list.

        if( node.statements != null ) {
            Slot slot;
            slot       = value.add(null,"_code_");
            slot.type  = ObjectType.type;
            slot.value = node.statements.evaluate(context,this);
            slot.attrs = modifier_attributes; // ACTION: do attrs.
            value = slot.value;
        } else {
            value = new CompletionValue();
        }

        // Pop the current object off the scope stack.

        context.exitClass();

        return value;
    }

    /**
     * ClassDeclarationNode
     */

    public Value evaluate( Context context, ClassDeclarationNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining ClassDeclarationNode: " + node );
        }

        ReferenceValue reference = (ReferenceValue) node.name.evaluate(context,this);
        String         name      = reference.getName();

        // 1. Create a new type value to represent the class currently
        //    being declared. The type of this type value is ObjectType.type.
        //    The value represents the class including all of its declared
        //    members, superclass, and code block.

        Value value = new TypeValue(new ClassType(name));

        // 2. Put the new value into the inner-most scope.

        Scope scope = context.getLocal();
        Slot slot   = scope.add(null,name);
        slot.value  = value;
        slot.type   = ObjectType.type;
        slot.attrs  = modifier_attributes; // ACTION: do attrs.

        return value;
    }

    /**
     * Superclass
     *
     * 1. Get the type object for the superclass.
     * 2. Get the type object for the derived class (getThisClass).
     * 3. Set the superclass of the derived class.
     * 4. Add the derived class to the subtype set of the super.
     */

/*
    public Value evaluate( Context context, SuperclassNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining SuperclassNode: " + node );
        }

        Value value = node.expr.evaluate(context,this).getValue(context);

        if( ClassType.type.includes(value) ) {
            error(context,"superclass must be an object type, not a " + value.type,node.pos());
        }

        TypeValue superType = (TypeValue) value;
        TypeValue subType = (TypeValue) context.getThisClass();

        subType.setSuper(superType);
        superType.addSub(subType);

        return superType;
    }
*/
    /**
     * NamespaceDefinitionNode
     */

    Value evaluate( Context context, NamespaceDefinitionNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace( "defining NamespaceDefinitionNode: " + node );
        }

        ReferenceValue reference = (ReferenceValue) node.name.evaluate(context,this);
        String name = reference.getName();

        // Create a namespace instance.

        Value value = new ObjectValue(new NamespaceType(name));

        // ACTION: do the extends list.

        // Put the new namespace into the current local scope.

        Scope local = context.getLocal();
        Slot slot   = local.add(null,name);
        slot.type   = NamespaceType.type;
        slot.value  = value;
        slot.attrs  = null; // ACTION: do attrs.
        slot.block  = context.getBlock();

        return value;
    }

    /**
     * PackageDefinitionNode
     */

    Value evaluate( Context context, PackageDefinitionNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating PackageDefinitionNode = " + node);
        }

        return UndefinedValue.undefinedValue;
    }

    /**
     * ProgramNode
     */

    Value evaluate( Context context, ProgramNode node ) throws Exception {

        if( debug ) {
            Debugger.trace("evaluating ProgramNode = " + node);
        }

        Scope scope;
        Slot slot;

        Vector prev_namespaces = used_namespaces;
        used_namespaces = (Vector)prev_namespaces.clone();

        scope      = context.getGlobal();
        slot       = scope.add(null,"_code_");
        slot.value = node.statements.evaluate(context,this);
        slot.type  = ObjectType.type;
        slot.attrs = null; // ACTION: do attrs

        used_namespaces = prev_namespaces;

        return slot.value;
    }

/*
    static final void testProgram() throws Exception {

        String[] input = { 
            "var x = 1; function f() {} class C {}; f(); x;",
        };

        String[] expected = { 
            "",
        };

        testEvaluator("Program",input,expected);
    }
*/

    /**
     * error
     *
     * error handler for semantic analysis.
     */

    public final void error(Context context, String msg, int pos) {
        error(context,0,msg,pos);
    }

    public final void error(Context context, int kind, String msg, int pos) {
        context.errors++;
        InputBuffer in = context.getInput();
        String loc = "Ln " + (in.getLnNum(pos)+1) + ", Col " + (in.getColPos(pos)+1) + ": ";
        System.err.println(loc+msg);
        System.err.println(context.getInput().getLineText(pos));
        System.err.println(getLinePointer(context.getInput().getColPos(pos)));
        //skiperror(kind);
    }

    public final String getLinePointer(int pos) {
        StringBuffer padding = new StringBuffer();
        for( int i=0 ; i < pos ; i++ ) {
            padding.append( " " );
        }
        return new String(padding + "^");
    }

    /**
     * Main. Runs self diagnostics.
     */

    static int failureCount = 0;

    public static void main( String[] args ) {
        try {

            Debugger.trace( "Parser test begin" );

            testFunctionExpression();
            testObjectLiteral();
            testArrayLiteral();
            testNewExpression();
            testMemberExpression();
            testCoersionExpression();

            //testProgram();

            if( failureCount != 0 )
                Debugger.trace( "ConstantEvaluator test completed: " + failureCount + " tests failed" );
            else
                Debugger.trace( "ConstantEvaluator test completed successfully" );
            }
            catch( Exception e ) {
                //Debugger.trace( "compile error: " + e );
                e.printStackTrace();
        }
        finally {
        }
    }

    static final void testEvaluator(String name, String[] input, String[] expected) throws Exception {

        Debugger.trace( "begin testEvaluator: " + name );

        String result;

        Node node;
        Value type;
        Evaluator evaluator;

        Class pc = Parser.class;
        Class[] args = new Class[0];

        Parser parser;
        long t=0;

        ObjectValue global = null;

        for( int i = 0; i < input.length; i++ ) {

            Context context = new Context();
 
            try {

                global = new ObjectValue("__systemGlobal", new GlobalObject());
                context.enterClass(global);

                parser    = new Parser(new StringReader(input[i]));
                Evaluator devaluator = new BlockEvaluator();
                Evaluator cevaluator = new ConstantEvaluator();

                System.gc();
	            t = System.currentTimeMillis();
                node = parser.parseProgram();
                Debugger.trace("    I: "+global);
                context.evaluator = devaluator;
                node.evaluate(context, devaluator);
                Debugger.trace("    D: "+global);
                context.evaluator = cevaluator;
                Value value = node.evaluate(context, cevaluator);
                Debugger.trace("    C: "+global);
                Debugger.trace("    Dom: "+context.D);
                value = value.getValue(context);
                result = value.toString();

	            t = System.currentTimeMillis() - t;
                context.exitClass();

            } catch( Exception x ) {
                x.printStackTrace();
	            t = System.currentTimeMillis() - t;
                result = "error("+x.getMessage()+")";
                context.exitClass();
            }
            if( result.equals( expected[i] ) ) {
                Debugger.trace( "  " + i + " passed in " + Long.toString(t) + 
                                " msec [" + input[i] + "] = " + result );
            } else {
                failureCount++;
                Debugger.trace( "  " + i + " failed [" + input[i] + "] = " + result );
            }

        }

        Debugger.trace( "finish testEvaluator: " + name );
    }

}

/**
 * Flow
 */

class Flow {

    private static boolean debug = true;

    /**
     * Compute whether block d dominates block r.
     *
     * d dominates r when all paths to r pass through d.
     *
     * Algorithm from Aho et al (1986) p. 670:
     *
     * Input: A flow graph G and set of nodes (that is, blocks) N,
     * set of edges E and initial node n0.
     *
     * Output: The relation dom.
     *
     * Algorithm:
     *
     * 1. D(n0) = {n0}
     * 2. for n in N, {n0} do D(n) := N;
     * 3. while D(n) is changing do
     * 4.   for n in N - {n0} do
     * 5.     D(n) := {n} union (intersection D(p) {p is all predecessors of n})
     *
     * End.
     */

    /**
     * Compute whether Block d dominates Block r.
     */

    public static Hashtable getDominatorTable(Context context) {
        return context.D;
    }

    public static boolean dom(Context context, Block d, Block r) {

        Debugger.trace("Context.dom() with d = " + d + ", r = " + r );
        
        if( d == null ) {
            return false;
        }

        Vector    B = context.getBlocks();  // All blocks.

        // For now reinitialized D on each call. In the future be
        // smarter about resulting it.

        context.D = new Hashtable();      // Dom of each block.
        
        // 1. D(n0) = {n0}

        Vector domB0 = new Vector();        // Dominators of the initial block, B0.
        Block  B0    = (Block) B.elementAt(0);      // Always, only itself.
        domB0.addElement(B0);              
        context.D.put(B0,domB0);             // Initialize D(n0).

        // 2. For each block in B, starting with {b1} initialize D(b) to B.

        int max = B.size();
        for(int i = 1; i<max; i++) {
            context.D.put(B.elementAt(i),B.clone());
        }

        // 3. While D(n) does not change
        // 4.   For n in N


        // Recompute domBn for every Bn.

        boolean stillChanging;
        do {
        stillChanging = false;
        for(int i = 1; i<B.size(); i++) {
            
            // compute domBn.

            Vector domBn = new Vector();
            Block Bn = (Block) B.elementAt(i);

            // Add Bn to domBn. Every block dominates itself.

            domBn.addElement(Bn);

            // Add the intersection of the dominators of Bn's predecessors.

            Enumeration e = intersectionOfPredecessors(context,Bn).elements();
            while(e.hasMoreElements()) {
                Object el = e.nextElement();
                if( !domBn.contains(el) ) {
                    domBn.addElement(el);
                }
            }
            
            // Update domBn in the dominators table.

            Object domBn_prev = context.D.get(Bn);
            if( !domBn.equals(domBn_prev) ) {
                Debugger.trace("Bn="+Bn+", domBn="+domBn+", domBn_prev="+domBn_prev);
                stillChanging = true;
                context.D.remove(Bn);
                context.D.put(Bn,domBn);            
            }
        }

        } while ( stillChanging );

        Vector domR = (Vector) context.D.get(r);

        if( debug ) {
            Debugger.trace("leaving Context.dom() with result = " + domR.contains(d));
        }

        return domR.contains(d);
    }

    static Vector intersectionOfPredecessors(Context context, Block Bn) {

	    if( debug ) {
		    Debugger.trace("Flow.intersectionOfPredecessors() with D = " + context.D + ", Bn = " + Bn);
		}

        //Hashtable D = context.D;

        // Get the predecessors of Bn.

        Enumeration predBn = getPredecessors(context,Bn).elements();

        if( !predBn.hasMoreElements() ) {
            return new Vector();
        }

        // Get the dominators of the first predecessor of Bn.

		Vector domRest = (Vector) context.D.get(predBn.nextElement());

	    if( debug ) {
		    Debugger.trace("Flow.intersectionOfPredecessors() with domRest = " + domRest);
		}

        // Remove any blocks that are not also dominators of each
        // of the other predecessors.

        while(predBn.hasMoreElements()) {
            Enumeration domPredBn = ((Vector)context.D.get(predBn.nextElement())).elements();
            while(domPredBn.hasMoreElements()) {
                Object domPredBnn = domPredBn.nextElement();
                if( !domRest.contains(domPredBnn) ) {
                    domRest.removeElement(domPredBnn);
                }
            }
        }

        return domRest;
    }

    static public void dumpDominators(Context context) {
	    Vector blocks = context.getBlocks();
        dom(context,(Block)blocks.elementAt(0),(Block)blocks.elementAt(blocks.size()-1));
        Debugger.trace("Dominators = " + context.D);
    }

    /**
     * Get all the predecessors of b.
     */

    static Vector getPredecessors(Context context, Block b) {
	    Hashtable predecessors = context.predecessors;
        Vector preds = (Vector) predecessors.get(b);
	    if( debug ) {
		    Debugger.trace("Flow.getPredecessors() with b = " + b + ", preds = " + preds);
		}
        if(preds==null) {
            preds = new Vector();
            predecessors.put(b,preds);
        }
        return preds;
    }
}

/*
 * The end.
 */
