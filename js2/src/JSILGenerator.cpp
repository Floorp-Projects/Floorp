#pragma warning ( disable : 4786 )

#include "Nodes.h"
#include "JSILGenerator.h"
#include "../jsc/src/cpp/parser/NodeFactory.h"
#include "ReferenceValue.h"
#include "ConstantEvaluator.h"
#include "Builder.h"
#include "GlobalObjectBuilder.h"

namespace esc {
namespace v1 {

    JavaScript::ICG::ICodeModule* JSILGenerator::emit() {
        return 0;
    }

	// Evaluators

    // Base node
    
	Value* JSILGenerator::evaluate( Context& cx, Node* node ) { 
        throw;
    }

	// Expression evaluators

    Value* JSILGenerator::evaluate( Context& cx, ThisExpressionNode* node ) { 
		throw;
    }
    
	/*
	 * Unqualified identifiers evaluate to a ReferenceValue during semantic analysis,
	 * and so this method is never called.
	 */
	
	Value* JSILGenerator::evaluate( Context& cx, IdentifierNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, QualifiedIdentifierNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, LiteralBooleanNode* node ) { 
        throw;
    }

    Value* JSILGenerator::evaluate( Context& cx, LiteralNumberNode* node ) { 
        throw;
    }

	/*
	 * Literal string
	 */
	
	Value* JSILGenerator::evaluate( Context& cx, LiteralStringNode* node ) { 
        return 0;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, LiteralUndefinedNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, LiteralRegExpNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, UnitExpressionNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, FunctionExpressionNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ParenthesizedExpressionNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ParenthesizedListExpressionNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, LiteralObjectNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, LiteralFieldNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, LiteralArrayNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, PostfixExpressionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, NewExpressionNode* node ) { 
        throw;
    }
    
	/*
	 * Indexed member expressions evaluate to a ReferenceValue during semantic analysis,
	 * and so this method is never called.
	 */

	Value* JSILGenerator::evaluate( Context& cx, IndexedMemberExpressionNode* node ) {
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ClassofExpressionNode* node ) {
        throw;
    }
    
	/*
	 * Member expressions evaluate to a ReferenceValue during semantic analysis,
	 * and so this method is never called.
	 */

	Value* JSILGenerator::evaluate( Context& cx, MemberExpressionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, CoersionExpressionNode* node ) { 
        throw;
    }

	/*
	 * CallExpressionNode
	 *
	 * Call expressions can be generated as invocations of the function
	 * object's call method, or as a direct call to a native function.
	 * If constant evaluation was able to resolve the function reference
	 * to a built-in native function, then call a direct call is generated.
	 *
	 * NOTE: this code is being generated into the start function with
	 *       parameters (Stack scope, ObjectValue this). These are in
	 *       local registers (0 and 1).
	 */

	Value* JSILGenerator::evaluate( Context& cx, CallExpressionNode* node ) { 
		return 0;
    }
    
	/*
	 * GetExpressionNode
	 *
	 * Get expressions are psuedo syntactic constructs, created when
	 * a member expression is used in a context where a value is
	 * expected. In the general case, a get expression is the same as
	 * a call expression with no arguments. In specfic cases, a get
	 * expression can be optimized as a direct access of a native 
	 * field.
	 */

	/* 
	 * What do we need to compile a variable reference to a field id?
	 * the name and the class that defines it. Instance variables would
	 * be instance fields of the Global prototype object. The runtime
	 * version of this object would have the native field that implements
	 * that variable.
	 *
	 * get x ();
	 *
	 * 1 aload_1							// get the target object value
	 * 2 getfield #3 <Field int _values_[]>	// get the property values array
	 * 5 iconst_0							// get the index of value
	 * 6 iaload								// load the value from values
	 */
		
	Value* JSILGenerator::evaluate( Context& cx, GetExpressionNode* node ) { 
		return 0;
    }
    
	/*
	 * SetExpressionNode
	 *
	 * Set expressions are psuedo syntactic constructs, created when
	 * a member expression is used in a context where a storage location
	 * is expected. In the general case, a set expression is the same as
	 * a call expression with one argument (the value to be stored.) In 
	 * specfic cases, a get expression can be optimized as a direct access 
	 * of a native field.
	 *
	 * set x (value);
     *
	 * 1 aload_1							// get the target object value
	 * 2 getfield #3 <Field int values[]>	// get the property values array
	 * 5 iconst_0							// get the index of the value
	 * 6 iconst_5							// get the value
	 * 7 iastore							// store the value in values
	 */

	Value* JSILGenerator::evaluate( Context& cx, SetExpressionNode* node ) { 
		return 0;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, UnaryExpressionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, BinaryExpressionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ConditionalExpressionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, AssignmentExpressionNode* node ) { 
        throw;
    }
    
	/*
	 * Generate the code for a list (e.g. argument list). The owner of this node
	 * has already allocated a fixed size array. This function stuffs the list
	 * values into that array.
	 */
	
	int list_index;
	int list_array_register;

	Value* JSILGenerator::evaluate( Context& cx, ListNode* node ) { 
		return 0;
    }

    // Statements

    Value* JSILGenerator::evaluate( Context& cx, StatementListNode* node ) { 
		return ObjectValue::undefinedValue;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, EmptyStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ExpressionStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, AnnotatedBlockNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, LabeledStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, IfStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, SwitchStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, CaseLabelNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, DoStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, WhileStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ForInStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ForStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, WithStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ContinueStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, BreakStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ReturnStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ThrowStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, TryStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, CatchClauseNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, FinallyClauseNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, UseStatementNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, IncludeStatementNode* node ) { 
        throw;
    }

    // Definitions

    Value* JSILGenerator::evaluate( Context& cx, ImportDefinitionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ImportBindingNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, AnnotatedDefinitionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, AttributeListNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ExportDefinitionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ExportBindingNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, VariableDefinitionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, VariableBindingNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, TypedVariableNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, FunctionDefinitionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, FunctionDeclarationNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, FunctionNameNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, FunctionSignatureNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ParameterNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, OptionalParameterNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ClassDefinitionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, ClassDeclarationNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, InheritanceNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, NamespaceDefinitionNode* node ) { 
        throw;
    }
    
	Value* JSILGenerator::evaluate( Context& cx, PackageDefinitionNode* node ) { 
        throw;
    }

    Value* JSILGenerator::evaluate( Context& cx, ProgramNode* node ) {
        throw;
    }

    /*
     * Test driver
     */

}
}

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
