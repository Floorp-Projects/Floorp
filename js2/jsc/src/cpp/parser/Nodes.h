/*
 * Parse tree nodes.
 */

#ifndef Nodes_h
#define Nodes_h

#include <string>

#include "Node.h"
#include "Evaluator.h"
#include "Tokens.h"
#include "Type.h"

namespace esc {
namespace v1 {

class ReferenceValue;

/*
 * IdentifierNode
 */

struct IdentifierNode : public Node {

	std::string name;
	Value* ref;

    IdentifierNode( std::string name, int pos )  : Node(pos) {
        this->name = name;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

	std::string toString() {
        return "identifier( " + name + " )";
    }
};

/**
 * AnnotatedBlockNode
 */

struct AnnotatedBlockNode : public Node {

    Node* attributes;
	Node* statements;

    AnnotatedBlockNode( Node* attributes, Node* statements ) {
        this->attributes = attributes;
        this->statements = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    Node* last() {
        return statements->last();
    }

    std::string toString() {
        return "annotatedblock";
    }
};

/**
 * AnnotatedDefinitionNode
 */

struct AnnotatedDefinitionNode : public Node {

    Node* attributes;
	Node* definition;

    AnnotatedDefinitionNode( Node* attributes, Node* definition ) {
        this->attributes = attributes;
        this->definition = definition;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "annotateddefinition";
    }
};

/**
 * AssignmentExpressionNode
 */

struct AssignmentExpressionNode : public Node, private Tokens {

    Node* lhs;
	Node* rhs;
    int  op;
    Value ref;

    AssignmentExpressionNode( Node* lhs, int op, Node* rhs ) {
        this->lhs = lhs;
        this->op = op;
        this->rhs = rhs;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "assignmentexpression";
    }
};

/**
 * AttributeListNode
 */

struct AttributeListNode : public Node {

    Node* item;
	Node* list;

    AttributeListNode( Node* item, Node* list ) {
        this->item = item;
        this->list = list;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "attributelist";
    }
};

/**
 * BinaryExpressionNode
 */

struct BinaryExpressionNode : public Node {

    Node* lhs;
	Node* rhs;
    int  op;
	//Slot lhs_slot,rhs_slot;

    BinaryExpressionNode( int op, Node* lhs, Node* rhs ) {
        this->op  = op;
        this->lhs = lhs;
        this->rhs = rhs;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "binaryexpression";
    }
};

/**
 * BreakStatementNode
 */

struct BreakStatementNode : public Node {

    Node* expr;

    BreakStatementNode(Node* expr) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "breakstatement";
    }
};

/*
 * CallExpressionNode
 */

struct CallExpressionNode : public Node {

    MemberExpressionNode* member;
	ListNode* args;

	ReferenceValue* ref;

    CallExpressionNode( MemberExpressionNode* member, ListNode* args ) {
        this->member = member;
        this->args   = args;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "callexpression";
    }
};

/*
 * GetExpressionNode
 */

struct GetExpressionNode : public Node {

    MemberExpressionNode* member;

	ReferenceValue* ref;

    GetExpressionNode( MemberExpressionNode* member ) {
        this->member = member;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "callexpression";
    }
};

/*
 * SetExpressionNode
 */

struct SetExpressionNode : public Node {

    Node* member;
	Node* args;

	ReferenceValue* ref;

    SetExpressionNode( Node* member, Node* args ) {
        this->member = member;
        this->args   = args;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "setexpression";
    }
};

/**
 * CaseLabelNode
 */

struct CaseLabelNode : public Node {

    Node* label;
    
    CaseLabelNode( Node* label ) {
        this->label = label;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "caselabel";
    }
};

/**
 * CatchClauseNode
 */

struct CatchClauseNode : public Node {

    Node* parameter;
	Node* statements;

    CatchClauseNode(Node* parameter, Node* statements) {
        this->parameter = parameter;
        this->statements = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "catchclause";
    }
};

/**
 * ClassDeclarationNode
 */

struct ClassDeclarationNode : public Node {

    Node* name;

    ClassDeclarationNode( Node* name ) {
        this->name = name;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "classdeclaration";
    }
};

/**
 * ClassDefinitionNode
 */

struct ClassDefinitionNode : public Node {

    Node* name;
	Node* statements;
    InheritanceNode* inheritance;
    //Slot slot;

    static ClassDefinitionNode* ClassDefinition( Node* name, Node* inheritance, Node* statements ) {
        return new ClassDefinitionNode(name,inheritance,statements);
    }

    ClassDefinitionNode( Node* name, Node* inheritance, Node* statements ) {
        this->name        = name;
        this->inheritance = (InheritanceNode*)inheritance;
        this->statements  = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    Node* first() {
        return statements->first();
    }

    Node* last() {
        return statements->last();
    }

    std::string toString() {
        return "classdefinition";
    }
};

/**
 * ClassofExpressionNode
 */

struct ClassofExpressionNode : public Node {

    Node* base;

    ClassofExpressionNode( Node* base ) {
        this->base = base;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "classofexpression";
    }
};

/**
 * CoersionExpressionNode
 */

struct CoersionExpressionNode : public Node {

    Node* expr;
	Node* type;
    std::string type_name;

    CoersionExpressionNode( Node* expr, Node* type, int pos )  : Node(pos) {
        this->expr = expr;
        this->type = type;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "coersionexpression";
    }
};

/**
 * ConditionalExpressionNode
 */

struct ConditionalExpressionNode : public Node {

	Node* condition;
	Node* thenexpr;
	Node* elseexpr;

	ConditionalExpressionNode( Node* condition, Node* thenexpr, Node* elseexpr ) {
		this->condition = condition;
		this->thenexpr  = thenexpr;
		this->elseexpr  = elseexpr;
	}

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

	std::string toString() {
		return "conditionalexpression";
	}
};

/**
 * ContinueStatementNode
 */

struct ContinueStatementNode : public Node {

    Node* expr;

    ContinueStatementNode(Node* expr) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "continuestatement";
    }
};

/**
 * DoStatementNode
 */

struct DoStatementNode : public Node {

    Node* statements;
	Node* expr;

    DoStatementNode(Node* statements, Node* expr) {
        this->statements = statements;
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "dostatement";
    }
};

/**
 * ElementListNode
 */

struct ElementListNode : public Node {

    Node* list;
	Node* item;

    ElementListNode( Node* list, Node* item ) {
        this->list = list;
        this->item = item;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "elementlist";
    }
};

/**
 * EmptyStatementNode
 */

struct EmptyStatementNode : public Node {

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "emptystatement";
    }
};

/**
 * ExpressionStatementNode
 */

struct ExpressionStatementNode : public Node {

    Node* expr;

    ExpressionStatementNode( Node* expr ) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "expressionstatement";
    }
};

/**
 * ExportBindingNode
 */

struct ExportBindingNode : public Node {
  
    Node* name;
    Node* value;

    ExportBindingNode( Node* name, Node* value ) {
        this->name  = name;
	    this->value = value;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "exportbinding";
    }
};

/**
 * ExportDefinitionNode
 */

struct ExportDefinitionNode : public Node {
  
    Node* list;

    ExportDefinitionNode( Node* list ) {
	    this->list = list;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "exportdefinition";
    }
};

/**
 * FieldListNode
 */

struct FieldListNode : public Node {

    Node* list;
	Node* field;

    FieldListNode( Node* list, Node* field ) {
        this->list = list;
        this->field     = field;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "fieldlist";
    }
};

/**
 * FinallyClauseNode
 */

struct FinallyClauseNode : public Node {

    Node* statements;
    
    FinallyClauseNode(Node* statements) {
        this->statements = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "finallyclause";
    }
};

/**
 * ForStatementNode
 */

struct ForStatementNode : public Node {

    Node* initialize;
	Node* test;
	Node* increment;
	Node* statement;

    ForStatementNode( Node* initialize, Node* test, Node* increment, Node* statement ) {

        if( initialize == NULL ) {
//            this->initialize = NodeFactory::LiteralUndefined();
        } else {
            this->initialize = initialize;
        }

        if( test == NULL ) {
//            this->test = NodeFactory::LiteralBoolean(true);
        } else {
            this->test = test;
        }

        if( increment == NULL ) {
//            this->increment = NodeFactory::LiteralUndefined();
        } else {
            this->increment = increment;
        }

        this->statement = statement;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "forstatement";
    }
};

/**
 * ForInStatementNode
 */

struct ForInStatementNode : public Node {

    Node* property;
	Node* object;
	Node* statement;

    ForInStatementNode( Node* property, Node* object, Node* statement ) {
        this->property  = property;
        this->object    = object;
        this->statement = statement;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "forinstatement";
    }
};

/**
 * FunctionExpressionNode
 */

struct FunctionExpressionNode : public Node {

    Node* name;
	Node* signature;
	Node* body;

    FunctionExpressionNode( Node* name, Node* signature, Node* body ) {
        this->name      = name;
        this->signature = signature;
        this->body      = body;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "functionexpression";
    }
};

/**
 * ReturnStatementNode
 */

struct ReturnStatementNode : public Node {

    Node* expr;

    ReturnStatementNode( Node* expr ) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "returnstatement";
    }
};

/**
 * FunctionDeclarationNode
 */

struct FunctionDeclarationNode : public Node {

    Node* name;
	Node* signature;
    //Slot slot;
	Value ref;

    FunctionDeclarationNode( Node* name, Node* signature ) {
        this->name      = name;
        this->signature = signature;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "functiondeclaration";
    }
};

/**
 * FunctionDefinitionNode
 */

struct FunctionDefinitionNode : public Node {

    Node* decl;
	Node* body;
	int fixedCount;

    FunctionDefinitionNode( Node* decl, Node* body ) {
        this->decl = decl;
        this->body = body;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    Node* first() {
        return body->first();
    }

    Node* last() {
        return body->last();
    }

    std::string toString() {
        return "functiondefinition";
    }
};

/**
 * FunctionNameNode
 */

struct FunctionNameNode : public Node {

    Node* name;
    int  kind;

    FunctionNameNode( int kind, Node* name ) {
	    this->kind = kind;
        this->name = (IdentifierNode*)name;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "functionname";
    }
};

/**
 * FunctionSignatureNode
 */

struct FunctionSignatureNode : public Node {

    Node* parameter;
	Node* result;
    //Slot slot;

    FunctionSignatureNode( Node* parameter, Node* result ) {
        this->parameter = parameter;
        this->result    = result;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "functionsignature";
    }
};

/**
 * IfStatementNode
 */

struct IfStatementNode : public Node {

    Node* condition;
	Node* thenactions;
	Node* elseactions;

    IfStatementNode( Node* condition, Node* thenactions, Node* elseactions ) {
        this->condition   = condition;
        this->thenactions = thenactions;
        this->elseactions = elseactions;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "ifstatement";
    }

    bool isBranch() {
        return true;
    }
};

/**
 * ImportBindingNode
 */

struct ImportBindingNode : public Node {

    Node* identifier;
    Node* item;

    ImportBindingNode(Node* identifier, Node* item) {
        this->identifier = identifier;
        this->item = item;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "importbinding";
    }
};

/**
 * ImportDefinitionNode
 */

struct ImportDefinitionNode : public Node {

    Node* item;
	Node* list;

    ImportDefinitionNode(Node* item, Node* list) {
        this->item = item;
        this->list = list;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "importdefinition";
    }
};

/**
 * IncludeStatementNode
 */

struct IncludeStatementNode : public Node {

    Node* item;

    IncludeStatementNode(Node* item) {
        this->item = item;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "includestatement";
    }
};

/**
 * IndexedMemberExpressionNode
 */

struct IndexedMemberExpressionNode : public Node {

    Node* base;
	Node* expr;

    IndexedMemberExpressionNode( Node* base, Node* expr, int pos )  : Node(pos) {
        this->base = base;
	    this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "indexedmemberexpression";
    }
};

/**
 * InheritanceNode
 */

struct InheritanceNode : public Node {

    Node* baseclass;
	Node* interfaces;

    InheritanceNode( Node* baseclass, Node* interfaces ) {
        this->baseclass  = baseclass;
        this->interfaces = interfaces;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "inheritance";
    }
};

/**
 * InterfaceDeclarationNode
 */

struct InterfaceDeclarationNode : public Node {

    Node* name;

    InterfaceDeclarationNode( Node* name ) {
        this->name = name;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "interfacedeclaration";
    }
};

/**
 * InterfaceDefinitionNode
 */

struct InterfaceDefinitionNode : public Node {

    Node* name;
	Node* interfaces;
	Node* statements;
    //Slot slot;

    InterfaceDefinitionNode( Node* name, Node* interfaces, Node* statements ) {
        this->name       = name;
        this->interfaces = interfaces;
        this->statements = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "interfacedefinition";
    }
};

/**
 * LabeledStatementNode
 */

struct LabeledStatementNode : public Node {

    Node* label;
    Node* statement;

    LabeledStatementNode(Node* label, Node* statement) {
        this->label     = label;
        this->statement = statement;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "labeledstatement";
    }
};

/**
 * LanguageDeclarationNode
 */

struct LanguageDeclarationNode : public Node {

    Node* list;

    LanguageDeclarationNode(Node* list) {
        this->list = list;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "languagedeclaration";
    }
};

/**
 * ListNode
 */

struct ListNode : public Node {

    ListNode* list;
	Node* item;

    ListNode( ListNode* list, Node* item, int pos )  : Node(pos) {
        this->list = list;
        this->item = item;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    short size() {
		ListNode* node = list;
		short count = 1;
		while( node ) {
			++count; 
			node = node->list; 
		}
		return count;
	}
    
	int pos() {
        return item->pos();
    }

    std::string toString() {
        return "list";
    }
};

/**
 * LiteralArrayNode
 */

struct LiteralArrayNode : public Node {

    Node* elementlist;
	Value value;

    LiteralArrayNode( Node* elementlist ) {
        this->elementlist = elementlist;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literalarray";
    }
};

/**
 * LiteralBooleanNode
 */

struct LiteralBooleanNode : public Node {

    bool value;

    LiteralBooleanNode( bool value ) {
        this->value = value;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literalboolean";
    }
};

/**
 * LiteralFieldNode
 */

struct LiteralFieldNode : public Node {

    Node* name;
    Node* value;
	Value ref;

    LiteralFieldNode( Node* name, Node* value ) {
        this->name  = name;
        this->value = value;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literalfield";
    }
};

/**
 * LiteralNullNode
 */

struct LiteralNullNode : public Node {

  LiteralNullNode() {
  }

  Value* evaluate( Context& cx, Evaluator* evaluator ) {
    return evaluator->evaluate( cx, this );
  }

  std::string toString() {
    return "literalNULL";
  }
};

/**
 * LiteralNumberNode
 */

struct LiteralNumberNode : public Node {

    std::string value;

    LiteralNumberNode( std::string value ) {
        this->value = value;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literalnumber(";
    }
};

/**
 * LiteralObjectNode
 */

struct LiteralObjectNode : public Node {

    Node* fieldlist;
	Value value;

    LiteralObjectNode( Node* fieldlist ) {
        this->fieldlist = fieldlist;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literalobject";
    }
};

/**
 * LiteralRegExpNode
 */

struct LiteralRegExpNode : public Node {

    std::string value;

    LiteralRegExpNode( std::string value ) {
        this->value = value;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literalregexp";
    }
};

/**
 * LiteralStringNode
 */

struct LiteralStringNode : public Node {

    std::string value;

    LiteralStringNode( std::string value ) {
        this->value = value;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literalstring";
    }
};

/**
 * LiteralTypeNode
 */

struct LiteralTypeNode : public Node {

    Type* type;

    LiteralTypeNode( Type* type ) {
        this->type = type;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "literaltype";
    }
};

/**
 * LiteralUndefinedNode
 */

struct LiteralUndefinedNode : public Node {

    LiteralUndefinedNode() {
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
      return evaluator->evaluate( cx, this );
    }

    std::string toString() {
      return "literalundefined";
    }
};

/**
 * MemberExpressionNode
 */

struct MemberExpressionNode : public Node {

    Node* base;
	Node* expr;
    Value ref;

    MemberExpressionNode( Node* base, Node* expr, int pos )  : Node(pos) {
        this->base = base;
	    this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "memberexpression";
    }
};

/**
 * NamedParameterNode
 */

struct NamedParameterNode : public Node {
  
    Node* name;
    Node* parameter;

    NamedParameterNode( Node* name, Node* parameter ) {
        this->name      = name;
	    this->parameter = parameter;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "namedparameter";
    }
};

/**
 * NamespaceDefinitionNode
 */

struct NamespaceDefinitionNode : public Node {

    Node* name;
	Node* publiclist;

    NamespaceDefinitionNode( Node* name, Node* publiclist ) {
        this->name       = name;
        this->publiclist = publiclist;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "namespacedefinition";
    }
};

/**
 * NewExpressionNode
 */

struct NewExpressionNode : public Node {

    Node* expr;

    NewExpressionNode( Node* expr ) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "newexpression";
    }
};

/**
 * PackageDefinitionNode
 */

struct PackageDefinitionNode : public Node {

    Node* name;
	Node* statements;

    PackageDefinitionNode( Node* name, Node* statements ) {
        this->name  = name;
        this->statements = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "packagedefinition";
    }
};

/**
 * OptionalParameterNode
 */

struct OptionalParameterNode : public Node {
  
    Node* parameter;
    Node* initializer;

    OptionalParameterNode( Node* parameter, Node* initializer ) {
        this->parameter  = parameter;
	    this->initializer = initializer;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "optionalparameter";
    }
};

/**
 * ParameterNode
 */

struct ParameterNode : public Node {
  
    Node* identifier;
    Node* type;
    //Slot slot;

    ParameterNode( Node* identifier, Node* type ) {
        this->identifier  = identifier;
	    this->type        = type;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "parameter";
    }
};

/**
 * ParenthesizedExpressionNode
 */

struct ParenthesizedExpressionNode : public Node {

    Node* expr;

    ParenthesizedExpressionNode( Node* expr, int pos ) : Node(pos) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "parenthesizedexpression";
    }
};

/**
 * ParenthesizedListExpressionNode
 */

struct ParenthesizedListExpressionNode : public Node {

    Node* expr;

    ParenthesizedListExpressionNode( Node* expr ) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "parenthesizedlistexpression";
    }
};

/**
 * PostfixExpressionNode
 */

struct PostfixExpressionNode : public Node {

    int  op;
    Node* expr;

    PostfixExpressionNode( int op, Node* expr ) {
        this->op   = op;
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "postfixexpression";
    }
};

/**
 * ProgramNode
 */

struct ProgramNode : public Node {

    Node* statements;

    ProgramNode(Node* statements) {
        this->statements = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "program";
    }
};

/**
 * QualifiedIdentifierNode
 *
 * QualifiedIdentifier : QualifiedIdentifier Identifier
 */

struct QualifiedIdentifierNode : IdentifierNode {

    Node* qualifier; 

    QualifiedIdentifierNode( Node* qualifier, IdentifierNode* identifier, int pos ) 
			: IdentifierNode(identifier->name,pos) {
        this->qualifier = qualifier;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "qualifiedidentifier";
    }
};

/**
 * RestParameterNode
 */

struct RestParameterNode : public Node {
  
    Node* parameter;

    RestParameterNode( Node* parameter ) {
        this->parameter = parameter;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "restparameter";
    }
};

/**
 * StatementListNode
 */

struct StatementListNode : public Node {

    StatementListNode* list;
    Node* item;

    StatementListNode( StatementListNode* list, Node* item ) {
        this->list = list;
	    this->item = item;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    Node* first() {
        StatementListNode* node = this;
        while(node->list!=NULL) {
            node = node->list;
        }
        return node->item;
    }

    Node* last() {
        return this->item;
    }

    std::string toString() {
        return "statementlist";
    }
};

/**
 * SwitchStatementNode
 */

struct SwitchStatementNode : public Node {

    Node* expr;
    StatementListNode* statements;
    
    SwitchStatementNode( Node* expr, StatementListNode* statements ) {
        this->expr = expr;
        this->statements = statements;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "switchstatement";
    }
};

/**
 * SuperExpressionNode
 */


struct SuperExpressionNode : public Node {

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "superexpression";
    }
};

/**
 * ThisExpressionNode
 */


struct ThisExpressionNode : public Node {

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "thisexpression";
    }
};

/**
 * ThrowStatementNode
 */

struct ThrowStatementNode : public Node {

    Node* expr;
    
    ThrowStatementNode(Node* expr) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "throwstatement";
    }
};

/**
 * TryStatementNode
 */

struct TryStatementNode : public Node {

    Node* tryblock;
    StatementListNode* catchlist;
    Node* finallyblock;

    TryStatementNode(Node* tryblock, StatementListNode* catchlist, Node* finallyblock) {
        this->tryblock     = tryblock;
        this->catchlist    = catchlist;
        this->finallyblock = finallyblock;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "trystatement";
    }
};

/**
 * TypedVariableNode
 */

struct TypedVariableNode : public Node {
  
    IdentifierNode* identifier;
    IdentifierNode* type;
    //Slot slot;

    TypedVariableNode( Node* identifier, Node* type, int pos ) : Node(pos) {
        this->identifier  = (IdentifierNode*)identifier;
	    this->type        = (IdentifierNode*)type;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "typedvariable";
    }
};

/**
 * UnaryExpressionNode
 */

struct UnaryExpressionNode : public Node {

    Node* expr;
    int  op;

    UnaryExpressionNode( int op, Node* expr ) {
	    this->op   = op;
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "unaryexpression";
    }
};

/**
 * UnitExpressionNode
 */

struct UnitExpressionNode : public Node {

    Node* value;
	Node* type;

    UnitExpressionNode( Node* value, Node* type ) {
        this->value = value;
        this->type  = type;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "unitexpression";
    }
};

/**
 * UseStatementNode
 */

struct UseStatementNode : public Node {

    Node* expr;

    UseStatementNode( Node* expr ) {
        this->expr = expr;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "usestatement";
    }
};

/**
 * VariableBindingNode
 */

struct VariableBindingNode : public Node {
  
    TypedVariableNode* variable;
    Node* initializer;

    VariableBindingNode( Node* variable, Node* initializer ) {
        this->variable  = (TypedVariableNode*)variable;
	    this->initializer = initializer;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    int pos() {
        return variable->pos();
    }

    std::string toString() {
        return "variablebinding";
    }
};

/**
 * VariableDefinitionNode
 */

struct VariableDefinitionNode : public Node {
  
    int  kind;
    Node* list;

    VariableDefinitionNode( int kind, Node* list, int pos ) : Node(pos) {
        this->kind = kind;
	    this->list = list;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "variabledefinition";
    }
};

/**
 * WhileStatementNode
 */

struct WhileStatementNode : public Node {

    Node* expr;
	Node* statement;

    WhileStatementNode( Node* expr, Node* statement ) {
	    this->expr      = expr;
	    this->statement = statement;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    bool isBranch() {
        return true;
    }

    std::string toString() {
        return "whilestatement";
    }
};

/**
 * WithStatementNode
 */

struct WithStatementNode : public Node {

    Node* expr;
    Node* statement;

    WithStatementNode( Node* expr, Node* statement ) {
	    this->expr      = expr;
	    this->statement = statement;
    }

    Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return evaluator->evaluate( cx, this );
    }

    std::string toString() {
        return "withstatement";
    }
};

}
}

#if 0

#endif // 0

#endif // Nodes_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
