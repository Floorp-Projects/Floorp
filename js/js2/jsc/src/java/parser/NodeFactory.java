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
 * Creates parse tree nodes.
 */

package com.compilercompany.ecmascript;

public final class NodeFactory {

    private static final boolean debug = false;

    static private InputBuffer in;

    static void init(InputBuffer input) {
        in = input;
    }

    static IdentifierNode Identifier( String name ) {
        if( debug ) {
            Debugger.trace("" + in.positionOfMark() + ": Identifier " + name );
        }
        return new IdentifierNode(name,in.positionOfMark());
    }
    static QualifiedIdentifierNode QualifiedIdentifier( Node qualifier, IdentifierNode identifier ) {
        if( debug ) {
            Debugger.trace("" + in.positionOfMark() + ": QualifiedIdentifier " + identifier);
        }
        return new QualifiedIdentifierNode(qualifier,identifier,identifier.pos());
    }
    static LiteralNullNode LiteralNull() {
        return new LiteralNullNode();
    }
    static LiteralBooleanNode LiteralBoolean(boolean value) {
        return new LiteralBooleanNode(value);
    }
    static LiteralArrayNode LiteralArray( Node elementlist ) {
        return new LiteralArrayNode(elementlist);
    }
    static LiteralFieldNode LiteralField( Node name, Node value ) {
        return new LiteralFieldNode(name,value);
    }
    static LiteralNumberNode LiteralNumber( String value ) {
        return new LiteralNumberNode(value);
    }
    static LiteralObjectNode LiteralObject( Node fieldlist ) {
        return new LiteralObjectNode(fieldlist);
    }
    static LiteralRegExpNode LiteralRegExp( String value ) {
        return new LiteralRegExpNode(value);
    }
    static LiteralStringNode LiteralString( String value ) {
        return new LiteralStringNode(value);
    }
    static LiteralTypeNode LiteralType( Type type ) {
        return new LiteralTypeNode(type);
    }
    static LiteralUndefinedNode LiteralUndefined() {
        return new LiteralUndefinedNode();
    }
    static ParenthesizedExpressionNode ParenthesizedExpression( Node expr ) {
        return new ParenthesizedExpressionNode(expr,in.positionOfMark());
    }
    static ParenthesizedListExpressionNode ParenthesizedListExpression( Node expr ) {
        return new ParenthesizedListExpressionNode(expr);
    }
    static FunctionExpressionNode FunctionExpression( Node name, Node signature, Node body ) {
        return new FunctionExpressionNode(name,signature,body);
    }
    static AnnotatedDefinitionNode AnnotatedDefinition( Node attributes, Node definition ) {
        return new AnnotatedDefinitionNode(attributes,definition);
    }
    static AttributeListNode AttributeList( Node item, Node list ) {
        return new AttributeListNode(item,list);
    }
    static UnitExpressionNode UnitExpression( Node value, Node type ) {
        return new UnitExpressionNode(value,type);
    }
    static ThisExpressionNode ThisExpression() {
        return new ThisExpressionNode();
    }
    static SuperExpressionNode SuperExpression() {
        return new SuperExpressionNode();
    }
    static ListNode List( Node list, Node item ) {
        return new ListNode(list,item,item.pos());
    }
    static PostfixExpressionNode PostfixExpression( int op, Node expr ) {
        return new PostfixExpressionNode(op,expr);
    }
    static NewExpressionNode NewExpression( Node member ) {
        return new NewExpressionNode(member);
    }
    static ClassofExpressionNode ClassofExpression( Node base ) {
        if( debug ) {
            Debugger.trace("base = " + base);
        }
        return new ClassofExpressionNode(base);
    }
    static CallExpressionNode CallExpression( Node member, Node args ) {
        return new CallExpressionNode(member,args);
    }
    static IndexedMemberExpressionNode IndexedMemberExpression( Node base, Node member ) {
        return new IndexedMemberExpressionNode(base,member,in.positionOfMark());
    }
    static MemberExpressionNode MemberExpression( Node base, Node name ) {
        return new MemberExpressionNode(base,name,in.positionOfMark());
    }
    static CoersionExpressionNode CoersionExpression( Node expr, Node type ) {
        return new CoersionExpressionNode(expr,type,in.positionOfMark());
    }
    static UnaryExpressionNode UnaryExpression( int op, Node expr ) {
        return new UnaryExpressionNode(op,expr);
    }
    static BinaryExpressionNode BinaryExpression( int op, Node lhs, Node rhs ) {
        return new BinaryExpressionNode(op,lhs,rhs);
    }
    static ConditionalExpressionNode ConditionalExpression( Node cond, Node thenexpr, Node elseexpr ) {
        return new ConditionalExpressionNode(cond,thenexpr,elseexpr);
    }
    static AssignmentExpressionNode AssignmentExpression( Node lhs, int op, Node rhs ) {
        return new AssignmentExpressionNode(lhs,op,rhs);
    }
    static StatementListNode StatementList( StatementListNode list, Node item ) {
        return new StatementListNode(list,item);
    }

    static EmptyStatementNode EmptyStatement() {
        return new EmptyStatementNode();
    }
    static ExpressionStatementNode ExpressionStatement( Node expr ) {
        return new ExpressionStatementNode(expr);
    }
    static AnnotatedBlockNode AnnotatedBlock( Node attributes, Node definition ) {
        return new AnnotatedBlockNode(attributes,definition);
    }
    static LabeledStatementNode LabeledStatement( Node label, Node statement ) {
        return new LabeledStatementNode(label,statement);
    }
    static IfStatementNode IfStatement( Node test, Node tblock, Node eblock ) {
        return new IfStatementNode(test,tblock,eblock);
    }
    static SwitchStatementNode SwitchStatement( Node expr, StatementListNode statements ) {
        return new SwitchStatementNode(expr,statements);
    }
    static CaseLabelNode CaseLabel( Node label ) {
        return new CaseLabelNode(label);
    }
    static DoStatementNode DoStatement( Node block, Node expr ) {
        return new DoStatementNode(block,expr);
    }
    static WhileStatementNode WhileStatement( Node expr, Node statement ) {
        return new WhileStatementNode(expr,statement);
    }
    static ForInStatementNode ForInStatement( Node expr1, Node expr2, Node statement ) {
        return new ForInStatementNode(expr1,expr2,statement);
    }
    static ForStatementNode ForStatement( Node expr1, Node expr2, Node expr3, Node statement ) {
        return new ForStatementNode(expr1,expr2,expr3,statement);
    }
    static WithStatementNode WithStatement( Node expr, Node statement ) {
        return new WithStatementNode(expr,statement);
    }
    static ContinueStatementNode ContinueStatement(Node expr) {
        return new ContinueStatementNode(expr);
    }
    static BreakStatementNode BreakStatement(Node expr) {
        return new BreakStatementNode(expr);
    }
    static ReturnStatementNode ReturnStatement( Node expr ) {
        return new ReturnStatementNode(expr);
    }
    static ThrowStatementNode ThrowStatement(Node list) {
        return new ThrowStatementNode(list);
    }
    static TryStatementNode TryStatement(Node tryblock, StatementListNode catchlist, Node finallyblock) {
        return new TryStatementNode(tryblock,catchlist,finallyblock);
    }
    static CatchClauseNode CatchClause(Node parameter, Node block) {
        return new CatchClauseNode(parameter,block);
    }
    static FinallyClauseNode FinallyClause( Node block ) {
        return new FinallyClauseNode(block);
    }
    static IncludeStatementNode IncludeStatement( Node list ) {
        return new IncludeStatementNode(list);
    }
    static UseStatementNode UseStatement( Node expr ) {
        return new UseStatementNode(expr);
    }

    static ImportDefinitionNode ImportDefinition( Node item, Node list ) {
        return new ImportDefinitionNode(item,list);
    }
    static ImportBindingNode ImportBinding( Node identifer, Node item ) {
        return new ImportBindingNode(identifer,item);
    }
    static ExportDefinitionNode ExportDefinition( Node list ) {
        return new ExportDefinitionNode(list);
    }
    static ExportBindingNode ExportBinding( Node name, Node value ) {
        return new ExportBindingNode(name,value);
    }
    static VariableDefinitionNode VariableDefinition( int kind, Node list ) {
        return new VariableDefinitionNode(kind,list,list.pos());
    }
    static VariableBindingNode VariableBinding( Node identifier, Node initializer ) {
        return new VariableBindingNode(identifier,initializer);
    }
    static TypedVariableNode TypedVariable( Node identifier, Node type ) {
        if( debug ) {
            Debugger.trace("" + in.positionOfMark() + ": TypedVariable " + type );
        }
        return new TypedVariableNode(identifier,type,type!=null?type.pos():identifier.pos());
    }
    static FunctionDefinitionNode FunctionDefinition( Node decl, Node body ) {
        return new FunctionDefinitionNode(decl,body);
    }
    static FunctionDeclarationNode FunctionDeclaration( Node name, Node signature ) {
        return new FunctionDeclarationNode(name,signature);
    }
    static FunctionNameNode FunctionName( int kind, Node name ) {
        return new FunctionNameNode(kind,name);
    }
    static RestParameterNode RestParameter( Node expr ) {
        return new RestParameterNode(expr);
    }
    static ParameterNode Parameter( Node identifer, Node type ) {
        return new ParameterNode(identifer,type);
    }
    static OptionalParameterNode OptionalParameter( Node identifer, Node init ) {
        return new OptionalParameterNode(identifer,init);
    }
    static NamedParameterNode NamedParameter( Node name, Node parameter ) {
        return new NamedParameterNode(name,parameter);
    }
    static ClassDeclarationNode ClassDeclaration( Node name ) {
        return new ClassDeclarationNode(name);
    }
    static ClassDefinitionNode ClassDefinition( Node name, Node interfaces, Node statements ) {
        return new ClassDefinitionNode(name,interfaces,statements);
    }
    static InheritanceNode Inheritance( Node baseclass, Node interfaces ) {
        return new InheritanceNode(baseclass,interfaces);
    }
    static InterfaceDeclarationNode InterfaceDeclaration( Node name ) {
        return new InterfaceDeclarationNode(name);
    }
    static InterfaceDefinitionNode InterfaceDefinition( Node name, Node interfaces, Node statements ) {
        return new InterfaceDefinitionNode(name,interfaces,statements);
    }
    static NamespaceDefinitionNode NamespaceDefinition( Node identifier, Node list ) {
        return new NamespaceDefinitionNode(identifier,list);
    }
    static LanguageDeclarationNode LanguageDeclaration( Node list ) {
        return new LanguageDeclarationNode(list);
    }
    static PackageDefinitionNode PackageDefinition( Node name, Node block ) {
        return new PackageDefinitionNode(name,block);
    }
    static ProgramNode Program( Node statements ) {
        return new ProgramNode(statements);
    }

    /*
    static Node () {
        return new Node();
    }
    */

}

/*
 * The end.
 */
