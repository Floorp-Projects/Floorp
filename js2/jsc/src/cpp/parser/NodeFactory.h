/*
 * Creates parse tree nodes.
 */

#ifndef NodeFactory_h
#define NodeFactory_h

#include <vector>
#include <algorithm>

#include "Node.h"
#include "Nodes.h"

namespace esc {
namespace v1 {

struct NodeFactory {

	/*
	 * Test driver
	 */

	static int main(int argc, char* argv[]);
	
	/*
	 * roots is a vector of references to all the nodes allocated
	 * since the last call to clear().
	 */
	
	static std::vector<Node*>* roots;
	static void init() {
		NodeFactory::roots = new std::vector<Node*>();
	}

	static void clear_root(Node* root) {
		delete root;
	}

	static void clear() {
		std::for_each(roots->begin(),roots->end(),clear_root);
		roots->clear();
		delete roots;
	}


	// Nodes

    static IdentifierNode* Identifier( std::string name ) {
		IdentifierNode* node = new IdentifierNode(name,0/*in->positionOfMark()*/);
		roots->push_back(node);
		return node;
    }
    static QualifiedIdentifierNode* QualifiedIdentifier( Node* qualifier, IdentifierNode* identifier ) {
        QualifiedIdentifierNode* node = new QualifiedIdentifierNode(qualifier,identifier,identifier->pos());
		roots->push_back(node);
		return node;
    }
    static LiteralNullNode* LiteralNull() {
        LiteralNullNode* node = new LiteralNullNode();
		roots->push_back(node);
		return node;
    }
    static LiteralBooleanNode* LiteralBoolean(bool value) {
        LiteralBooleanNode* node = new LiteralBooleanNode(value);
		roots->push_back(node);
		return node;
    }
    static LiteralArrayNode* LiteralArray( Node* elementlist ) {
        LiteralArrayNode* node = new LiteralArrayNode(elementlist);
		roots->push_back(node);
		return node;
    }
    static LiteralFieldNode* LiteralField( Node* name, Node* value ) {
        LiteralFieldNode* node = new LiteralFieldNode(name,value);
		roots->push_back(node);
		return node;
    }
    static LiteralNumberNode* LiteralNumber( std::string value ) {
        LiteralNumberNode* node = new LiteralNumberNode(value);
		roots->push_back(node);
		return node;
    }
    static LiteralObjectNode* LiteralObject( Node* fieldlist ) {
        LiteralObjectNode* node = new LiteralObjectNode(fieldlist);
		roots->push_back(node);
		return node;
    }
    static LiteralRegExpNode* LiteralRegExp( std::string value ) {
        LiteralRegExpNode* node = new LiteralRegExpNode(value);
		roots->push_back(node);
		return node;
    }
    static LiteralStringNode* LiteralString( std::string value ) {
        LiteralStringNode* node = new LiteralStringNode(value);
		roots->push_back(node);
		return node;
    }
#if 0
    static LiteralTypeNode* LiteralType( Type* type ) {
        LiteralTypeNode* node = new LiteralTypeNode(type);
		roots->push_back(node);
		return node;
    }
#endif
    static LiteralUndefinedNode* LiteralUndefined() {
        LiteralUndefinedNode* node = new LiteralUndefinedNode();
		roots->push_back(node);
		return node;
    }
    static ParenthesizedExpressionNode* ParenthesizedExpression( Node* expr ) {
        ParenthesizedExpressionNode* node = new ParenthesizedExpressionNode(expr,0/*in->positionOfMark()*/);
		roots->push_back(node);
		return node;
    }
    static ParenthesizedListExpressionNode* ParenthesizedListExpression( Node* expr ) {
        ParenthesizedListExpressionNode* node = new ParenthesizedListExpressionNode(expr);
		roots->push_back(node);
		return node;
    }
    static FunctionExpressionNode* FunctionExpression( Node* name, Node* signature, Node* body ) {
        FunctionExpressionNode* node = new FunctionExpressionNode(name,signature,body);
		roots->push_back(node);
		return node;
    }
    static AnnotatedDefinitionNode* AnnotatedDefinition( Node* attributes, Node* definition ) {
        AnnotatedDefinitionNode* node = new AnnotatedDefinitionNode(attributes,definition);
		roots->push_back(node);
		return node;
    }
    static AttributeListNode* AttributeList( Node* item, Node* list ) {
        AttributeListNode* node = new AttributeListNode(item,list);
		roots->push_back(node);
		return node;
    }
    static UnitExpressionNode* UnitExpression( Node* value, Node* type ) {
        UnitExpressionNode* node = new UnitExpressionNode(value,type);
		roots->push_back(node);
		return node;
    }
    static ThisExpressionNode* ThisExpression() {
        ThisExpressionNode* node = new ThisExpressionNode();
		roots->push_back(node);
		return node;
    }
    static SuperExpressionNode* SuperExpression() {
        SuperExpressionNode* node = new SuperExpressionNode();
		roots->push_back(node);
		return node;
    }
    static ListNode* List( ListNode* list, Node* item ) {
        ListNode* node = new ListNode(list,item,item->pos());
		roots->push_back(node);
		return node;
    }
    static PostfixExpressionNode* PostfixExpression( int op, Node* expr ) {
        PostfixExpressionNode* node = new PostfixExpressionNode(op,expr);
		roots->push_back(node);
		return node;
    }
    static NewExpressionNode* NewExpression( Node* member ) {
        NewExpressionNode* node = new NewExpressionNode(member);
		roots->push_back(node);
		return node;
    }
    static ClassofExpressionNode* ClassofExpression( Node* base ) {
        ClassofExpressionNode* node = new ClassofExpressionNode(base);
		roots->push_back(node);
		return node;
    }
    static CallExpressionNode* CallExpression( MemberExpressionNode* member, ListNode* args ) {
        CallExpressionNode* node = new CallExpressionNode(member,args);
		roots->push_back(node);
		return node;
    }
    static GetExpressionNode* GetExpression( MemberExpressionNode* member ) {
        GetExpressionNode* node = new GetExpressionNode(member);
		roots->push_back(node);
		return node;
    }
    static SetExpressionNode* SetExpression( Node* member, Node* args ) {
        SetExpressionNode* node = new SetExpressionNode(member,args);
		roots->push_back(node);
		return node;
    }
    static IndexedMemberExpressionNode* IndexedMemberExpression( Node* base, Node* member ) {
        IndexedMemberExpressionNode* node = new IndexedMemberExpressionNode(base,member,0/*in->positionOfMark()*/);
		roots->push_back(node);
		return node;
    }
    static MemberExpressionNode* MemberExpression( Node* base, Node* name ) {
        MemberExpressionNode* node = new MemberExpressionNode(base,name,0/*in->positionOfMark()*/);
		roots->push_back(node);
		return node;
    }
    static CoersionExpressionNode* CoersionExpression( Node* expr, Node* type ) {
        CoersionExpressionNode* node = new CoersionExpressionNode(expr,type,0/*in->positionOfMark()*/);
		roots->push_back(node);
		return node;
    }
    static UnaryExpressionNode* UnaryExpression( int op, Node* expr ) {
        UnaryExpressionNode* node = new UnaryExpressionNode(op,expr);
		roots->push_back(node);
		return node;
    }
    static BinaryExpressionNode* BinaryExpression( int op, Node* lhs, Node* rhs ) {
        BinaryExpressionNode* node = new BinaryExpressionNode(op,lhs,rhs);
		roots->push_back(node);
		return node;
    }
    static ConditionalExpressionNode* ConditionalExpression( Node* cond, Node* thenexpr, Node* elseexpr ) {
        ConditionalExpressionNode* node = new ConditionalExpressionNode(cond,thenexpr,elseexpr);
		roots->push_back(node);
		return node;
    }
    static AssignmentExpressionNode* AssignmentExpression( Node* lhs, int op, Node* rhs ) {
        AssignmentExpressionNode* node = new AssignmentExpressionNode(lhs,op,rhs);
		roots->push_back(node);
		return node;
    }
    static StatementListNode* StatementList( StatementListNode* list, Node* item ) {
        StatementListNode* node = new StatementListNode(list,item);
		roots->push_back(node);
		return node;
    }
    static EmptyStatementNode* EmptyStatement() {
        EmptyStatementNode* node = new EmptyStatementNode();
		roots->push_back(node);
		return node;
    }
    static ExpressionStatementNode* ExpressionStatement( Node* expr ) {
        ExpressionStatementNode* node = new ExpressionStatementNode(expr);
		roots->push_back(node);
		return node;
    }
    static AnnotatedBlockNode* AnnotatedBlock( Node* attributes, Node* definition ) {
        AnnotatedBlockNode* node = new AnnotatedBlockNode(attributes,definition);
		roots->push_back(node);
		return node;
    }
    static LabeledStatementNode* LabeledStatement( Node* label, Node* statement ) {
        LabeledStatementNode* node = new LabeledStatementNode(label,statement);
		roots->push_back(node);
		return node;
    }
    static IfStatementNode* IfStatement( Node* test, Node* tblock, Node* eblock ) {
        IfStatementNode* node = new IfStatementNode(test,tblock,eblock);
		roots->push_back(node);
		return node;
    }
    static SwitchStatementNode* SwitchStatement( Node* expr, StatementListNode* statements ) {
        SwitchStatementNode* node = new SwitchStatementNode(expr,statements);
		roots->push_back(node);
		return node;
    }
    static CaseLabelNode* CaseLabel( Node* label ) {
        CaseLabelNode* node = new CaseLabelNode(label);
		roots->push_back(node);
		return node;
    }
    static DoStatementNode* DoStatement( Node* block, Node* expr ) {
        DoStatementNode* node = new DoStatementNode(block,expr);
		roots->push_back(node);
		return node;
    }
    static WhileStatementNode* WhileStatement( Node* expr, Node* statement ) {
        WhileStatementNode* node = new WhileStatementNode(expr,statement);
		roots->push_back(node);
		return node;
    }
    static ForInStatementNode* ForInStatement( Node* expr1, Node* expr2, Node* statement ) {
        ForInStatementNode* node = new ForInStatementNode(expr1,expr2,statement);
		roots->push_back(node);
		return node;
    }
    static ForStatementNode* ForStatement( Node* expr1, Node* expr2, Node* expr3, Node* statement ) {
        ForStatementNode* node = new ForStatementNode(expr1,expr2,expr3,statement);
		roots->push_back(node);
		return node;
    }
    static WithStatementNode* WithStatement( Node* expr, Node* statement ) {
        WithStatementNode* node = new WithStatementNode(expr,statement);
		roots->push_back(node);
		return node;
    }
    static ContinueStatementNode* ContinueStatement(Node* expr) {
        ContinueStatementNode* node = new ContinueStatementNode(expr);
		roots->push_back(node);
		return node;
    }
    static BreakStatementNode* BreakStatement(Node* expr) {
        BreakStatementNode* node = new BreakStatementNode(expr);
		roots->push_back(node);
		return node;
    }
    static ReturnStatementNode* ReturnStatement( Node* expr ) {
        ReturnStatementNode* node = new ReturnStatementNode(expr);
		roots->push_back(node);
		return node;
    }
    static ThrowStatementNode* ThrowStatement(Node* list) {
        ThrowStatementNode* node = new ThrowStatementNode(list);
		roots->push_back(node);
		return node;
    }
    static TryStatementNode* TryStatement(Node* tryblock, StatementListNode* catchlist, Node* finallyblock) {
        TryStatementNode* node = new TryStatementNode(tryblock,catchlist,finallyblock);
		roots->push_back(node);
		return node;
    }
    static CatchClauseNode* CatchClause(Node* parameter, Node* block) {
        CatchClauseNode* node = new CatchClauseNode(parameter,block);
		roots->push_back(node);
		return node;
    }
    static FinallyClauseNode* FinallyClause( Node* block ) {
        FinallyClauseNode* node = new FinallyClauseNode(block);
		roots->push_back(node);
		return node;
    }
    static IncludeStatementNode* IncludeStatement( Node* list ) {
        IncludeStatementNode* node = new IncludeStatementNode(list);
		roots->push_back(node);
		return node;
    }
    static UseStatementNode* UseStatement( Node* expr ) {
        UseStatementNode* node = new UseStatementNode(expr);
		roots->push_back(node);
		return node;
    }
    static ImportDefinitionNode* ImportDefinition( Node* item, Node* list ) {
        ImportDefinitionNode* node = new ImportDefinitionNode(item,list);
		roots->push_back(node);
		return node;
    }
    static ImportBindingNode* ImportBinding( Node* identifer, Node* item ) {
        ImportBindingNode* node = new ImportBindingNode(identifer,item);
		roots->push_back(node);
		return node;
    }
    static ExportDefinitionNode* ExportDefinition( Node* list ) {
        ExportDefinitionNode* node = new ExportDefinitionNode(list);
		roots->push_back(node);
		return node;
    }
    static ExportBindingNode* ExportBinding( Node* name, Node* value ) {
        ExportBindingNode* node = new ExportBindingNode(name,value);
		roots->push_back(node);
		return node;
    }
    static VariableDefinitionNode* VariableDefinition( int kind, Node* list ) {
        VariableDefinitionNode* node = new VariableDefinitionNode(kind,list,list->pos());
		roots->push_back(node);
		return node;
    }
    static VariableBindingNode* VariableBinding( Node* identifier, Node* initializer ) {
        VariableBindingNode* node = new VariableBindingNode(identifier,initializer);
		roots->push_back(node);
		return node;
    }
    static TypedVariableNode* TypedVariable( Node* identifier, Node* type ) {
        TypedVariableNode* node = new TypedVariableNode(identifier,type,type!=NULL?type->pos():identifier->pos());
		roots->push_back(node);
		return node;
    }
    static FunctionDefinitionNode* FunctionDefinition( Node* decl, Node* body ) {
        FunctionDefinitionNode* node = new FunctionDefinitionNode(decl,body);
		roots->push_back(node);
		return node;
    }
    static FunctionDeclarationNode* FunctionDeclaration( Node* name, Node* signature ) {
        FunctionDeclarationNode* node = new FunctionDeclarationNode(name,signature);
		roots->push_back(node);
		return node;
    }
    static FunctionNameNode* FunctionName( int kind, Node* name ) {
        FunctionNameNode* node = new FunctionNameNode(kind,name);
		roots->push_back(node);
		return node;
    }
    static RestParameterNode* RestParameter( Node* expr ) {
        RestParameterNode* node = new RestParameterNode(expr);
		roots->push_back(node);
		return node;
    }
    static ParameterNode* Parameter( Node* identifer, Node* type ) {
        ParameterNode* node = new ParameterNode(identifer,type);
		roots->push_back(node);
		return node;
    }
    static OptionalParameterNode* OptionalParameter( Node* identifer, Node* init ) {
        OptionalParameterNode* node = new OptionalParameterNode(identifer,init);
		roots->push_back(node);
		return node;
    }
    static NamedParameterNode* NamedParameter( Node* name, Node* parameter ) {
        NamedParameterNode* node = new NamedParameterNode(name,parameter);
		roots->push_back(node);
		return node;
    }
    static ClassDeclarationNode* ClassDeclaration( Node* name ) {
        ClassDeclarationNode* node = new ClassDeclarationNode(name);
		roots->push_back(node);
		return node;
    }
    static ClassDefinitionNode* ClassDefinition( Node* name, Node* interfaces, Node* statements ) {
        ClassDefinitionNode* node = new ClassDefinitionNode(name,interfaces,statements);
		roots->push_back(node);
		return node;
    }
    static InheritanceNode* Inheritance( Node* baseclass, Node* interfaces ) {
        InheritanceNode* node = new InheritanceNode(baseclass,interfaces);
		roots->push_back(node);
		return node;
    }
    static NamespaceDefinitionNode* NamespaceDefinition( Node* identifier, Node* list ) {
        NamespaceDefinitionNode* node = new NamespaceDefinitionNode(identifier,list);
		roots->push_back(node);
		return node;
    }
    static LanguageDeclarationNode* LanguageDeclaration( Node* list ) {
        LanguageDeclarationNode* node = new LanguageDeclarationNode(list);
		roots->push_back(node);
		return node;
    }
    static PackageDefinitionNode* PackageDefinition( Node* name, Node* block ) {
        PackageDefinitionNode* node = new PackageDefinitionNode(name,block);
		roots->push_back(node);
		return node;
    }
    static ProgramNode* Program( Node* statements ) {
        ProgramNode* node = new ProgramNode(statements);
		roots->push_back(node);
		return node;
    }
};

}
}

#endif // NodeFactory_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
