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
import java.io.*;
import java.util.*;

/*
 * Generates code for the JSVM.
 */

public class JSILGenerator extends Evaluator implements Tokens {

    public static boolean debug = false;

    private static PrintStream out;
    public static void setOut(String filename) throws Exception {
        out = new PrintStream( new FileOutputStream(filename) );
    }

    private boolean doASM;
    public JSILGenerator(boolean doASM) {
	    this.doASM = doASM;
	}

    private static void emit(String str) {
        if( debug ) {
            Debugger.trace("emitln " + str);
        }

        if( out!=null ) {
            out.print(str);
        }
        else {
        }
    }
    
    private static void emitln(String str) {
        if( out!=null ) {
            out.println("");
			emit(str);
        }
        else {
        }
    }
    
    private static String opName(int id) {
        String name = "<TBD>";
        switch(id) {
        case plus_token:
            name = "Add";
            break;
        case minus_token:
            name = "Subtract";
            break;
        case mult_token:
            name = "Multiply";
            break;
        case div_token:
            name = "Divide";
            break;
        case modulus_token:
            name = "Remainder";
            break;
        case leftshift_token:
            name = "LeftShift";
            break;
        case rightshift_token:
            name = "RightShift";
            break;
        case unsignedrightshift_token:
            name = "LogicalRightShift";
            break;
        case bitwiseand_token:
            name = "BitwiseAnd";
            break;
        case bitwiseor_token:
            name = "BitwiseOr";
            break;
        case bitwisexor_token:
            name = "BitwiseXor";
            break;
        case logicaland_token:
            name = "LogicalAnd";
            break;
        case logicalor_token:
            name = "LogicalOr";
            break;
        case logicalxor_token:
            name = "LogicalXor";
            break;
        case lessthan_token:
            name = "Less";
            break;
        case lessthanorequals_token:
            name = "LessOrEqual";
            break;
        case equals_token:
            name = "Equal";
            break;
        case strictequals_token:
            name = "Identical";
            break;
        default:
            break;
        }
        return name;
    }
    
    
    Value evaluate( Context context, Node node ) throws Exception { 
        String tag = "missing evaluator for " + node.getClass(); emitln(context.getIndent()+tag); return null; 
    }

    // Expression evaluators

    /*
     * This reference
     */

    Value evaluate( Context context, ThisExpressionNode node ) throws Exception {
		emitln("this");
        return null;
    }

    /*
     * Unqualified identifier
     *
     * STATUS
     * Jan-03-2001: Coded
     *
     * NOTES
     * Generates a dynamic name lookup of an unqualified identifier.
     * Identifier nodes that can be statically bound, are converted
     * to reference values during constant evaluation, and gen'd as
     * static property accesses.
     */

    Value evaluate( Context context, IdentifierNode node ) throws Exception { 
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"LOAD_NAME "+rname+",'"+node.name+"'");
        return null;
    }

    /*
     * Qualified identifier
     *
     * STATUS
     * Jan-09-2001: Coded
     *
     * NOTES
     * Generates a dynamic name lookup of a qualified identifier.
     * Identifier nodes that can be statically bound, are converted
     * to reference values during constant evaluation, and gen'd as
     * static property accesses.
     */

    Value evaluate( Context context, QualifiedIdentifierNode node ) throws Exception { 
		if( node.qualifier != null ) {
			node.qualifier.evaluate(context,this); 
		}
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		String qname = Machine.nameRegister(node.qualifier.store);
		emitln(context.getIndent()+"LOAD_QNAME "+rname+","+qname+",'"+node.name+"'");
        return null;
    }

    /*
     * Boolean literal
     *
     * STATUS
     * Jan-03-2001: Coded
     *
     * NOTES
     * Generates an immediate load of a boolean value.
     */

    Value evaluate( Context context, LiteralBooleanNode node ) throws Exception { 
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"LOAD_BOOLEAN "+rname+",'"+node.value+"'");
        return null;
    }

    /*
     * Null literal
     *
     * STATUS
     * Jan-03-2001: Coded
     *
     * NOTES
     * Generates an immediate load of null.
     */

    Value evaluate( Context context, LiteralNullNode node ) throws Exception { 
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"LOAD_NULL "+rname);
        return null;
    }

    /*
     * Number literal
     *
     * STATUS
     * Jan-03-2001: Coded
     *
     * NOTES
     * Generates an immediate load of a number.
     */

    Value evaluate( Context context, LiteralNumberNode node ) throws Exception {
        Debugger.trace("evaluate(LiteralNumber) with node = "+node); 
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"LOAD_IMMEDIATE "+rname+","+node.value);
        Debugger.trace("evaluate(LiteralNumber) with node = "+node+", store = " + node.store); 
        return null;
    }

    /*
     * String literal
     *
     * STATUS
     * Jan-03-2001: Coded
     *
     * NOTES
     * Generates an immediate load of a string value.
     */

    Value evaluate( Context context, LiteralStringNode node ) throws Exception { 
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"LOAD_STRING "+rname+",'"+node.value+"'");
        return null;
    }

    /*
     * Regular expression literal
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, LiteralRegExpNode node ) throws Exception {
	    if( doASM ) {
        } else {
			String tag = "LiteralRegExp: " + node.value; 
			emitln(context.getIndent()+tag);
		}
        return null;
    }

    /*
     * Unit expression
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, UnitExpressionNode node ) throws Exception {
		if( node.value != null ) {
			node.value.evaluate(context,this); 
		}
		if( node.type != null ) {
			node.type.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Parenthesized expression
     *
     * STATUS
     * Jan-09-2001: Coded
     *
     * NOTES
     * Simply evaluates the constituent part.
     */

    Value evaluate( Context context, ParenthesizedExpressionNode node ) throws Exception {
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Parenthesized list expression
     *
     * STATUS
     * Jan-09-2001: Coded
     *
     * NOTES
     * Simply evaluates the constituent part.
     */

    Value evaluate( Context context, ParenthesizedListExpressionNode node ) throws Exception {
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Function literal
     *
     * STATUS
     * Not implemented.
     *
     * NOTES
     * Generates the code for an anonymous function nested in 
     * the lexical scope of its definition.
     */

    Value evaluate( Context context, FunctionExpressionNode node ) throws Exception {
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"NEW_CLOSURE "+rname+",ICodeModule");

		/*
        if( node.name != null ) {
			node.name.evaluate(context,this); 
		}
		if( node.signature != null ) {
			node.signature.evaluate(context,this); 
		}
		if( node.body != null ) {
			node.body.evaluate(context,this); 
		}
        */
        return null;
    }

    /*
     * Object literal
     *
     * STATUS
     * Jan-03-2001: Coded
     *
     * NOTES
     * Generates code for object creation and initialization
     * to make an object value with the form of the literal.
     */

    Value evaluate( Context context, LiteralObjectNode node ) throws Exception {
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"NEW_OBJECT "+rname+",'Object'");

		// for each property in value, init a property in the new object.

		ObjectValue value = (ObjectValue) node.value;
        Enumeration k = value.slots.keys();
		Enumeration e = value.slots.elements();
		while(k.hasMoreElements()) {
			String tname = Machine.nameRegister(new Store(null,Machine.allocateTempRegister()));
			Slot s = (Slot) e.nextElement();
			String p = (String) k.nextElement();
			if(s.value.getType(context)==NumberType.type) {
    			emitln(context.getIndent()+"LOAD_IMMEDIATE "+tname+","+s.value);
			}
			else
			if(s.value.getType(context)==StringType.type) {
    			emitln(context.getIndent()+"LOAD_STRING "+tname+",'"+s.value+"'");
			}
			emitln(context.getIndent()+"SET_PROP "+rname+",'"+p+"',"+tname);
		}
        return null;
    }

    /*
     * Array literal
     *
     * STATUS
     * Jan-09-2001: Coded
     *
     * NOTES
     * Generates code for object creation and initialization
     * to make an object value with the form of the literal.
     * [Only handles number and string values.]
     */

    Value evaluate( Context context, LiteralArrayNode node ) throws Exception {
		node.store = new Store(null,Machine.allocateTempRegister());
		String rname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"NEW_ARRAY "+rname);

		// for each property in value, init a property in the new object.

        if(node.value == null) {
            return null;
        }

        int p = 0;
        ListValue value = (ListValue) node.value;
		Enumeration e = value.elements();
		while(e.hasMoreElements()) {
			String tname = Machine.nameRegister(new Store(null,Machine.allocateTempRegister()));
			Value v = (Value) e.nextElement();
			if(v.getType(context)==NumberType.type) {
    			emitln(context.getIndent()+"LOAD_IMMEDIATE "+tname+","+v);
			}
			else
			if(v.getType(context)==StringType.type) {
    			emitln(context.getIndent()+"LOAD_STRING "+tname+",'"+v+"'");
			}
			emitln(context.getIndent()+"SET_PROP "+rname+",'"+p+++"',"+tname);
		}
        return null;
    }

    /*
     * Postfix expression
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, PostfixExpressionNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * New expression
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, NewExpressionNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Indexed member expression
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, IndexedMemberExpressionNode node ) throws Exception { 
		if( node.base != null ) {
			node.base.evaluate(context,this); 
		}
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Property access 
     *
     * STATUS
     * Jan-05-2001: Coded
     *
     * NOTES
     * Generates code for getting a property of a value.
     */

    Value evaluate( Context context, MemberExpressionNode node ) throws Exception { 
		if( node.base != null ) {
			node.base.evaluate(context,this); 
		}
        String dname,rname;
		node.store = new Store(null,Machine.allocateTempRegister());

		rname = Machine.nameRegister(node.base.store);
        dname = Machine.nameRegister(node.store);
        ReferenceValue ref = (ReferenceValue) node.ref;
		emitln(context.getIndent()+"GET_PROP "+
			           dname+","+rname+",'"+ref.getName()+"'");
        return null;
    }

    /*
     * Classof 
     *
     * STATUS
     * Jan-05-2001: Coded
     *
     * NOTES
     * Generates code for getting the class object of a value.
     */

    Value evaluate( Context context, ClassofExpressionNode node ) throws Exception { 

		if( node.base != null ) {
			node.base.evaluate(context,this); 
		}

        String dname,lname,rname;
		node.store = new Store(null,Machine.allocateTempRegister());
		rname = Machine.nameRegister(node.base.store);
        dname = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"GET_CLASS "+
			           dname+","+rname);
        return null;
    }

    /*
     * Coersion 
     *
     * STATUS
     * Jan-05-2001: Coded
     *
     * NOTES
     * Generates code for coercing a value to another value of a specific type.
     */

    Value evaluate( Context context, CoersionExpressionNode node ) throws Exception { 

        Debugger.trace("CoersionExpressionNode with type = " + node.type);
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}

		if( node.type != null ) {
			node.type.evaluate(context,this); 
		}

        String dreg,ereg,treg;
		node.store = new Store(null,Machine.allocateTempRegister());
		ereg = Machine.nameRegister(node.expr.store);
		treg = Machine.nameRegister(node.type.store);
        dreg = Machine.nameRegister(node.store);
		emitln(context.getIndent()+"CAST "+
			           dreg+","+ereg+","+treg);
        Debugger.trace("JSILGenerator.evaluate(CoersionExpressionNode) with node.store = " + node.store);
        return null;
    }

    /*
     * Call expression
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, CallExpressionNode node ) throws Exception { 
		if( node.member != null ) {
			node.member.evaluate(context,this); 
		}
        String args = "";
        Debugger.trace("evaluate(CallExpressionNode) with node.args = " + node.args);
		if( node.args != null ) {
			node.args.evaluate(context,this);
            ListNode list = (ListNode)node.args;
            while(list!=null) {
                args = Machine.nameRegister(list.item.store)+((args=="")?"":",")+args;
                list = (ListNode)list.list;
            }  
		}
		node.store = new Store(null,Machine.allocateTempRegister());
		String reg1 = Machine.nameRegister(node.store);
		String reg2 = Machine.nameRegister(node.member.store);
		emitln(context.getIndent()+"CALL "+
			           reg1+","+reg2+",("+args+")");
        return null;
    }

    /*
     * Unary expression
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, UnaryExpressionNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Binary expression
     *
     * STATUS
     * Partially implemented
     */

    Value evaluate( Context context, BinaryExpressionNode node ) throws Exception { 
		Value rhs,lhs;

		
        // compute the lhs value using either
        // 1 lhs expression
        // 2 unbound reference
        // 3 bound reference

        String dname,lname,rname;

        if( node.lhs_slot != null ) {
            lname = Machine.nameRegister(node.lhs_slot.store);
        } else if ( node.lhs_ref != null ) {
            // get the value of the reference and map lname to its location.
            lname = "TBD";
        } else {
            node.lhs.evaluate(context,this); 
            lname = Machine.nameRegister(node.lhs.store);
        } 

        if( node.rhs_slot != null ) {
            rname = Machine.nameRegister(node.rhs_slot.store);
        } else if ( node.rhs_ref != null ) {
            // get the value of the reference and map rname to its location.
            rname = "TBD";
        } else {
            node.rhs.evaluate(context,this); 
            rname = Machine.nameRegister(node.rhs.store);
        } 

		node.store = new Store(null,Machine.allocateTempRegister());
        dname = Machine.nameRegister(node.store);

		emitln(context.getIndent()+"GENERIC_BINARY_OP "+
			           dname+","+opName(node.op)+
			           ","+lname + "," + rname);

        return null;
    }

    /*
     * Conditional expression
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ConditionalExpressionNode node ) throws Exception { 
		if( node.condition != null ) {
			node.condition.evaluate(context,this); 
		}
		if( node.thenexpr != null ) {
			node.thenexpr.evaluate(context,this); 
		}
		if( node.elseexpr != null ) {
			node.elseexpr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Assignment expression
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, AssignmentExpressionNode node ) throws Exception { 
        // during constant eval,
        // save reference to lhs in node. (key!)

        // 1 Load the rhs value into temporary storage
        // 2 Load the lhs value into temporary storage
        // 3 Operate (move,generic_binary_op)
        // 4 Store the result in the place indicated by the lhs reference.

		if( node.rhs != null ) {
			node.rhs.evaluate(context,this); 
		}
    	if( node.lhs != null ) {
			node.lhs.evaluate(context,this); 
		}

        String dname,lname,rname;
		node.store = new Store(null,Machine.allocateTempRegister());
        lname = Machine.nameRegister(node.lhs.store);
        rname = Machine.nameRegister(node.rhs.store);
        dname = Machine.nameRegister(node.store);
        if( node.op == assign_token ) {
		    emitln(context.getIndent()+"MOVE "+dname+","+rname);
        } else {
		    emitln(context.getIndent()+"GENERIC_BINARY_OP "+dname+",OP,"+lname+","+rname);
		}

        ReferenceValue ref = (ReferenceValue) node.ref;
        if( ref.scope == null ) {
            emitln(context.getIndent()+"SAVE_NAME '"+
			           ref.getName()+"',"+dname);
        } else {
       		String bname = Machine.nameRegister(((MemberExpressionNode)node.lhs).base.store);
            emitln(context.getIndent()+"SET_PROP "+bname+",'"+
			           ref.getName()+"',"+dname);
        }
        return null;
    }

    /*
     * List
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, ListNode node ) throws Exception { 
		if( node.list != null ) {
			node.list.evaluate(context,this); 
		}
		if( node.item != null ) {
			node.item.evaluate(context,this); 
		}
        return null;
    }

    // Statements

    /*
     * Statement list
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, StatementListNode node ) throws Exception { 
		if( node.list != null ) {
			node.list.evaluate(context,this); 
		}
		if( node.item != null ) {
			node.item.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Empty statement
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, EmptyStatementNode node ) throws Exception { 
        return null;
    }

    /*
     * Expression statement
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, ExpressionStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Annotated block
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, AnnotatedBlockNode node ) throws Exception { 
		if( node.attributes != null ) {
			node.attributes.evaluate(context,this); 
		}
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Labeled statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, LabeledStatementNode node ) throws Exception { 
		if( node.label != null ) {
			node.label.evaluate(context,this); 
		}
		if( node.statement != null ) {
			node.statement.evaluate(context,this); 
		}
        return null;
    }

    /*
     * If statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, IfStatementNode node ) throws Exception { 
		if( node.condition != null ) {
			node.condition.evaluate(context,this); 
		}
		if( node.thenactions != null ) {
			node.thenactions.evaluate(context,this); 
		}
		if( node.elseactions != null ) {
			node.elseactions.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Switch statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, SwitchStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Case label
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, CaseLabelNode node ) throws Exception { 
		if( node.label != null ) {
			node.label.evaluate(context,this); 
		} else {
			emitln(context.getIndent()+"default");
		}
        return null;
    }

    /*
     * Do statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, DoStatementNode node ) throws Exception { 
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * While statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, WhileStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
		if( node.statement != null ) {
			node.statement.evaluate(context,this); 
		}
        return null;
    }

    /*
     * For statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ForStatementNode node ) throws Exception { 
		if( node.initialize != null ) {
			node.initialize.evaluate(context,this); 
		}
		if( node.test != null ) {
			node.test.evaluate(context,this); 
		}
		if( node.increment != null ) {
			node.increment.evaluate(context,this); 
		}
		if( node.statement != null ) {
			node.statement.evaluate(context,this); 
		}
        return null;
    }

    /*
     * For in statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ForInStatementNode node ) throws Exception { 
		if( node.property != null ) {
			node.property.evaluate(context,this); 
		}
		if( node.object != null ) {
			node.object.evaluate(context,this); 
		}
		if( node.statement != null ) {
			node.statement.evaluate(context,this); 
		}
        return null;
    }

    /*
     * With statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, WithStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
		if( node.statement != null ) {
			node.statement.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Continue statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ContinueStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Break statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, BreakStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Return statement
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, ReturnStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
		String rname = Machine.nameRegister(node.expr.store);
		emitln(context.getIndent()+"RETURN "+rname);
        return null;
    }

    /*
     * Throw statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ThrowStatementNode node ) throws Exception { 
		String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"ThrowStatement";
		emitln(context.getIndent()+tag);
		context.indent++;
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Try statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, TryStatementNode node ) throws Exception { 
		if( node.tryblock != null ) {
			node.tryblock.evaluate(context,this); 
		}
		if( node.catchlist != null ) {
			node.catchlist.evaluate(context,this); 
		}
		if( node.finallyblock != null ) {
			node.finallyblock.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Catch clause
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, CatchClauseNode node ) throws Exception { 
		if( node.parameter != null ) {
			node.parameter.evaluate(context,this); 
		}
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Finally clause
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, FinallyClauseNode node ) throws Exception { 
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Use statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, UseStatementNode node ) throws Exception { 
		if( node.expr != null ) {
			node.expr.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Include statement
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, IncludeStatementNode node ) throws Exception { 
		if( node.item != null ) {
			node.item.evaluate(context,this); 
		}
        return null;
    }

    // Definitions

    /*
     * Import definition
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ImportDefinitionNode node ) throws Exception { 
		if( node.item != null ) {
			node.item.evaluate(context,this); 
		}
		if( node.list != null ) {
			node.list.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Import binding
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ImportBindingNode node ) throws Exception { 
		if( node.identifier != null ) {
			node.identifier.evaluate(context,this); 
		}
		if( node.item != null ) {
			node.item.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Annotated definition
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, AnnotatedDefinitionNode node ) throws Exception { 
		if( node.attributes != null ) {
			node.attributes.evaluate(context,this); 
		}
		if( node.definition != null ) {
			node.definition.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Attribute list
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, AttributeListNode node ) throws Exception { 
		if( node.item != null ) {
			node.item.evaluate(context,this); 
		}
		if( node.list != null ) {
			node.list.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Export definition
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ExportDefinitionNode node ) throws Exception { 
		if( node.list != null ) {
			node.list.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Export binding
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, ExportBindingNode node ) throws Exception { 
		if( node.name != null ) {
			node.name.evaluate(context,this); 
		}
		if( node.value != null ) {
			node.value.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Variable definition
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, VariableDefinitionNode node ) throws Exception { 
		if( node.list != null ) {
			node.list.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Variable binding
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, VariableBindingNode node ) throws Exception { 
		if( node.variable != null ) {
			node.variable.evaluate(context,this); 
		}
		if( node.initializer != null ) {
			node.initializer.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Typed variable
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, TypedVariableNode node ) throws Exception { 
		if( node.identifier != null ) {
			node.identifier.evaluate(context,this); 
		}
		if( node.type != null ) {
			node.type.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Function definition
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, FunctionDefinitionNode node ) throws Exception { 
		Machine.init(node.fixedCount);
		emitln(context.getIndent()+"<method ");
		if( node.decl != null ) {
			node.decl.evaluate(context,this); 
		}
        context.indent++;
		if( node.body != null ) {
			node.body.evaluate(context,this); 
		}
        context.indent--;
		emitln(context.getIndent()+"</method>");
        return null;
    }

    /*
     * Function declaration
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, FunctionDeclarationNode node ) throws Exception { 
        ReferenceValue ref = (ReferenceValue) node.ref;
		emit("name="+ref.getName());
		//if( node.name != null ) {
		//	node.name.evaluate(context,this); 
		//}
		if( node.signature != null ) {
			node.signature.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Function signature
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, FunctionSignatureNode node ) throws Exception { 
		emit(" type=");
		if( node.result != null ) {
            ReferenceValue ref = (ReferenceValue) node.ref;
		    emit(ref.getName());
		} else {
			emit("Object");
		}
		emit("/>");
		    
Debugger.trace("evaluate(FunctionSignatureNode) with node.parameter = " + node.parameter);

		if( node.parameter != null ) {
			node.parameter.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Function name
     *
     * STATUS
     * Not used
     */

    Value evaluate( Context context, FunctionNameNode node ) throws Exception { 
        return null;
    }

    /*
     * Parameter
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, ParameterNode node ) throws Exception { 
		emitln(context.getIndent()+"<parameter name="+node.name);

/*
		if( node.identifier != null ) {
			node.identifier.evaluate(context,this); 
		}
		emit(" type=");
		if( node.type != null ) {
			node.type.evaluate(context,this); 
		}
*/
		emit("/>");
        return null;
    }

    /*
     * Optional parameter
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, OptionalParameterNode node ) throws Exception { 
		if( node.parameter != null ) {
			node.parameter.evaluate(context,this); 
		}
		if( node.initializer != null ) {
			node.initializer.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Class definition
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, ClassDefinitionNode node ) throws Exception { 
		emitln("<class name=" + node.name + "\">");
		context.indent++;
		if( node.name != null ) {
			node.name.evaluate(context,this); 
		}
		if( node.inheritance != null ) {
			node.inheritance.evaluate(context,this); 
		}
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
		context.indent--;
        return null;
    }

    /*
     * Inheritance definition
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, InheritanceNode node ) throws Exception { 
		if( node.baseclass != null ) {
			node.baseclass.evaluate(context,this); 
		}
		if( node.interfaces != null ) {
			node.interfaces.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Class declaration
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, ClassDeclarationNode node ) throws Exception { 
		if( node.name != null ) {
			node.name.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Namespace definition
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, NamespaceDefinitionNode node ) throws Exception { 
		if( node.name != null ) {
			node.name.evaluate(context,this); 
		}
		if( node.extendslist != null ) {
			node.extendslist.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Package definition
     *
     * STATUS
     * Not implemented
     */

    Value evaluate( Context context, PackageDefinitionNode node ) throws Exception { 
		if( node.name != null ) {
			node.name.evaluate(context,this); 
		}
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
        return null;
    }

    /*
     * Program
     *
     * STATUS
     * Jan-24-2001: Coded
     */

    Value evaluate( Context context, ProgramNode node ) throws Exception { 
		if( node.statements != null ) {
			node.statements.evaluate(context,this); 
		}
        return null;
    }
}

/*
 * Static model of the virtual machine.
 */

class Machine {

    private static boolean debug = true;

	static int registerCount;
	static int fixedRegisterCount;

    static void init(int fixedCount) {
	    Debugger.trace("Machine.init() with fixedCount = " + fixedCount);
	    registerCount = fixedRegisterCount = fixedCount;
	}

	static int allocateTempRegister() {
	    return registerCount++;
	}

    static void resetTempRegisters() {
	}

    static void resetAllRegisters() {
	}

	static String nameRegister(Store store) {
	    return "R"+ (store.index+1);
	}

	static String nameTemp() {
	    return "R"+ (registerCount+1);
	}
}

/*
 * The end.
 */
