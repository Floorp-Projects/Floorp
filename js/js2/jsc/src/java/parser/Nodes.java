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

/*
 * Parse tree nodes.
 */

package com.compilercompany.ecmascript;

/**
 * AnnotatedBlockNode
 */

final class AnnotatedBlockNode extends Node {

    Node attributes, statements;

    AnnotatedBlockNode( Node attributes, Node statements ) {
        this.attributes = attributes;
        this.statements = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public Node last() {
        return statements.last();
    }

    public String toString() {
        return "annotatedblock( " + attributes + ", " + statements + " )";
    }
}

/**
 * AnnotatedDefinitionNode
 */

final class AnnotatedDefinitionNode extends Node {

    Node attributes, definition;

    AnnotatedDefinitionNode( Node attributes, Node definition ) {
        this.attributes = attributes;
        this.definition = definition;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "annotateddefinition( " + attributes + ", " + definition + " )";
    }
}

/**
 * AssignmentExpressionNode
 */

final class AssignmentExpressionNode extends Node implements Tokens {

    Node lhs, rhs;
    int  op;
	/*Reference*/Value ref;

    AssignmentExpressionNode( Node lhs, int op, Node rhs ) {
        this.lhs = lhs;
        this.op = op;
        this.rhs = rhs;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public static boolean isFieldName(Node node) {
        return node instanceof LiteralStringNode ||
               node instanceof LiteralNumberNode ||
               node instanceof IdentifierNode ||
               node instanceof ParenthesizedExpressionNode;
    }

    public String toString() {
        return "assignmentexpression( " + Token.getTokenClassName( op )  + ", " + lhs + ", " + rhs + ")";
    }
}

/**
 * AttributeListNode
 */

final class AttributeListNode extends Node {

    Node item, list;

    AttributeListNode( Node item, Node list ) {
        this.item = item;
        this.list = list;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "attributelist( " + item + ", " + list + " )";
    }
}

/**
 * BinaryExpressionNode
 */

final class BinaryExpressionNode extends Node {

    protected Node lhs, rhs;
    protected int  op;
    Value     lhs_ref,rhs_ref;
	Slot      lhs_slot,rhs_slot;

    BinaryExpressionNode( int op, Node lhs, Node rhs ) {
        this.op  = op;
        this.lhs = lhs;
        this.rhs = rhs;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "binaryexpression( " + Token.getTokenClassName( op ) + ", " + lhs + ", " + rhs + " )";
    }
}

/**
 * BreakStatementNode
 */

final class BreakStatementNode extends Node {

    Node expr;

    public BreakStatementNode(Node expr) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "breakstatement( " + expr + " )";
    }
}

/**
 * CallExpressionNode
 */

final class CallExpressionNode extends Node {

    protected Node member, args;

    CallExpressionNode( Node member, Node args ) {
        this.member = member;
        this.args   = args;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "callexpression( " + member + ", " + args + " )";
    }
}

/**
 * CaseLabelNode
 */

final class CaseLabelNode extends Node {

    Node label;
    
    CaseLabelNode( Node label ) {
        this.label = label;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "caselabel( " + label + " )";
    }
}

/**
 * CatchClauseNode
 */

final class CatchClauseNode extends Node {

    Node parameter,statements;

    CatchClauseNode(Node parameter, Node statements) {
        this.parameter = parameter;
        this.statements = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "catchclause( " + parameter + ", " + statements + " )";
    }
}

/**
 * ClassDeclarationNode
 */

final class ClassDeclarationNode extends Node {

    Node name;

    ClassDeclarationNode( Node name ) {
        this.name = name;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "classdeclaration( " + name + " )";
    }
}

/**
 * ClassDefinitionNode
 */

final class ClassDefinitionNode extends Node {

    Node name, statements;
    InheritanceNode inheritance;
    Slot slot;

    static ClassDefinitionNode ClassDefinition( Node name, Node inheritance, Node statements ) {
        return new ClassDefinitionNode(name,inheritance,statements);
    }

    ClassDefinitionNode( Node name, Node inheritance, Node statements ) {
        this.name        = name;
        this.inheritance = (InheritanceNode)inheritance;
        this.statements  = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public Node first() {
        return statements.first();
    }

    public Node last() {
        return statements.last();
    }

    public String toString() {
        return "classdefinition( " + name + ", " + inheritance + ", " + statements + " )";
    }
}

/**
 * ClassofExpressionNode
 */

final class ClassofExpressionNode extends Node {

    Node base;

    ClassofExpressionNode( Node base ) {
        this.base = base;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "classofexpression( " + base + " )";
    }
}

/**
 * CoersionExpressionNode
 */

final class CoersionExpressionNode extends Node {

    Node expr, type;
    String typename;

    CoersionExpressionNode( Node expr, Node type, int pos ) {
	    super(pos);
        this.expr = expr;
        this.type = type;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "coersionexpression( " + expr + ", " + type + " )";
    }
}

/**
 * ConditionalExpressionNode
 */

final class ConditionalExpressionNode extends Node {

  protected Node condition, thenexpr, elseexpr;

  ConditionalExpressionNode( Node condition, Node thenexpr, Node elseexpr ) {
    this.condition = condition;
    this.thenexpr  = thenexpr;
    this.elseexpr  = elseexpr;
  }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

  public String toString() {
    return "conditionalexpression( " + condition + ", " + thenexpr + ", " + elseexpr + " )";
  }
}

/**
 * ContinueStatementNode
 */

final class ContinueStatementNode extends Node {

    Node expr;

    ContinueStatementNode(Node expr) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "continuestatement( " + expr + " )";
    }
}

/**
 * DoStatementNode
 */

final class DoStatementNode extends Node {

    Node statements,expr;

    DoStatementNode(Node statements, Node expr) {
        this.statements = statements;
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "dostatement( " + statements + ", " + expr + " )";
    }
}

/**
 * ElementListNode
 */

final class ElementListNode extends Node {

    Node list, item;

    ElementListNode( Node list, Node item ) {
        this.list = list;
        this.item = item;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "elementlist( " + list + ", " + item + " )";
    }
}

/**
 * EmptyStatementNode
 */

final class EmptyStatementNode extends Node {

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "emptystatement";
    }
}

/**
 * ExpressionStatementNode
 */

final class ExpressionStatementNode extends Node {

    Node expr;

    ExpressionStatementNode( Node expr ) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "expressionstatement( " + expr + " )";
    }
}

/**
 * ExportBindingNode
 */

final class ExportBindingNode extends Node {
  
    Node name;
    Node value;

    ExportBindingNode( Node name, Node value ) {
        this.name  = name;
	    this.value = value;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "exportbinding( " + name + ", " + value + " )";
    }
}

/**
 * ExportDefinitionNode
 */

final class ExportDefinitionNode extends Node {
  
    Node list;

    ExportDefinitionNode( Node list ) {
	    this.list = list;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "exportdefinition( " + list + " )";
    }
}

/**
 * FieldListNode
 */

final class FieldListNode extends Node {

    Node list, field;

    FieldListNode( Node list, Node field ) {
        this.list = list;
        this.field     = field;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "fieldlist( " + list + ", " + field + " )";
    }
}

/**
 * FinallyClauseNode
 */

final class FinallyClauseNode extends Node {

    Node statements;
    
    FinallyClauseNode(Node statements) {
        this.statements = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "finallyclause( " + statements + " )";
    }
}

/**
 * ForStatementNode
 */

final class ForStatementNode extends Node {

    protected Node initialize, test, increment, statement;

    ForStatementNode( Node initialize, Node test, Node increment, Node statement ) {

        if( initialize == null ) {
            this.initialize = NodeFactory.LiteralUndefined();
        } else {
            this.initialize = initialize;
        }

        if( test == null ) {
            this.test = NodeFactory.LiteralBoolean(true);
        } else {
            this.test = test;
        }

        if( increment == null ) {
            this.increment = NodeFactory.LiteralUndefined();
        } else {
            this.increment = increment;
        }

        this.statement = statement;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public Node[] getTargets() {
        Node[] targets = new Node[1];
        targets[0] = statement;
        return targets;
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "forstatement( " + initialize + ", " + test + ", " + increment + ", " + statement + " )";
    }
}

/**
 * ForInStatementNode
 */

final class ForInStatementNode extends Node {

    Node property, object, statement;

    ForInStatementNode( Node property, Node object, Node statement ) {
        this.property  = property;
        this.object    = object;
        this.statement = statement;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public Node[] getTargets() {
        Node[] targets = new Node[1];
        targets[0] = statement;
        return targets;
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "forinstatement( " + property + ", " + object + ", " + statement + " )";
    }
}

/**
 * FunctionExpressionNode
 */

final class FunctionExpressionNode extends Node {

    Node name, signature, body;

    FunctionExpressionNode( Node name, Node signature, Node body ) {
        this.name      = name;
        this.signature = signature;
        this.body      = body;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "functionexpression( " + name + ", " + signature + ", " + body + " )";
    }
}

/**
 * ReturnStatementNode
 */

final class ReturnStatementNode extends Node {

    Node expr;

    ReturnStatementNode( Node expr ) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return super.toString()+"returnstatement( " + expr + " )";
    }
}

/**
 * FunctionDeclarationNode
 */

final class FunctionDeclarationNode extends Node {

    Node name, signature;
    Slot slot;
	/*Reference*/Value ref;

    FunctionDeclarationNode( Node name, Node signature ) {
        this.name      = name;
        this.signature = signature;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "functiondeclaration( " + name + ", " + signature + " )";
    }
}

/**
 * FunctionDefinitionNode
 */

final class FunctionDefinitionNode extends Node {

    Node decl, body;
	int fixedCount;

    FunctionDefinitionNode( Node decl, Node body ) {
        this.decl = decl;
        this.body = body;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public Node first() {
        return body.first();
    }

    public Node last() {
        return body.last();
    }

    public String toString() {
        return "functiondefinition( " + decl + ", " + body + " )";
    }
}

/**
 * FunctionNameNode
 */

final class FunctionNameNode extends Node {

    Node name;
    int  kind;

    FunctionNameNode( int kind, Node name ) {
	    this.kind = kind;
        this.name = (IdentifierNode)name;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "functionname( " + Token.getTokenClassName(kind) + ", " + name + " )";
    }
}

/**
 * FunctionSignatureNode
 */

final class FunctionSignatureNode extends Node {

    Node parameter, result;
    Slot slot;
    Value ref;

    FunctionSignatureNode( Node parameter, Node result ) {
        this.parameter = parameter;
        this.result    = result;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "functionsignature( " + parameter + ", " + result + " )";
    }
}

/**
 * IdentifierNode
 */

class IdentifierNode extends Node {

    String name;
	/*Reference*/Value ref;

    IdentifierNode( String name, int pos ) {
        super(pos);
        this.name = name;
    }

    IdentifierNode( int token_class, int pos ) {
        super(pos);
        this.name = Token.getTokenClassName(token_class);
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "identifier( " + name + " )";
    }
}

/**
 * IfStatementNode
 */

final class IfStatementNode extends Node {

    protected Node condition, thenactions, elseactions;

    IfStatementNode( Node condition, Node thenactions, Node elseactions ) {
        this.condition   = condition;
        this.thenactions = thenactions;
        this.elseactions = elseactions;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return super.toString()+"ifstatement( " + condition + ", " + thenactions + ", " + elseactions + " )";
    }

    public boolean isBranch() {
        return true;
    }

    public Node[] getTargets() {
        Node[] targets = elseactions==null?new Node[1]:new Node[2];
        targets[0] = thenactions.first();
        if( elseactions != null ) {
            targets[1] = elseactions.first();
        }
        return targets;
    }
}

/**
 * ImportBindingNode
 */

final class ImportBindingNode extends Node {

    Node identifier;
    Node item;

    ImportBindingNode(Node identifier, Node item) {
        this.identifier = identifier;
        this.item = item;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "importbinding( " + identifier + ", " + item + " )";
    }
}

/**
 * ImportDefinitionNode
 */

final class ImportDefinitionNode extends Node {

    Node item,list;

    ImportDefinitionNode(Node item, Node list) {
        this.item = item;
        this.list = list;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "importdefinition( " + item + ", " + list + " )";
    }
}

/**
 * IncludeStatementNode
 */

final class IncludeStatementNode extends Node {

    Node item;

    IncludeStatementNode(Node item) {
        this.item = item;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "includestatement( " + item + " )";
    }
}

/**
 * IndexedMemberExpressionNode
 */

final class IndexedMemberExpressionNode extends Node {

    Node base, expr;

    IndexedMemberExpressionNode( Node base, Node expr, int pos ) {
	    super(pos);
        this.base = base;
	    this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "indexedmemberexpression( " + base + ", " + expr + " )";
    }
}

/**
 * InheritanceNode
 */

final class InheritanceNode extends Node {

    Node baseclass,interfaces;

    InheritanceNode( Node baseclass, Node interfaces ) {
        this.baseclass  = baseclass;
        this.interfaces = interfaces;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "inheritance( " + baseclass + ", " + interfaces + " )";
    }
}

/**
 * InterfaceDeclarationNode
 */

final class InterfaceDeclarationNode extends Node {

    Node name;

    InterfaceDeclarationNode( Node name ) {
        this.name = name;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "interfacedeclaration( " + name + " )";
    }
}

/**
 * InterfaceDefinitionNode
 */

final class InterfaceDefinitionNode extends Node {

    Node name, interfaces, statements;
    Slot slot;

    InterfaceDefinitionNode( Node name, Node interfaces, Node statements ) {
        this.name       = name;
        this.interfaces = interfaces;
        this.statements = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "interfacedefinition( " + name + ", " + interfaces + ", " + statements + " )";
    }
}

/**
 * LabeledStatementNode
 */

final class LabeledStatementNode extends Node {

    Node label;
    Node statement;

    LabeledStatementNode(Node label, Node statement) {
        this.label     = label;
        this.statement = statement;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public Node[] getTargets() {
        Node[] targets = new Node[1];
        targets[0] = this;
        return targets;
    }

    public String toString() {
        return "labeledstatement( " + label + ", "+ statement + " )";
    }
}

/**
 * LanguageDeclarationNode
 */

final class LanguageDeclarationNode extends Node {

    Node list;

    LanguageDeclarationNode(Node list) {
        this.list = list;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "languagedeclaration( " + list + " )";
    }
}

/**
 * ListNode
 */

final class ListNode extends Node {

    Node list, item;

    ListNode( Node list, Node item, int pos ) {
        super(pos);
        this.list = list;
        this.item = item;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    boolean isReference() {
        return true;
    }

    public int pos() {
        return item.pos();
    }

    public String toString() {
        return "list( " + list + ", " + item + " )";
    }
}

/**
 * LiteralArrayNode
 */

final class LiteralArrayNode extends Node {

    Node elementlist;
	/*List*/Value value;

    LiteralArrayNode( Node elementlist ) {
        this.elementlist = elementlist;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literalarray( " + elementlist + " )";
    }
}

/**
 * LiteralBooleanNode
 */

final class LiteralBooleanNode extends Node {

    protected boolean value;

    LiteralBooleanNode( boolean value ) {
        this.value = value;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literalboolean( " + new Boolean( value ) + " )";
    }
}

/**
 * LiteralFieldNode
 */

final class LiteralFieldNode extends Node {

    Node name;
    Node value;
	/*Reference*/Value ref;

    LiteralFieldNode( Node name, Node value ) {
        this.name  = name;
        this.value = value;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literalfield( " + name + ", " + value + " )";
    }
}

/**
 * LiteralNullNode
 */

final class LiteralNullNode extends Node {

  LiteralNullNode() {
  }

  public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
    return evaluator.evaluate( context, this );
  }

  public String toString() {
    return "literalnull";
  }
}

/**
 * LiteralNumberNode
 */

final class LiteralNumberNode extends Node {

    String value;

    LiteralNumberNode( String value ) {
        this.value = value;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literalnumber( " + value + " )";
    }
}

/**
 * LiteralObjectNode
 */

final class LiteralObjectNode extends Node {

    Node fieldlist;
	/*Object*/Value value;

    LiteralObjectNode( Node fieldlist ) {
        this.fieldlist = fieldlist;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literalobject( " + fieldlist + " )";
    }
}

/**
 * LiteralRegExpNode
 */

final class LiteralRegExpNode extends Node {

    String value;

    LiteralRegExpNode( String value ) {
        this.value = value;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literalregexp( " + value + " )";
    }
}

/**
 * LiteralStringNode
 */

final class LiteralStringNode extends Node {

    String value;

    LiteralStringNode( String value ) {
        this.value = value;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literalstring( " + value + " )";
    }
}

/**
 * LiteralTypeNode
 */

final class LiteralTypeNode extends Node {

    Type type;

    LiteralTypeNode( Type type ) {
        this.type = type;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "literaltype( " + type + " )";
    }
}

/**
 * LiteralUndefinedNode
 */

final class LiteralUndefinedNode extends Node {

    LiteralUndefinedNode() {
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
      return evaluator.evaluate( context, this );
    }

    public String toString() {
      return "literalundefined";
    }
}

/**
 * MemberExpressionNode
 */

final class MemberExpressionNode extends Node {

    Node base, name;
	/*Reference*/Value ref;

    MemberExpressionNode( Node base, Node name, int pos ) {
	    super(pos);
        this.base = base;
	    this.name = name;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "memberexpression( " + base + ", " + name + " )";
    }
}

/**
 * NamedParameterNode
 */

final class NamedParameterNode extends Node {
  
    Node name;
    Node parameter;

    NamedParameterNode( Node name, Node parameter ) {
        this.name      = name;
	    this.parameter = parameter;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "namedparameter( " + name + ", " + parameter + " )";
    }
}

/**
 * NamespaceDefinitionNode
 */

final class NamespaceDefinitionNode extends Node {

    Node name, extendslist;

    NamespaceDefinitionNode( Node name, Node extendslist ) {
        this.name        = name;
        this.extendslist = extendslist;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "namespacedefinition( " + name + ", " + extendslist + " )";
    }
}

/**
 * NewExpressionNode
 */

final class NewExpressionNode extends Node {

    Node expr;

    NewExpressionNode( Node expr ) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public static boolean isNew(Node node) {
        return node instanceof NewExpressionNode;
    }

    public String toString() {
        return "newexpression( " + expr + " )";
    }
}

/**
 * PackageDefinitionNode
 */

final class PackageDefinitionNode extends Node {

    Node name, statements;

    PackageDefinitionNode( Node name, Node statements ) {
        this.name  = name;
        this.statements = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "packagedefinition( " + name + ", " + statements + " )";
    }
}

/**
 * OptionalParameterNode
 */

final class OptionalParameterNode extends Node {
  
    Node parameter;
    Node initializer;

    OptionalParameterNode( Node parameter, Node initializer ) {
        this.parameter  = parameter;
	    this.initializer = initializer;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "optionalparameter( " + parameter + ", " + initializer + " )";
    }
}

/**
 * ParameterNode
 */

final class ParameterNode extends Node {
  
    Node identifier;
    Node type;
    Slot slot;
    String name;

    ParameterNode( Node identifier, Node type ) {
        this.identifier  = identifier;
	    this.type        = type;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "parameter( " + identifier + ", " + type + " )";
    }
}

/**
 * ParenthesizedExpressionNode
 */

final class ParenthesizedExpressionNode extends Node {

    Node expr;

    public ParenthesizedExpressionNode( Node expr, int pos ) {
	    super(pos);
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isListExpression() {
        return expr instanceof ListNode;
    }

    public String toString() {
        return "parenthesizedexpression( " + expr + " )";
    }
}

/**
 * ParenthesizedListExpressionNode
 */

final class ParenthesizedListExpressionNode extends Node {

    Node expr;

    public ParenthesizedListExpressionNode( Node expr ) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isListExpression() {
        return expr instanceof ListNode;
    }

    public String toString() {
        return "parenthesizedlistexpression( " + expr + " )";
    }
}

/**
 * PostfixExpressionNode
 */

final class PostfixExpressionNode extends Node {

    int  op;
    Node expr;

    public PostfixExpressionNode( int op, Node expr ) {
        this.op   = op;
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "postfixexpression( " + Token.getTokenClassName(op) + ", " + expr + " )";
    }
}

/**
 * ProgramNode
 */

final class ProgramNode extends Node {

    Node statements;

    ProgramNode(Node statements) {
        this.statements = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "program( " + statements + " )";
    }
}

/**
 * QualifiedIdentifierNode
 *
 * QualifiedIdentifier : QualifiedIdentifier Identifier
 */

final class QualifiedIdentifierNode extends IdentifierNode {

    Node qualifier; 

    QualifiedIdentifierNode( Node qualifier, IdentifierNode identifier, int pos ) {
        super(identifier.name,pos);
        this.qualifier = qualifier;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "qualifiedidentifier( " + qualifier + ", " + name + " )";
    }
}

/**
 * RestParameterNode
 */

final class RestParameterNode extends Node {
  
    Node parameter;

    RestParameterNode( Node parameter ) {
        this.parameter = parameter;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "restparameter( " + parameter + " )";
    }
}

/**
 * StatementListNode
 */

final class StatementListNode extends Node {

    StatementListNode list;
    Node item;

    StatementListNode( StatementListNode list, Node item ) {
        this.list = list;
	    this.item = item;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public Node first() {
        StatementListNode node = this;
        while(node.list!=null) {
            node = node.list;
        }
        return node.item;
    }

    public Node last() {
        return this.item;
    }

    public String toString() {
        return "statementlist( " + list + ", " + item + " )";
    }
}

/**
 * SwitchStatementNode
 */

final class SwitchStatementNode extends Node {

    Node expr;
    StatementListNode statements;
    
    SwitchStatementNode( Node expr, StatementListNode statements ) {
        this.expr = expr;
        this.statements = statements;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public Node[] getTargets() {

        StatementListNode node;
        
        int count = 0;
        node = statements;
        Debugger.trace("statements = " + statements);
        
        do {
        Debugger.trace("node = " + node.item);
            if( node.item instanceof CaseLabelNode ) {
                count++;
            }
            node = node.list;
        } while(node!=null);
    
        Node[] targets = new Node[count];
        node = statements;
        count = 0;

        do {
            if( node.item instanceof CaseLabelNode ) {
                targets[count++] = node.item;
            }
            node = node.list;
        } while(node!=null);

        return targets;
    }

    public String toString() {
        return "switchstatement( " + expr + ", " + statements + " )";
    }
}

/**
 * SuperExpressionNode
 */


final class SuperExpressionNode extends Node {

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "superexpression";
    }
}

/**
 * ThisExpressionNode
 */


final class ThisExpressionNode extends Node {

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "thisexpression";
    }
}

/**
 * ThrowStatementNode
 */

final class ThrowStatementNode extends Node {

    Node expr;
    
    ThrowStatementNode(Node expr) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "throwstatement( " + expr + " )";
    }
}

/**
 * TryStatementNode
 */

final class TryStatementNode extends Node {

    Node tryblock;
    StatementListNode catchlist;
    Node finallyblock;

    TryStatementNode(Node tryblock, StatementListNode catchlist, Node finallyblock) {
        this.tryblock     = tryblock;
        this.catchlist    = catchlist;
        this.finallyblock = finallyblock;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public boolean isBranch() {
        return true;
    }

    public Node[] getTargets() {

        StatementListNode node;
        
        int count = 0;
        node = catchlist;
        
        do {
            if( node.item instanceof CatchClauseNode ) {
                count++;
            }
            node = node.list;
        } while(node!=null);

        if( finallyblock != null ) {
            count++;
        }
    
        Node[] targets = new Node[count];
        node = catchlist;
        count = 0;

        do {
            if( node.item instanceof CatchClauseNode ) {
                targets[count++] = node.item;
            }
            node = node.list;
        } while(node!=null);

        if( finallyblock != null ) {
            targets[count++] = finallyblock;
        }

        return targets;
    }

    public String toString() {
        return "trystatement( " + tryblock + ", " + catchlist + ", " + finallyblock + " )";
    }
}

/**
 * TypedVariableNode
 */

final class TypedVariableNode extends Node {
  
    IdentifierNode identifier;
    IdentifierNode type;
    Slot slot;

    TypedVariableNode( Node identifier, Node type, int pos ) {
        super(pos);
        this.identifier  = (IdentifierNode)identifier;
	    this.type        = (IdentifierNode)type;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "typedvariable( " + identifier + ", " + type + " )";
    }
}

/**
 * UnaryExpressionNode
 */

final class UnaryExpressionNode extends Node {

    Node expr;
    int  op;

    UnaryExpressionNode( int op, Node expr ) {
	    this.op   = op;
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "unaryexpression( " + Token.getTokenClassName(op) + ", " + expr + " )";
    }
}

/**
 * UnitExpressionNode
 */

final class UnitExpressionNode extends Node {

    Node value, type;

    UnitExpressionNode( Node value, Node type ) {
        this.value = value;
        this.type  = type;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "unitexpression( " + value + ", " + type + " )";
    }
}

/**
 * UseStatementNode
 */

final class UseStatementNode extends Node {

    Node expr;

    UseStatementNode( Node expr ) {
        this.expr = expr;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "usestatement( " + expr + " )";
    }
}

/**
 * VariableBindingNode
 */

final class VariableBindingNode extends Node {
  
    TypedVariableNode variable;
    Node initializer;

    VariableBindingNode( Node variable, Node initializer ) {
        this.variable  = (TypedVariableNode)variable;
	    this.initializer = initializer;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public int pos() {
        return variable.pos();
    }

    public String toString() {
        return "variablebinding( " + variable + ", " + initializer + " )";
    }
}

/**
 * VariableDefinitionNode
 */

final class VariableDefinitionNode extends Node {
  
    int  kind;
    Node list;

    VariableDefinitionNode( int kind, Node list, int pos ) {
        super(pos);
        this.kind = kind;
	    this.list = list;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "variabledefinition( " + Token.getTokenClassName(kind) + ", " + list + " )";
    }
}

/**
 * WhileStatementNode
 */

final class WhileStatementNode extends Node {

    Node expr, statement;

    WhileStatementNode( Node expr, Node statement ) {
	    this.expr      = expr;
	    this.statement = statement;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public Node[] getTargets() {
        Node[] targets = new Node[1];
        targets[0] = statement;
        return targets;
    }

    public boolean isBranch() {
        return true;
    }

    public String toString() {
        return "whilestatement( " + expr + ", " + statement + " ) ";
    }
}

/**
 * WithStatementNode
 */

final class WithStatementNode extends Node {

    Node expr;
    Node statement;

    WithStatementNode( Node expr, Node statement ) {
	    this.expr      = expr;
	    this.statement = statement;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return evaluator.evaluate( context, this );
    }

    public String toString() {
        return "withstatement( " + expr + ", " + statement + " )";
    }
}

/*
 * The end.
 */
