/*
 * JSILGenerator
 */

#ifndef JSILGenerator_h
#define JSILGenerator_h

#include <vector>

#include "Value.h"
#include "Context.h"
#include "Evaluator.h"
#include "icodegenerator.h"
//#include "ByteCodeFactory.h"
//#include "ClassFileConstants.h"

namespace esc {
namespace v1 {

    class JavaScript::ICG::ICodeModule;

using namespace esc::v1;

class JSILGenerator : public Evaluator /*, private ByteCodeFactory*/ {

public:

	/*
	 * Test driver
	 */

    static int main(int argc, char* argv[]);

	/* Create a JSILGenerator object for each ICode module.
	 */

	JSILGenerator(std::string scriptname) {
	}

	~JSILGenerator() {
	}

    JavaScript::ICG::ICodeModule* emit();

	// Base node
    
	Value* evaluate( Context& cx, Node* node );

	// 3rd Edition features
	
	Value* evaluate( Context& cx, IdentifierNode* node );
    Value* evaluate( Context& cx, ThisExpressionNode* node );
    Value* evaluate( Context& cx, LiteralBooleanNode* node );
    Value* evaluate( Context& cx, LiteralNumberNode* node );
    Value* evaluate( Context& cx, LiteralStringNode* node );
    Value* evaluate( Context& cx, LiteralUndefinedNode* node );
    Value* evaluate( Context& cx, LiteralRegExpNode* node );
    Value* evaluate( Context& cx, FunctionExpressionNode* node );
    Value* evaluate( Context& cx, ParenthesizedExpressionNode* node );
    Value* evaluate( Context& cx, ParenthesizedListExpressionNode* node );
    Value* evaluate( Context& cx, LiteralObjectNode* node );
    Value* evaluate( Context& cx, LiteralFieldNode* node );
    Value* evaluate( Context& cx, LiteralArrayNode* node );
    Value* evaluate( Context& cx, PostfixExpressionNode* node );
    Value* evaluate( Context& cx, NewExpressionNode* node );
    Value* evaluate( Context& cx, IndexedMemberExpressionNode* node );
    Value* evaluate( Context& cx, MemberExpressionNode* node );
    Value* evaluate( Context& cx, CallExpressionNode* node );
    Value* evaluate( Context& cx, GetExpressionNode* node );
    Value* evaluate( Context& cx, SetExpressionNode* node );
    Value* evaluate( Context& cx, UnaryExpressionNode* node );
    Value* evaluate( Context& cx, BinaryExpressionNode* node );
    Value* evaluate( Context& cx, ConditionalExpressionNode* node );
    Value* evaluate( Context& cx, AssignmentExpressionNode* node );
    Value* evaluate( Context& cx, ListNode* node );
    Value* evaluate( Context& cx, StatementListNode* node );
    Value* evaluate( Context& cx, EmptyStatementNode* node );
    Value* evaluate( Context& cx, ExpressionStatementNode* node );
    Value* evaluate( Context& cx, AnnotatedBlockNode* node );
    Value* evaluate( Context& cx, LabeledStatementNode* node );
    Value* evaluate( Context& cx, IfStatementNode* node );
    Value* evaluate( Context& cx, SwitchStatementNode* node );
    Value* evaluate( Context& cx, CaseLabelNode* node );
    Value* evaluate( Context& cx, DoStatementNode* node );
    Value* evaluate( Context& cx, WhileStatementNode* node );
    Value* evaluate( Context& cx, ForInStatementNode* node );
    Value* evaluate( Context& cx, ForStatementNode* node );
    Value* evaluate( Context& cx, WithStatementNode* node );
    Value* evaluate( Context& cx, ContinueStatementNode* node );
    Value* evaluate( Context& cx, BreakStatementNode* node );
    Value* evaluate( Context& cx, ReturnStatementNode* node );
    Value* evaluate( Context& cx, ThrowStatementNode* node );
    Value* evaluate( Context& cx, TryStatementNode* node );
    Value* evaluate( Context& cx, CatchClauseNode* node );
    Value* evaluate( Context& cx, FinallyClauseNode* node );
    Value* evaluate( Context& cx, AnnotatedDefinitionNode* node );
    Value* evaluate( Context& cx, VariableDefinitionNode* node );
    Value* evaluate( Context& cx, VariableBindingNode* node );
    Value* evaluate( Context& cx, FunctionDefinitionNode* node );
    Value* evaluate( Context& cx, FunctionDeclarationNode* node );
    Value* evaluate( Context& cx, FunctionNameNode* node );
    Value* evaluate( Context& cx, FunctionSignatureNode* node );
    Value* evaluate( Context& cx, ParameterNode* node );
    Value* evaluate( Context& cx, ProgramNode* node );

	// 4th Edition features

    Value* evaluate( Context& cx, QualifiedIdentifierNode* node );
    Value* evaluate( Context& cx, UnitExpressionNode* node );
    Value* evaluate( Context& cx, ClassofExpressionNode* node );
    Value* evaluate( Context& cx, CoersionExpressionNode* node );
    Value* evaluate( Context& cx, UseStatementNode* node );
    Value* evaluate( Context& cx, IncludeStatementNode* node );
    Value* evaluate( Context& cx, ImportDefinitionNode* node );
    Value* evaluate( Context& cx, ImportBindingNode* node );
    Value* evaluate( Context& cx, AttributeListNode* node );
    Value* evaluate( Context& cx, ExportDefinitionNode* node );
    Value* evaluate( Context& cx, ExportBindingNode* node );
    Value* evaluate( Context& cx, TypedVariableNode* node );
    Value* evaluate( Context& cx, OptionalParameterNode* node );
    Value* evaluate( Context& cx, ClassDefinitionNode* node );
    Value* evaluate( Context& cx, ClassDeclarationNode* node );
    Value* evaluate( Context& cx, InheritanceNode* node );
    Value* evaluate( Context& cx, NamespaceDefinitionNode* node );
    Value* evaluate( Context& cx, PackageDefinitionNode* node );
};

}
}

#endif // JSILGenerator_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
