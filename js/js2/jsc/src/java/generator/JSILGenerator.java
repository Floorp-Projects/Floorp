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

/**
 * JSILGenerator
 */

public class JSILGenerator extends Evaluator implements Tokens {

    public static boolean debug = false;

    private static PrintStream out;
    public static void setOut(String filename) throws Exception {
        out = new PrintStream( new FileOutputStream(filename) );
    }

    private static void emit(String str) {
        if( debug ) {
            Debugger.trace("emit " + str);
        }

        if( out!=null ) {
            out.println(str);
        }
        else {
        }
    }
    
    Value evaluate( Context context, Node node ) throws Exception { 
        String tag = "missing evaluator for " + node.getClass(); emit(context.getIndent()+tag); return null; 
    }

    // Expression evaluators


    Value evaluate( Context context, ThisExpressionNode node ) throws Exception { 
        String tag = "ThisExpression "; 
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, IdentifierNode node ) throws Exception { 
        String tag = "Identifier "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            emit(context.getIndent()+node.name);
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, QualifiedIdentifierNode node ) throws Exception { 
        String tag = "QualifiedIdentifier "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.qualifier != null ) {
            node.qualifier.evaluate(context,this); 
        }
        if( node.name != null ) {
            emit(context.getIndent()+node.name);
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, LiteralBooleanNode node ) throws Exception { 
        String tag = "LiteralBoolean: " + node.value; 
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, LiteralNullNode node ) throws Exception { 
        String tag = "LiteralNull "; 
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, LiteralNumberNode node ) throws Exception { 
        String tag = "LiteralNumber: " + node.value; 
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, LiteralStringNode node ) throws Exception { 
        String tag = "LiteralString: " + node.value; 
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, LiteralUndefinedNode node ) throws Exception {
        String tag = "LiteralUndefined "; 
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, LiteralRegExpNode node ) throws Exception {
        String tag = "LiteralRegExp: " + node.value; 
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, UnitExpressionNode node ) throws Exception {
        String tag = "UnitExpression "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.value != null ) {
            node.value.evaluate(context,this); 
        }
        if( node.type != null ) {
            node.type.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ParenthesizedExpressionNode node ) throws Exception {
        String tag = "ParenthesizedExpression "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ParenthesizedListExpressionNode node ) throws Exception {
        String tag = "ParenthesizedListExpression "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, FunctionExpressionNode node ) throws Exception {
        String tag = "FunctionExpression "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        if( node.signature != null ) {
            node.signature.evaluate(context,this); 
        }
        if( node.body != null ) {
            node.body.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, LiteralObjectNode node ) throws Exception {
        String tag = "LiteralObject "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.fieldlist != null ) {
            node.fieldlist.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, LiteralFieldNode node ) throws Exception {
        String tag = "LiteralField "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        if( node.value != null ) {
            node.value.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, LiteralArrayNode node ) throws Exception {
        String tag = "LiteralArray "; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.elementlist != null ) {
            node.elementlist.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, PostfixExpressionNode node ) throws Exception { 
        String tag = "PostfixExpression: " + Token.getTokenClassName(node.op); 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, NewExpressionNode node ) throws Exception { 
        String tag = "NewExpression"; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, IndexedMemberExpressionNode node ) throws Exception { 
        String tag = "IndexedMemberExpression"; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.base != null ) {
            node.base.evaluate(context,this); 
        }
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, MemberExpressionNode node ) throws Exception { 
        String tag = "MemberExpression"; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.base != null ) {
            node.base.evaluate(context,this); 
        }
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ClassofExpressionNode node ) throws Exception { 
        String tag = "ClassofExpression"; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.base != null ) {
            node.base.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, CoersionExpressionNode node ) throws Exception { 
        String tag = "CoersionExpression"; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        if( node.type != null ) {
            node.type.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, CallExpressionNode node ) throws Exception { 
        String tag = "CallExpression"; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.member != null ) {
            node.member.evaluate(context,this); 
        }
        if( node.args != null ) {
            node.args.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, UnaryExpressionNode node ) throws Exception { 
        String tag = "UnaryExpression: " + Token.getTokenClassName(node.op); 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, BinaryExpressionNode node ) throws Exception { 
        String tag = "BinaryExpression: " + Token.getTokenClassName(node.op); 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.lhs != null ) {
            node.lhs.evaluate(context,this); 
        }
        if( node.rhs != null ) {
            node.rhs.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ConditionalExpressionNode node ) throws Exception { 
        String tag = "ConditionalExpression"; 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.condition != null ) {
            node.condition.evaluate(context,this); 
        }
        if( node.thenexpr != null ) {
            node.thenexpr.evaluate(context,this); 
        }
        if( node.elseexpr != null ) {
            node.elseexpr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, AssignmentExpressionNode node ) throws Exception { 
        String tag = "AssignmentExpression: " + ((node.op==empty_token)?"":Token.getTokenClassName(node.op)); 
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.lhs != null ) {
            node.lhs.evaluate(context,this); 
        }
        if( node.rhs != null ) {
            node.rhs.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ListNode node ) throws Exception { 
        String tag = "List"; 
        //emit(context.getIndent()+tag);
        if( node.list != null ) {
            node.list.evaluate(context,this); 
        }
        if( node.item != null ) {
            node.item.evaluate(context,this); 
        }
        return null;
    }

    // Statements

    Value evaluate( Context context, StatementListNode node ) throws Exception { 
        String tag = "StatementList"; 
        //emit(context.getIndent()+tag);
        if( node.list != null ) {
            node.list.evaluate(context,this); 
        }
        if( node.item != null ) {
            node.item.evaluate(context,this); 
        }
        return null;
    }

    Value evaluate( Context context, EmptyStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"EmptyStatement";
        emit(context.getIndent()+tag);
        return null;
    }

    Value evaluate( Context context, ExpressionStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"ExpressionStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, AnnotatedBlockNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"AnnotatedBlock";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.attributes != null ) {
            node.attributes.evaluate(context,this); 
        }
        if( node.statements != null ) {
            node.statements.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, LabeledStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"LabeledStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.label != null ) {
            node.label.evaluate(context,this); 
        }
        if( node.statement != null ) {
            node.statement.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, IfStatementNode node ) throws Exception { 
        
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"IfStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.condition != null ) {
            node.condition.evaluate(context,this); 
        }
        if( node.thenactions != null ) {
            node.thenactions.evaluate(context,this); 
        }
        if( node.elseactions != null ) {
            node.elseactions.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, SwitchStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"SwitchStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        if( node.statements != null ) {
            node.statements.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, CaseLabelNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"CaseLabel";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.label != null ) {
            node.label.evaluate(context,this); 
        } else {
            emit(context.getIndent()+"default");
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, DoStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"DoStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.statements != null ) {
            node.statements.evaluate(context,this); 
        }
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, WhileStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"WhileStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        if( node.statement != null ) {
            node.statement.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ForStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"ForStatement";
        emit(context.getIndent()+tag);
        context.indent++;
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
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ForInStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"ForInStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.property != null ) {
            node.property.evaluate(context,this); 
        }
        if( node.object != null ) {
            node.object.evaluate(context,this); 
        }
        if( node.statement != null ) {
            node.statement.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, WithStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"WithStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        if( node.statement != null ) {
            node.statement.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ContinueStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"ContinueStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, BreakStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"BreakStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ReturnStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"ReturnStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ThrowStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"ThrowStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, TryStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"TryStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.tryblock != null ) {
            node.tryblock.evaluate(context,this); 
        }
        if( node.catchlist != null ) {
            node.catchlist.evaluate(context,this); 
        }
        if( node.finallyblock != null ) {
            node.finallyblock.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, CatchClauseNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"CatchClause";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.parameter != null ) {
            node.parameter.evaluate(context,this); 
        }
        if( node.statements != null ) {
            node.statements.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, FinallyClauseNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"FinallyClause";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.statements != null ) {
            node.statements.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, UseStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"UseStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.expr != null ) {
            node.expr.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, IncludeStatementNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"IncludeStatement";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.item != null ) {
            node.item.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    // Definitions

    Value evaluate( Context context, ImportDefinitionNode node ) throws Exception { 
        String tag = "ImportDefinition";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.item != null ) {
            node.item.evaluate(context,this); 
        }
        if( node.list != null ) {
            node.list.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ImportBindingNode node ) throws Exception { 
        String tag = "ImportBinding";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.identifier != null ) {
            node.identifier.evaluate(context,this); 
        }
        if( node.item != null ) {
            node.item.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, AnnotatedDefinitionNode node ) throws Exception { 
        String tag = (node.isLeader ? "*"+node.block+":" : ""+node.block+": ")+"AnnotatedDefinition";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.attributes != null ) {
            node.attributes.evaluate(context,this); 
        }
        if( node.definition != null ) {
            node.definition.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, AttributeListNode node ) throws Exception { 
        String tag = "AttributeList";
        //emit(context.getIndent()+tag);
        //context.indent++;
        if( node.item != null ) {
            node.item.evaluate(context,this); 
        }
        if( node.list != null ) {
            node.list.evaluate(context,this); 
        }
        //context.indent--;
        return null;
    }

    Value evaluate( Context context, ExportDefinitionNode node ) throws Exception { 
        String tag = "ExportDefinition";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.list != null ) {
            node.list.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ExportBindingNode node ) throws Exception { 
        String tag = "ExportBinding";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        if( node.value != null ) {
            node.value.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, VariableDefinitionNode node ) throws Exception { 
        String tag = "VariableDefinition: " + Token.getTokenClassName(node.kind);
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.list != null ) {
            node.list.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, VariableBindingNode node ) throws Exception { 
        String tag = "VariableBinding";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.variable != null ) {
            node.variable.evaluate(context,this); 
        }
        if( node.initializer != null ) {
            node.initializer.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, TypedVariableNode node ) throws Exception { 
        String tag = "TypedVariable";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.identifier != null ) {
            node.identifier.evaluate(context,this); 
        }
        if( node.type != null ) {
            node.type.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, FunctionDefinitionNode node ) throws Exception { 
        String tag = "FunctionDefinition";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.decl != null ) {
            node.decl.evaluate(context,this); 
        }
        if( node.body != null ) {
            node.body.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, FunctionDeclarationNode node ) throws Exception { 
        String tag = "FunctionDeclaration";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        if( node.signature != null ) {
            node.signature.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, FunctionSignatureNode node ) throws Exception { 
        String tag = "FunctionSignature";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.parameter != null ) {
            node.parameter.evaluate(context,this); 
        }
        if( node.result != null ) {
            node.result.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, FunctionNameNode node ) throws Exception { 
        String tag = "FunctionName: " + ((node.kind==empty_token)?"":Token.getTokenClassName(node.kind));
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ParameterNode node ) throws Exception { 
        String tag = "Parameter";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.identifier != null ) {
            node.identifier.evaluate(context,this); 
        }
        if( node.type != null ) {
            node.type.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, OptionalParameterNode node ) throws Exception { 
        String tag = "OptionalParameter";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.parameter != null ) {
            node.parameter.evaluate(context,this); 
        }
        if( node.initializer != null ) {
            node.initializer.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ClassDefinitionNode node ) throws Exception { 
        String tag = "ClassDefinition";
        emit(context.getIndent()+tag);
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

    Value evaluate( Context context, InheritanceNode node ) throws Exception { 
        String tag = "Inheritance";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.baseclass != null ) {
            node.baseclass.evaluate(context,this); 
        }
        if( node.interfaces != null ) {
            node.interfaces.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ClassDeclarationNode node ) throws Exception { 
        String tag = "ClassDeclaration";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, NamespaceDefinitionNode node ) throws Exception { 
        String tag = "NamespaceDefinition";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        if( node.extendslist != null ) {
            node.extendslist.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, PackageDefinitionNode node ) throws Exception { 
        String tag = "PackageDefinition";
        emit(context.getIndent()+tag);
        context.indent++;
        if( node.name != null ) {
            node.name.evaluate(context,this); 
        }
        if( node.statements != null ) {
            node.statements.evaluate(context,this); 
        }
        context.indent--;
        return null;
    }

    Value evaluate( Context context, ProgramNode node ) throws Exception { 
        String tag = "Program"; 
        emit(context.getIndent()+tag);
        context.indent++;
        node.statements.evaluate(context,this); 
        context.indent--;
        return null;
    }
}

/*
 * The end.
 */
