/**
 * The base visitor object extended by semantic evaluators.
 *
 * This is a visitor that is used by the compiler for various forms for
 * evaluation of a parse tree (e.g. a type evaluator might compute the 
 * static type of an expression.)
 */

#ifndef Evaluator_h
#define Evaluator_h

#include "Value.h"
#include "Context.h"

namespace esc {
namespace v1 {

struct Node;
struct IdentifierNode;
struct ThisExpressionNode; 
struct SuperExpressionNode;
struct QualifiedIdentifierNode; 
struct LiteralNullNode; 
struct LiteralBooleanNode; 
struct LiteralNumberNode; 
struct LiteralStringNode; 
struct LiteralUndefinedNode;
struct LiteralRegExpNode;
struct UnitExpressionNode;
struct FunctionExpressionNode;
struct ParenthesizedExpressionNode;
struct ParenthesizedListExpressionNode;
struct LiteralObjectNode;
struct LiteralFieldNode;
struct LiteralArrayNode;
struct PostfixExpressionNode; 
struct NewExpressionNode; 
struct IndexedMemberExpressionNode; 
struct ClassofExpressionNode;
struct MemberExpressionNode; 
struct CoersionExpressionNode; 
struct CallExpressionNode; 
struct GetExpressionNode; 
struct SetExpressionNode; 
struct UnaryExpressionNode; 
struct BinaryExpressionNode; 
struct ConditionalExpressionNode; 
struct AssignmentExpressionNode; 
struct ListNode; 
struct StatementListNode; 
struct EmptyStatementNode; 
struct ExpressionStatementNode; 
struct AnnotatedBlockNode; 
struct LabeledStatementNode; 
struct IfStatementNode; 
struct SwitchStatementNode; 
struct CaseLabelNode; 
struct DoStatementNode; 
struct WhileStatementNode; 
struct ForInStatementNode; 
struct ForStatementNode; 
struct WithStatementNode; 
struct ContinueStatementNode; 
struct BreakStatementNode; 
struct ReturnStatementNode; 
struct ThrowStatementNode; 
struct TryStatementNode; 
struct CatchClauseNode; 
struct FinallyClauseNode; 
struct UseStatementNode; 
struct IncludeStatementNode; 
struct ImportDefinitionNode; 
struct ImportBindingNode; 
struct AnnotatedDefinitionNode; 
struct AttributeListNode; 
struct ExportDefinitionNode; 
struct ExportBindingNode; 
struct VariableDefinitionNode; 
struct VariableBindingNode; 
struct TypedVariableNode; 
struct FunctionDefinitionNode; 
struct FunctionDeclarationNode; 
struct FunctionNameNode; 
struct FunctionSignatureNode; 
struct ParameterNode; 
struct OptionalParameterNode; 
struct RestParameterNode;
struct NamedParameterNode;
struct ClassDefinitionNode; 
struct ClassDeclarationNode; 
struct InheritanceNode; 
struct NamespaceDefinitionNode; 
struct PackageDefinitionNode; 
struct LanguageDeclarationNode;
struct ProgramNode; 

class Evaluator {

public:

    // Base node
    
	virtual Value* evaluate( Context& cx, Node* node ) { 
        throw;
    }

	// Expression evaluators

	virtual Value* evaluate( Context& cx, IdentifierNode* node ) { 
        throw;
    }

    // Expression evaluators

    virtual Value* evaluate( Context& cx, ThisExpressionNode* node ) { 
		throw;
    }
    virtual Value* evaluate( Context& cx, QualifiedIdentifierNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, LiteralBooleanNode* node ) { 
        throw;
    }

    virtual Value* evaluate( Context& cx, LiteralNumberNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, LiteralStringNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, LiteralUndefinedNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, LiteralRegExpNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, UnitExpressionNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, FunctionExpressionNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, ParenthesizedExpressionNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, ParenthesizedListExpressionNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, LiteralObjectNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, LiteralFieldNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, LiteralArrayNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, PostfixExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, NewExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, IndexedMemberExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ClassofExpressionNode* node ) {
        throw;
    }
    virtual Value* evaluate( Context& cx, MemberExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, CoersionExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, CallExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, GetExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, SetExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, UnaryExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, BinaryExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ConditionalExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, AssignmentExpressionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ListNode* node ) { 
        throw;
    }

    // Statements

    virtual Value* evaluate( Context& cx, StatementListNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, EmptyStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ExpressionStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, AnnotatedBlockNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, LabeledStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, IfStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, SwitchStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, CaseLabelNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, DoStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, WhileStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ForInStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ForStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, WithStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ContinueStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, BreakStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ReturnStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ThrowStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, TryStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, CatchClauseNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, FinallyClauseNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, UseStatementNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, IncludeStatementNode* node ) { 
        throw;
    }

    // Definitions

    virtual Value* evaluate( Context& cx, ImportDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ImportBindingNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, AnnotatedDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, AttributeListNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ExportDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ExportBindingNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, VariableDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, VariableBindingNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, TypedVariableNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, FunctionDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, FunctionDeclarationNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, FunctionNameNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, FunctionSignatureNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ParameterNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, OptionalParameterNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ClassDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ClassDeclarationNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, InheritanceNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, NamespaceDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, PackageDefinitionNode* node ) { 
        throw;
    }
    virtual Value* evaluate( Context& cx, ProgramNode* node ) { 
        throw;
    }
};

}
}

#endif // Evaluator_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
