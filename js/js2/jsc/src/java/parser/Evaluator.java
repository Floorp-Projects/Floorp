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

/**
 * The base visitor object extended by semantic evaluators.
 *
 * This is a visitor that is used by the compiler for various forms for
 * evaluation of a parse tree (e.g. a type evaluator might compute the 
 * static type of an expression.)
 */

public class Evaluator {

    Value evaluate( Context context, Node node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node ); 
    }

    // Expression evaluators


    Value evaluate( Context context, ThisExpressionNode node ) throws Exception { 
      throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, IdentifierNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, QualifiedIdentifierNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralBooleanNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralNullNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralNumberNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralStringNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralUndefinedNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralRegExpNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, UnitExpressionNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, FunctionExpressionNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ParenthesizedExpressionNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ParenthesizedListExpressionNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralObjectNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralFieldNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LiteralArrayNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, PostfixExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, NewExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, IndexedMemberExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ClassofExpressionNode node ) throws Exception {
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, MemberExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, CoersionExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, CallExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, UnaryExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, BinaryExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ConditionalExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, AssignmentExpressionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ListNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }

    // Statements

    Value evaluate( Context context, StatementListNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, EmptyStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ExpressionStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, AnnotatedBlockNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, LabeledStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, IfStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, SwitchStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, CaseLabelNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, DoStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, WhileStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ForInStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ForStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, WithStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ContinueStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, BreakStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ReturnStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ThrowStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, TryStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, CatchClauseNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, FinallyClauseNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, UseStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, IncludeStatementNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }

    // Definitions

    Value evaluate( Context context, ImportDefinitionNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, ImportBindingNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, AnnotatedDefinitionNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, AttributeListNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, ExportDefinitionNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, ExportBindingNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, VariableDefinitionNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, VariableBindingNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, TypedVariableNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, FunctionDefinitionNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, FunctionDeclarationNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, FunctionNameNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, FunctionSignatureNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, ParameterNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, OptionalParameterNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, ClassDefinitionNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, ClassDeclarationNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, InheritanceNode node ) throws Exception { 
        throw new Exception( "synthesis unspecified for node = " + node );
    }
    Value evaluate( Context context, NamespaceDefinitionNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, PackageDefinitionNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
    Value evaluate( Context context, ProgramNode node ) throws Exception { 
        throw new Exception( "evaluation unspecified for node = " + node );
    }
}

/*
 * The end.
 */
