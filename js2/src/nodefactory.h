/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nodefactory_h___
#define nodefactory_h___

#include "utilities.h"
#include <vector>

namespace JavaScript {

    class NodeFactory {

    private:
        NodeFactory(Arena& arena) : arena(arena) {}

        static NodeFactory *state;
        Arena &arena;

    public:
        static void
        Init(Arena& arena) {
            state = new NodeFactory(arena);
        }
        static Arena &
        getArena() {
            return NodeFactory::state->arena;
        }
        static IdentifierExprNode *
        Identifier(const Token &t) {
            return new(getArena()) IdentifierExprNode(t);
        }
#if 0
        static QualifiedIdentifierNode
        QualifiedIdentifier(Node qualifier, IdentifierNode identifier ) {
            return new QualifiedIdentifierNode(qualifier,identifier);
        }
        static LiteralNullNode
        LiteralNull() {
            return new LiteralNullNode();
        }
        static LiteralBooleanNode
        LiteralBoolean(boolean value) {
            return new LiteralBooleanNode(value);
        }
        static LiteralArrayNode
        LiteralArray(Node elementlist) {
            return new LiteralArrayNode(elementlist);
        }
#endif
        static ExprPairList *
        LiteralField(ExprNode *name = 0, ExprNode *value = 0) {
            return new(getArena()) ExprPairList(name,value);
        }
        static NumberExprNode *
        LiteralNumber(const Token &t) {
            return new(getArena()) NumberExprNode(t);
        }
#if 0
        static LiteralObjectNode
        LiteralObject(Node fieldlist) {
            return new LiteralObjectNode(fieldlist);
        }
        static LiteralRegExpNode
        LiteralRegExp(String value) {
            return new LiteralRegExpNode(value);
        }
#endif
        static StringExprNode *
        LiteralString(uint32 pos, ExprNode::Kind kind, String &str) {
            return new(getArena()) StringExprNode(pos,kind,str);
        }
#if 0
        static LiteralTypeNode
        LiteralType(Type type) {
            return new LiteralTypeNode(type);
        }
        static LiteralUndefinedNode
        LiteralUndefined() {
            return new LiteralUndefinedNode();
        }
        static FunctionExpressionNode
        FunctionExpression(Node name, Node signature, Node body ) {
            return new FunctionExpressionNode(name,signature,body);
        }
        static AnnotatedDefinitionNode
        AnnotatedDefinition(Node attributes, Node definition) {
            return new AnnotatedDefinitionNode(attributes,definition);
        }
        static AttributeDefinitionNode
        AttributeDefinition(Node name, int kind, Node value) {
            return new AttributeDefinitionNode(name,kind,value);
        }
        static AttributeListNode
        AttributeList(Node item, Node list) {
            return new AttributeListNode(item,list);
        }
        static UnitExpressionNode
        UnitExpression(Node value, Node type) {
            return new UnitExpressionNode(value,type);
        }
        static ThisExpressionNode
        ThisExpression() {
            return new ThisExpressionNode();
        }
        static SuperExpressionNode
        SuperExpression() {
            return new SuperExpressionNode();
        }
        static EvalExpressionNode
        EvalExpression(Node expr) {
            return new EvalExpressionNode(expr);
        }
        static ListNode
        List(Node list, Node item) {
            return new ListNode(list,item);
        }
        static PostfixExpressionNode
        PostfixExpression(int op, Node expr) {
            return new PostfixExpressionNode(op,expr);
        }
        static NewExpressionNode
        NewExpression(Node member) {
            return new NewExpressionNode(member);
        }
        static CallExpressionNode
        CallExpression(Node member, Node args) {
            return new CallExpressionNode(member,args);
        }
        static IndexedMemberExpressionNode
        IndexedMemberExpression(Node base, Node member) {
            return new IndexedMemberExpressionNode(base,member);
        }
        static MemberExpressionNode
        MemberExpression(Node base, Node name) {
            return new MemberExpressionNode(base,name);
        }
        static CoersionExpressionNode
        CoersionExpression(Node expr, Node type) {
            return new CoersionExpressionNode(expr,type);
        }
        static UnaryExpressionNode
        UnaryExpression(int op, Node expr) {
            return new UnaryExpressionNode(op,expr);
        }
        static BinaryExpressionNode
        BinaryExpression(int op, Node lhs, Node rhs) {
            return new BinaryExpressionNode(op,lhs,rhs);
        }
        static ConditionalExpressionNode
        ConditionalExpression(Node cond, Node thenexpr, Node elseexpr) {
            return new ConditionalExpressionNode(cond,thenexpr,elseexpr);
        }
        static AssignmentExpressionNode
        AssignmentExpression(Node lhs, int op, Node rhs) {
            return new AssignmentExpressionNode(lhs,op,rhs);
        }
        static StatementListNode
        StatementList (StatementListNode list, Node item ) {
            return new StatementListNode(list,item);
        }
        static EmptyStatementNode
        EmptyStatement() {
            return new EmptyStatementNode();
        }
        static ExpressionStatementNode
        ExpressionStatement(Node expr) {
            return new ExpressionStatementNode(expr);
        }
        static AnnotatedBlockNode
        AnnotatedBlock(Node attributes, Node definition) {
            return new AnnotatedBlockNode(attributes,definition);
        }
        static LabeledStatementNode
        LabeledStatement(Node label, Node statement) {
            return new LabeledStatementNode(label,statement);
        }
        static IfStatementNode
        IfStatement(Node test, Node tblock, Node eblock) {
            return new IfStatementNode(test,tblock,eblock);
        }
        static SwitchStatementNode
        SwitchStatement(Node expr, Node statements) {
            return new SwitchStatementNode(expr,statements);
        }
        static DefaultStatementNode
        DefaultStatement() {
            return new DefaultStatementNode();
        }
        static DoStatementNode
        DoStatement(Node block, Node expr) {
            return new DoStatementNode(block,expr);
        }
        static WhileStatementNode
        WhileStatement(Node expr, Node statement) {
            return new WhileStatementNode(expr,statement);
        }
        static ForInStatementNode
        ForInStatement(Node expr1, Node expr2, Node statement) {
            return new ForInStatementNode(expr1,expr2,statement);
        }
        static ForStatementNode
        ForStatement(Node expr1, Node expr2, Node expr3, Node statement) {
            return new ForStatementNode(expr1,expr2,expr3,statement);
        }
        static WithStatementNode
        WithStatement(Node expr, Node statement) {
            return new WithStatementNode(expr,statement);
        }
        static ContinueStatementNode
        ContinueStatement(Node expr) {
            return new ContinueStatementNode(expr);
        }
        static BreakStatementNode
        BreakStatement(Node expr) {
            return new BreakStatementNode(expr);
        }
        static ReturnStatementNode
        ReturnStatement(Node expr) {
            return new ReturnStatementNode(expr);
        }
        static ThrowStatementNode
        ThrowStatement(Node list) {
            return new ThrowStatementNode(list);
        }
        static TryStatementNode
        TryStatement(Node tryblock, Node catchlist, Node finallyblock) {
            return new TryStatementNode(tryblock,catchlist,finallyblock);
        }
        static CatchClauseNode
        CatchClause(Node parameter, Node block) {
            return new CatchClauseNode(parameter,block);
        }
        static FinallyClauseNode
        FinallyClause(Node block) {
            return new FinallyClauseNode(block);
        }
        static IncludeStatementNode
        IncludeStatement(Node list) {
            return new IncludeStatementNode(list);
        }
        static UseStatementNode
        UseStatement(Node expr) {
            return new UseStatementNode(expr);
        }
        static ImportDefinitionNode
        ImportDefinition(Node item, Node list) {
            return new ImportDefinitionNode(item,list);
        }
        static ImportBindingNode
        ImportBinding(Node identifer, Node item) {
            return new ImportBindingNode(identifer,item);
        }
        static ExportDefinitionNode
        ExportDefinition(Node list) {
            return new ExportDefinitionNode(list);
        }
        static ExportBindingNode
        ExportBinding(Node name, Node value) {
            return new ExportBindingNode(name,value);
        }
        static VariableDefinitionNode
        VariableDefinition(int kind, Node list) {
            return new VariableDefinitionNode(kind,list);
        }
        static VariableBindingNode
        VariableBinding(Node identifier, Node initializer) {
            return new VariableBindingNode(identifier,initializer);
        }
        static TypedVariableNode
        TypedVariable(Node identifier, Node type) {
            return new TypedVariableNode(identifier,type);
        }
        static FunctionDefinitionNode
        FunctionDefinition(Node decl, Node body) {
            return new FunctionDefinitionNode(decl,body);
        }
        static FunctionDeclarationNode
        FunctionDeclaration(Node name, Node signature) {
            return new FunctionDeclarationNode(name,signature);
        }
        static FunctionNameNode
        FunctionName(int kind, Node name) {
            return new FunctionNameNode(kind,name);
        }
        static RestParameterNode
        RestParameter(Node expr) {
            return new RestParameterNode(expr);
        }
        static ParameterNode
        Parameter(Node identifer, Node type) {
            return new ParameterNode(identifer,type);
        }
#endif
        static VariableBinding *
        Parameter(ExprNode *name, ExprNode *type, bool constant) {
            return new(getArena()) VariableBinding(0, name, type, 0, constant);
        }
#if 0
        static VariableBinding *
        NamedParameter(StringExprNode *name, ParseNode *parameter) {
            return new(Arena()) NamedParameterNode(name, parameter);
        }
        static ClassDeclarationNode
        ClassDeclaration(Node name) {
            return new ClassDeclarationNode(name);
        }
        static ClassDefinitionNode
        ClassDefinition(Node name, Node interfaces, Node statements) {
            return new ClassDefinitionNode(name,interfaces,statements);
        }
        static InheritanceNode
        Inheritance(Node baseclass, Node interfaces) {
        return new InheritanceNode(baseclass,interfaces);
        }
        static InterfaceDeclarationNode
        InterfaceDeclaration(Node name) {
            return new InterfaceDeclarationNode(name);
        }
        static InterfaceDefinitionNode
        InterfaceDefinition(Node name, Node interfaces, Node statements) {
            return new InterfaceDefinitionNode(name,interfaces,statements);
        }
        static NamespaceDefinitionNode
        NamespaceDefinition(Node identifier, Node list) {
            return new NamespaceDefinitionNode(identifier,list);
        }
        static LanguageDeclarationNode
        LanguageDeclaration(Node list) {
            return new LanguageDeclarationNode(list);
        }
        static PackageDefinitionNode
        PackageDefinition(Node name, Node block) {
            return new PackageDefinitionNode(name,block);
        }
        static ProgramNode
        Program(Node statements) {
            return new ProgramNode(statements);
        }
#endif
    };
}
#endif // nodefactory_h
