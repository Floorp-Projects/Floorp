/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include "numerics.h"
#include "parser.h"
#include "nodefactory.h"

//
// Parser
//

namespace JavaScript
{
    NodeFactory *NodeFactory::state;

// Create a new Parser for parsing the provided source code, interning
// identifiers, keywords, and regular expressions in the designated world,
// and allocating the parse tree in the designated arena.
    Parser::Parser(World &world, Arena &arena, const String &source,
                       const String &sourceLocation, uint32 initialLineNum):
            lexer(world, source, sourceLocation, initialLineNum),
            arena(arena), lineBreaksSignificant(true)
    {
        NodeFactory::Init(arena);
    }


// Report a syntax error at the backUp-th last token read by the Lexer.
// In other words, if backUp is 0, the error is at the next token to be read
// by the Lexer (which must have been peeked already); if backUp is 1, the
// error is at the last token read by the Lexer, and so forth.
    void
    Parser::syntaxError(const char *message, uint backUp)
    {
        syntaxError(widenCString(message), backUp);
    }

// Same as above, but the error message is already a String.
    void
    Parser::syntaxError(const String &message, uint backUp)
    {
        while (backUp--)
            lexer.unget();
        getReader().error(Exception::syntaxError, message, lexer.getPos());
    }


// Get the next token using the given preferRegExp setting.  If that token's
// kind matches the given kind, consume that token and return it.  Otherwise
// throw a syntax error.
    const Token &
    Parser::require(bool preferRegExp, Token::Kind kind)
    {
        const Token &t = lexer.get(preferRegExp);
        if (!t.hasKind(kind)) {
            String message;
            bool special = Token::isSpecialKind(kind);
            
            if (special)
                message += '\'';
            message += Token::kindName(kind);
            if (special)
                message += '\'';
            message += " expected";
            syntaxError(message);
        }
        return t;
    }

// Copy the Token's chars into the current arena and return the resulting copy.
    inline String &
    Parser::copyTokenChars(const Token &t)
    {
        return newArenaString(arena, t.getChars());
    }

// If t is an Identifier, return a new IdentifierExprNode corresponding to t.
// Otherwise, return null.
    ExprNode *
    Parser::makeIdentifierExpression(const Token &t) const
    {
        if (t.hasIdentifierKind())
            return new(arena) IdentifierExprNode(t);
        return 0;
    }

// An identifier or parenthesized expression has just been parsed into e.
// If it is followed by one or more ::'s followed by identifiers, construct
// the appropriate qualify parse node and return it and set foundQualifiers
// to true.  If no :: is found, return e and set foundQualifiers to false.
// After parseIdentifierQualifiers finishes, the next token might have been
// peeked with the given preferRegExp setting.
    ExprNode *
    Parser::parseIdentifierQualifiers(ExprNode *e, bool &foundQualifiers,
                                      bool preferRegExp)
    {
        const Token *tDoubleColon = lexer.eat(preferRegExp,
                                              Token::doubleColon);
        if (!tDoubleColon) {
            foundQualifiers = false;
            return e;
        }

        foundQualifiers = true;
        checkStackSize();
        uint32 pos = tDoubleColon->getPos();
        return new(arena) BinaryExprNode(pos, ExprNode::qualify, e,
                                         parseQualifiedIdentifier(lexer.get(true),
                                                                  preferRegExp));
    }

// An opening parenthesis has just been parsed into tParen.  Finish parsing a
// ParenthesizedExpression.
// If it is followed by one or more ::'s followed by identifiers, construct
// the appropriate qualify parse node and return it and set foundQualifiers to
// true.  If no :: is found, return the ParenthesizedExpression and set
// foundQualifiers to false. After parseParenthesesAndIdentifierQualifiers
// finishes, the next token might have been peeked with the given preferRegExp
// setting.
    ExprNode *
    Parser::parseParenthesesAndIdentifierQualifiers(const Token &tParen,
                                                    bool &foundQualifiers,
                                                    bool preferRegExp)
    {
        uint32 pos = tParen.getPos();
        ExprNode *e = new(arena) UnaryExprNode(pos, ExprNode::parentheses,
                                               parseExpression(false));
        require(false, Token::closeParenthesis);
        return parseIdentifierQualifiers(e, foundQualifiers, preferRegExp);
    }

// Parse and return a qualifiedIdentifier.  The first token has already been
// parsed and is in t. If the second token was peeked, it should be have been
// done with the given preferRegExp setting. After parseQualifiedIdentifier
// finishes, the next token might have been peeked with the given
// preferRegExp setting.
    ExprNode *
    Parser::parseQualifiedIdentifier(const Token &t, bool preferRegExp)
    {
        bool foundQualifiers;
        ExprNode *e = makeIdentifierExpression(t);
        if (e)
            return parseIdentifierQualifiers(e, foundQualifiers, preferRegExp);
        if (t.hasKind(Token::openParenthesis)) {
            e = parseParenthesesAndIdentifierQualifiers(t, foundQualifiers,
                                                        preferRegExp);
            goto checkQualifiers;
        }
        if (t.hasKind(Token::Super)) {
            e = parseIdentifierQualifiers(new(arena) ExprNode(t.getPos(),
                                                              ExprNode::Super),
                                          foundQualifiers, preferRegExp);
            goto checkQualifiers;
        }
        if (t.hasKind(Token::Public) || t.hasKind(Token::Package) ||
            t.hasKind(Token::Private)) {
            e = parseIdentifierQualifiers(new(arena) IdentifierExprNode(t),
                                          foundQualifiers, preferRegExp);
          checkQualifiers:
            if (!foundQualifiers)
                syntaxError("'::' expected", 0);
            return e;
        }
        syntaxError("Identifier or '(' expected");
        return 0;   // Unreachable code here just to shut up compiler warnings
    }

// Parse and return an arrayLiteral. The opening bracket has already been
// read into initialToken.
    PairListExprNode *
    Parser::parseArrayLiteral(const Token &initialToken)
    {
        uint32 initialPos = initialToken.getPos();
        NodeQueue<ExprPairList> elements;
        
        while (true) {
            ExprNode *element = 0;
            const Token &t = lexer.peek(true);
            if (t.hasKind(Token::comma) || t.hasKind(Token::closeBracket))
                lexer.redesignate(false);   // Safe: neither ','
                                            // nor '}' starts with a slash.
            else
                element = parseAssignmentExpression(false);
            elements += new(arena) ExprPairList(0, element);

            const Token &tSeparator = lexer.get(false);
            if (tSeparator.hasKind(Token::closeBracket))
                break;
            if (!tSeparator.hasKind(Token::comma))
                syntaxError("',' expected");
        }
        return new(arena) PairListExprNode(initialPos, ExprNode::arrayLiteral,
                                           elements.first);
    }


// Parse and return an objectLiteral. The opening brace has already been
// read into initialToken.
    PairListExprNode *
    Parser::parseObjectLiteral(const Token &initialToken)
    {
        uint32 initialPos = initialToken.getPos();
        NodeQueue<ExprPairList> elements;
        
        if (!lexer.eat(true, Token::closeBrace))
            while (true) {
                const Token &t = lexer.get(true);
                ExprNode *field;
                if (t.hasIdentifierKind() ||
                    t.hasKind(Token::openParenthesis) ||
                    t.hasKind(Token::Super) || t.hasKind(Token::Public) ||
                    t.hasKind(Token::Package) || t.hasKind(Token::Private))
                    field = parseQualifiedIdentifier(t, false);
                else if (t.hasKind(Token::string)) {
                    field = NodeFactory::LiteralString(t.getPos(),
                                                       ExprNode::string,
                                                       copyTokenChars(t));
                }
                else if (t.hasKind(Token::number))
                    field = new(arena) NumberExprNode(t);
                else {
                    syntaxError("Field name expected");
                    // Unreachable code here just to shut up compiler warnings
                    field = 0;
                }
                require(false, Token::colon);
                elements += NodeFactory::LiteralField(field,
                                                      parseAssignmentExpression(false));

                const Token &tSeparator = lexer.get(false);
                if (tSeparator.hasKind(Token::closeBrace))
                    break;
                if (!tSeparator.hasKind(Token::comma))
                    syntaxError("',' expected");
            }
        return new(arena) PairListExprNode(initialPos,
                                           ExprNode::objectLiteral,
                                           elements.first);
}

// Parse and return a PrimaryExpression.
// If the first token was peeked, it should be have been done with
// preferRegExp set to true. After parsePrimaryExpression finishes, the next
// token might have been peeked with preferRegExp set to false.
    ExprNode *
    Parser::parsePrimaryExpression()
    {
        ExprNode *e;
        ExprNode::Kind eKind;
        
        const Token &t = lexer.get(true);
        switch (t.getKind()) {
            case Token::Null:
                eKind = ExprNode::Null;
                goto makeExprNode;
                
            case Token::True:
                eKind = ExprNode::True;
                goto makeExprNode;
                
            case Token::False:
                eKind = ExprNode::False;
                goto makeExprNode;
                
            case Token::This:
                eKind = ExprNode::This;
                goto makeExprNode;
                
            case Token::Super:
                eKind = ExprNode::Super;
                if (lexer.peek(false).hasKind(Token::doubleColon))
                    goto makeQualifiedIdentifierNode;
          makeExprNode:
                e = new(arena) ExprNode(t.getPos(), eKind);
                break;
                
            case Token::Public:
                if (lexer.peek(false).hasKind(Token::doubleColon))
                    goto makeQualifiedIdentifierNode;
                e = new(arena) IdentifierExprNode(t);
                break;
                
            case Token::number:
            {
                const Token &tUnit = lexer.peek(false);
                if (!lineBreakBefore(tUnit) &&
                    (tUnit.hasKind(Token::unit) ||
                     tUnit.hasKind(Token::string))) {
                    lexer.get(false);
                    e = new(arena) NumUnitExprNode(t.getPos(),
                                                   ExprNode::numUnit,
                                                   copyTokenChars(t),
                                                   t.getValue(),
                                                   copyTokenChars(tUnit));
                } else
                    e = new(arena) NumberExprNode(t);
            }
            break;
            
            case Token::string:
                e = NodeFactory::LiteralString(t.getPos(), ExprNode::string,
                                               copyTokenChars(t));
                break;
                
            case Token::regExp:
                e = new(arena) RegExpExprNode(t.getPos(), ExprNode::regExp,
                                              t.getIdentifier(),
                                              copyTokenChars(t));
                break;
                
            case Token::Package:
            case Token::Private:
            case CASE_TOKEN_NONRESERVED:
          makeQualifiedIdentifierNode:
          e = parseQualifiedIdentifier(t, false);
          break;
    
            case Token::openParenthesis:
            {
                bool foundQualifiers;
                e = parseParenthesesAndIdentifierQualifiers(t, foundQualifiers,
                                                            false);
                if (!foundQualifiers) {
                    const Token &tUnit = lexer.peek(false);
                    if (!lineBreakBefore(tUnit) &&
                        tUnit.hasKind(Token::string)) {
                        lexer.get(false);
                        e = new(arena) ExprUnitExprNode(t.getPos(),
                                                        ExprNode::exprUnit, e,
                                                        copyTokenChars(tUnit));
                    }
                }
            }
            break;
    
            case Token::openBracket:
                e = parseArrayLiteral(t);
                break;
                
            case Token::openBrace:
                e = parseObjectLiteral(t);
                break;
                
            case Token::Function:
            {
                FunctionExprNode *f = new(arena) FunctionExprNode(t.getPos());
                const Token &t2 = lexer.get(true);
                f->function.prefix = FunctionName::normal;
                if (!(f->function.name = makeIdentifierExpression(t2)))
                    lexer.unget();
                parseFunctionSignature(f->function);
                f->function.body = parseBody(0);
                e = f;
            }
            break;
            
            default:
                syntaxError("Expression expected");
                // Unreachable code here just to shut up compiler warnings
                e = 0;
        }
        
        return e;
    }
    
    
// Parse a . or @ followed by a QualifiedIdentifier or ParenthesizedExpression
// and return the resulting BinaryExprNode.  Use kind if a QualifiedIdentifier
// was found or parenKind if a ParenthesizedExpression was found.
// tOperator is the . or @ token.  target is the first operand.
    BinaryExprNode *
    Parser::parseMember(ExprNode *target, const Token &tOperator,
                        ExprNode::Kind kind, ExprNode::Kind parenKind)
    {
        uint32 pos = tOperator.getPos();
        ExprNode *member;
        const Token &t2 = lexer.get(true);
        if (t2.hasKind(Token::openParenthesis)) {
            bool foundQualifiers;
            member = parseParenthesesAndIdentifierQualifiers(t2,
                                                             foundQualifiers,
                                                             false);
            if (!foundQualifiers)
                kind = parenKind;
        } else
            member = parseQualifiedIdentifier(t2, false);
        return new(arena) BinaryExprNode(pos, kind, target, member);
    }
    

// Parse an ArgumentsList followed by a closing parenthesis or bracket and
// return the resulting InvokeExprNode.  The target function, indexed object,
// or created class is supplied.  The opening parenthesis or bracket has
// already been read.
// pos is the position to use for the InvokeExprNode.
    InvokeExprNode *
    Parser::parseInvoke(ExprNode *target, uint32 pos,
                        Token::Kind closingTokenKind,
                        ExprNode::Kind invokeKind)
    {
        NodeQueue<ExprPairList> arguments;
        
#ifdef NEW_PARSER
        parseArgumentList(arguments);
        match(closingTokenKind);
#else
        bool hasNamedArgument = false;

        if (!lexer.eat(true, closingTokenKind))
            while (true) {
                ExprNode *field = 0;
                ExprNode *value = parseAssignmentExpression(false);
                if (lexer.eat(false, Token::colon)) {
                    field = value;
                    if (!ExprNode::isFieldKind(field->getKind()))
                        syntaxError("Argument name must be an identifier, "
                                    "string, or number");
                    hasNamedArgument = true;
                    value = parseAssignmentExpression(false);
                } else if (hasNamedArgument)
                    syntaxError("Unnamed argument cannot follow named "
                                "argument", 0);
                arguments += new(arena) ExprPairList(field, value);
                
                const Token &tSeparator = lexer.get(false);
                if (tSeparator.hasKind(closingTokenKind))
                    break;
                if (!tSeparator.hasKind(Token::comma))
                    syntaxError("',' expected");
            }
#endif
        return new(arena) InvokeExprNode(pos, invokeKind, target,
                                         arguments.first);
    }

// Parse and return a PostfixExpression.
// If newExpression is true, this expression is immediately preceded by 'new',
// so don't allow call, postincrement, or postdecrement operators on it.
// If the first token was peeked, it should be have been done with
// preferRegExp set to true. After parsePostfixExpression finishes, the next
// token might have been peeked with preferRegExp set to false.
    ExprNode *
    Parser::parsePostfixExpression(bool newExpression)
    {
        ExprNode *e;
        
        const Token *tNew = lexer.eat(true, Token::New);
        if (tNew) {
            checkStackSize();
            uint32 posNew = tNew->getPos();
            e = parsePostfixExpression(true);
            if (lexer.eat(false, Token::openParenthesis))
                e = parseInvoke(e, posNew, Token::closeParenthesis,
                                ExprNode::New);
            else
                e = new(arena) InvokeExprNode(posNew, ExprNode::New, e, 0);
        } else
            e = parsePrimaryExpression();
        
        while (true) {
            ExprNode::Kind eKind;
            const Token &t = lexer.get(false);
            switch (t.getKind()) {
                case Token::openParenthesis:
                    if (newExpression)
                        goto other;
                    e = parseInvoke(e, t.getPos(), Token::closeParenthesis,
                                    ExprNode::call);
                    break;
                    
                case Token::openBracket:
                    e = parseInvoke(e, t.getPos(), Token::closeBracket,
                                    ExprNode::index);
                    break;
                    
                case Token::dot:
                    if (lexer.eat(true, Token::Class)) {
                        e = new(arena) UnaryExprNode(t.getPos(),
                                                     ExprNode::dotClass, e);
                    } else
                        e = parseMember(e, t, ExprNode::dot,
                                        ExprNode::dotParen);
                    break;
                    
                case Token::at:
                    e = parseMember(e, t, ExprNode::at, ExprNode::at);
                    break;
                    
                case Token::increment:
                    eKind = ExprNode::postIncrement;
              incDec:
                    if (newExpression || lineBreakBefore(t))
                        goto other;
                    e = new(arena) UnaryExprNode(t.getPos(), eKind, e);
                    break;
                    
                case Token::decrement:
                    eKind = ExprNode::postDecrement;
                    goto incDec;
                    
                default:
              other:
                    lexer.unget();
                    return e;
            }
        }
    }
    
// Ensure that e is a postfix expression.  If not, throw a syntax error on
// the current token.
    void
    Parser::ensurePostfix(const ExprNode *e)
    {
        ASSERT(e);
        if (!e->isPostfix())
            syntaxError("Only a postfix expression can be used as the result "
                        "of an assignment; enclose this expression in "
                        "parentheses", 0);
    }

// Parse and return a UnaryExpression.
// If the first token was peeked, it should be have been done with
// preferRegExp set to true.
// After parseUnaryExpression finishes, the next token might have been peeked
// with preferRegExp set to false.
    ExprNode *
    Parser::parseUnaryExpression()
    {
        ExprNode::Kind eKind;
        ExprNode *e;

        const Token &t = lexer.peek(true);
        uint32 pos = t.getPos();
        switch (t.getKind()) {
            case Token::Delete:
                eKind = ExprNode::Delete;
                goto getPostfixExpression;
                
            case Token::increment:
                eKind = ExprNode::preIncrement;
                goto getPostfixExpression;
                
            case Token::decrement:
                eKind = ExprNode::preDecrement;
          getPostfixExpression:
                lexer.get(true);
                e = parsePostfixExpression();
                break;
                
            case Token::Typeof:
                eKind = ExprNode::Typeof;
                goto getUnaryExpression;
                
            case Token::Eval:
                eKind = ExprNode::Eval;
                goto getUnaryExpression;

            case Token::plus:
                eKind = ExprNode::plus;
                goto getUnaryExpression;
                
            case Token::minus:
                eKind = ExprNode::minus;
                goto getUnaryExpression;
                
            case Token::complement:
                eKind = ExprNode::complement;
                goto getUnaryExpression;
                
            case Token::logicalNot:
                eKind = ExprNode::logicalNot;
          getUnaryExpression:
                lexer.get(true);
                checkStackSize();
                e = parseUnaryExpression();
                break;
                
            default:
                return parsePostfixExpression();
        }
        return new(arena) UnaryExprNode(pos, eKind, e);
    }
    
    
    const Parser::BinaryOperatorInfo
    Parser::tokenBinaryOperatorInfos[Token::kindsEnd] = {
        
        // Special

        {ExprNode::none, pExpression, pNone}, // end
        {ExprNode::none, pExpression, pNone}, // Token::number
        {ExprNode::none, pExpression, pNone}, // Token::string
        {ExprNode::none, pExpression, pNone}, // Token::unit
        {ExprNode::none, pExpression, pNone}, // Token::regExp
        
        // Punctuators
            
        {ExprNode::none, pExpression, pNone}, // Token::openParenthesis
        {ExprNode::none, pExpression, pNone}, // Token::closeParenthesis
        {ExprNode::none, pExpression, pNone}, // Token::openBracket
        {ExprNode::none, pExpression, pNone}, // Token::closeBracket
        {ExprNode::none, pExpression, pNone}, // Token::openBrace
        {ExprNode::none, pExpression, pNone}, // Token::closeBrace
        {ExprNode::comma, pExpression, pExpression}, // Token::comma
        {ExprNode::none, pExpression, pNone}, // Token::semicolon
        {ExprNode::none, pExpression, pNone}, // Token::dot
        {ExprNode::none, pExpression, pNone}, // Token::doubleDot
        {ExprNode::none, pExpression, pNone}, // Token::tripleDot
        {ExprNode::none, pExpression, pNone}, // Token::arrow
        {ExprNode::none, pExpression, pNone}, // Token::colon
        {ExprNode::none, pExpression, pNone}, // Token::doubleColon
        {ExprNode::none, pExpression, pNone}, // Token::pound
        {ExprNode::none, pExpression, pNone}, // Token::at
        {ExprNode::none, pExpression, pNone}, // Token::increment
        {ExprNode::none, pExpression, pNone}, // Token::decrement
        {ExprNode::none, pExpression, pNone}, // Token::complement
        {ExprNode::none, pExpression, pNone}, // Token::logicalNot
        {ExprNode::multiply, pMultiplicative, pMultiplicative}, // Token::times
        {ExprNode::divide, pMultiplicative, pMultiplicative}, // Token::divide
        {ExprNode::modulo, pMultiplicative, pMultiplicative}, // Token::modulo
        {ExprNode::add, pAdditive, pAdditive}, // Token::plus
        {ExprNode::subtract, pAdditive, pAdditive}, // Token::minus
        {ExprNode::leftShift, pShift, pShift}, // Token::leftShift
        {ExprNode::rightShift, pShift, pShift}, // Token::rightShift
        {ExprNode::logicalRightShift, pShift, pShift}, // Token::logicalRightShift
        {ExprNode::logicalAnd, pBitwiseOr, pLogicalAnd}, // Token::logicalAnd (right-associative for efficiency)
        {ExprNode::logicalXor, pLogicalAnd, pLogicalXor}, // Token::logicalXor (right-associative for efficiency)
        {ExprNode::logicalOr, pLogicalXor, pLogicalOr}, // Token::logicalOr (right-associative for efficiency)
        {ExprNode::bitwiseAnd, pBitwiseAnd, pBitwiseAnd}, // Token::bitwiseAnd
        {ExprNode::bitwiseXor, pBitwiseXor, pBitwiseXor}, // Token::bitwiseXor
        {ExprNode::bitwiseOr, pBitwiseOr, pBitwiseOr}, // Token::bitwiseOr
        {ExprNode::assignment, pPostfix, pAssignment}, // Token::assignment
        {ExprNode::multiplyEquals, pPostfix, pAssignment}, // Token::timesEquals
        {ExprNode::divideEquals, pPostfix, pAssignment}, // Token::divideEquals
        {ExprNode::moduloEquals, pPostfix, pAssignment}, // Token::moduloEquals
        {ExprNode::addEquals, pPostfix, pAssignment}, // Token::plusEquals
        {ExprNode::subtractEquals, pPostfix, pAssignment}, // Token::minusEquals
        {ExprNode::leftShiftEquals, pPostfix, pAssignment}, // Token::leftShiftEquals
        {ExprNode::rightShiftEquals, pPostfix, pAssignment}, // Token::rightShiftEquals
        {ExprNode::logicalRightShiftEquals, pPostfix, pAssignment}, // Token::logicalRightShiftEquals
        {ExprNode::logicalAndEquals, pPostfix, pAssignment}, // Token::logicalAndEquals
        {ExprNode::logicalXorEquals, pPostfix, pAssignment}, // Token::logicalXorEquals
        {ExprNode::logicalOrEquals, pPostfix, pAssignment}, // Token::logicalOrEquals
        {ExprNode::bitwiseAndEquals, pPostfix, pAssignment}, // Token::bitwiseAndEquals
        {ExprNode::bitwiseXorEquals, pPostfix, pAssignment}, // Token::bitwiseXorEquals
        {ExprNode::bitwiseOrEquals, pPostfix, pAssignment}, // Token::bitwiseOrEquals
        {ExprNode::equal, pEquality, pEquality}, // Token::equal
        {ExprNode::notEqual, pEquality, pEquality}, // Token::notEqual
        {ExprNode::lessThan, pRelational, pRelational}, // Token::lessThan
        {ExprNode::lessThanOrEqual, pRelational, pRelational}, // Token::lessThanOrEqual
        {ExprNode::greaterThan, pRelational, pRelational}, // Token::greaterThan
        {ExprNode::greaterThanOrEqual, pRelational, pRelational}, // Token::greaterThanOrEqual
        {ExprNode::identical, pEquality, pEquality}, // Token::identical
        {ExprNode::notIdentical, pEquality, pEquality}, // Token::notIdentical
        {ExprNode::conditional, pLogicalOr, pConditional}, // Token::question
        
            // Reserved words
        {ExprNode::none, pExpression, pNone}, // Token::Abstract
        {ExprNode::none, pExpression, pNone}, // Token::Break
        {ExprNode::none, pExpression, pNone}, // Token::Case
        {ExprNode::none, pExpression, pNone}, // Token::Catch
        {ExprNode::none, pExpression, pNone}, // Token::Class
        {ExprNode::none, pExpression, pNone}, // Token::Const
        {ExprNode::none, pExpression, pNone}, // Token::Continue
        {ExprNode::none, pExpression, pNone}, // Token::Debugger
        {ExprNode::none, pExpression, pNone}, // Token::Default
        {ExprNode::none, pExpression, pNone}, // Token::Delete
        {ExprNode::none, pExpression, pNone}, // Token::Do
        {ExprNode::none, pExpression, pNone}, // Token::Else
        {ExprNode::none, pExpression, pNone}, // Token::Enum
        {ExprNode::none, pExpression, pNone}, // Token::Export
        {ExprNode::none, pExpression, pNone}, // Token::Extends
        {ExprNode::none, pExpression, pNone}, // Token::False
        {ExprNode::none, pExpression, pNone}, // Token::Final
        {ExprNode::none, pExpression, pNone}, // Token::Finally
        {ExprNode::none, pExpression, pNone}, // Token::For
        {ExprNode::none, pExpression, pNone}, // Token::Function
        {ExprNode::none, pExpression, pNone}, // Token::Goto
        {ExprNode::none, pExpression, pNone}, // Token::If
        {ExprNode::none, pExpression, pNone}, // Token::Implements
        {ExprNode::none, pExpression, pNone}, // Token::Import
        {ExprNode::In, pRelational, pRelational}, // Token::In
        {ExprNode::Instanceof, pRelational, pRelational}, // Token::Instanceof
        {ExprNode::none, pExpression, pNone}, // Token::Interface
        {ExprNode::none, pExpression, pNone}, // Token::Namespace
        {ExprNode::none, pExpression, pNone}, // Token::Native
        {ExprNode::none, pExpression, pNone}, // Token::New
        {ExprNode::none, pExpression, pNone}, // Token::Null
        {ExprNode::none, pExpression, pNone}, // Token::Package
        {ExprNode::none, pExpression, pNone}, // Token::Private
        {ExprNode::none, pExpression, pNone}, // Token::Protected
        {ExprNode::none, pExpression, pNone}, // Token::Public
        {ExprNode::none, pExpression, pNone}, // Token::Return
        {ExprNode::none, pExpression, pNone}, // Token::Static
        {ExprNode::none, pExpression, pNone}, // Token::Super
        {ExprNode::none, pExpression, pNone}, // Token::Switch
        {ExprNode::none, pExpression, pNone}, // Token::Synchronized
        {ExprNode::none, pExpression, pNone}, // Token::This
        {ExprNode::none, pExpression, pNone}, // Token::Throw
        {ExprNode::none, pExpression, pNone}, // Token::Throws
        {ExprNode::none, pExpression, pNone}, // Token::Transient
        {ExprNode::none, pExpression, pNone}, // Token::True
        {ExprNode::none, pExpression, pNone}, // Token::Try
        {ExprNode::none, pExpression, pNone}, // Token::Typeof
        {ExprNode::none, pExpression, pNone}, // Token::Use
        {ExprNode::none, pExpression, pNone}, // Token::Var
        {ExprNode::none, pExpression, pNone}, // Token::Void
        {ExprNode::none, pExpression, pNone}, // Token::Volatile
        {ExprNode::none, pExpression, pNone}, // Token::While
        {ExprNode::none, pExpression, pNone}, // Token::With
            
        // Non-reserved words
        {ExprNode::none, pExpression, pNone}, // Token::Eval
        {ExprNode::none, pExpression, pNone}, // Token::Exclude
        {ExprNode::none, pExpression, pNone}, // Token::Get
        {ExprNode::none, pExpression, pNone}, // Token::Include
        {ExprNode::none, pExpression, pNone}, // Token::Set

        {ExprNode::none, pExpression, pNone}, // Token::identifier

    };

    struct Parser::StackedSubexpression {
        ExprNode::Kind kind; // The kind of BinaryExprNode the subexpression
                             // should generate
        uchar precedence;    // Precedence of an operator with respect to
                             // operators on its right
        uint32 pos;          // The operator token's position
        ExprNode *op1;       // First operand of the operator
        ExprNode *op2;       // Second operand of the operator (used for ?:
                             // only)
};

// Parse and return an Expression.  If noIn is false, allow the in operator.
// If noAssignment is false, allow the = and op= operators.  If noComma is
// false, allow the comma operator. If the first token was peeked, it should
// be have been done with preferRegExp set to true.
// After parseExpression finishes, the next token might have been peeked with
// preferRegExp set to false.
    ExprNode *
    Parser::parseExpression(bool noIn, bool noAssignment, bool noComma)
    {
        ArrayBuffer<StackedSubexpression, 10> subexpressionStack;

        checkStackSize();
        // Push a limiter onto subexpressionStack.
        subexpressionStack.reserve_advance_back()->precedence = pNone;

        while (true) {
          foundColon:
            ExprNode *e = parseUnaryExpression();
            
            const Token &t = lexer.peek(false);
            const BinaryOperatorInfo &binOpInfo =
                tokenBinaryOperatorInfos[t.getKind()];
            Precedence precedence = binOpInfo.precedenceLeft;
            ExprNode::Kind kind = binOpInfo.kind;
            ASSERT(precedence > pNone);
            
            // Disqualify assignments, 'in', and comma if the flags
            // indicate that these should end the expression.
            if (precedence == pPostfix && noAssignment ||
                kind == ExprNode::In && noIn ||
                kind == ExprNode::comma && noComma) {
                kind = ExprNode::none;
                precedence = pExpression;
            }
            
            if (precedence == pPostfix)
                ensurePostfix(e);   // Ensure that the target of an assignment
                                    // is a postfix subexpression.
            else
                // Reduce already stacked operators with precedenceLeft or
                // higher precedence
                while (subexpressionStack.back().precedence >= precedence) {
                    StackedSubexpression &s = subexpressionStack.pop_back();
                    if (s.kind == ExprNode::conditional) {
                        if (s.op2)
                            e = new(arena) TernaryExprNode(s.pos, s.kind,
                                                           s.op1, s.op2, e);
                        else {
                            if (!t.hasKind(Token::colon))
                                syntaxError("':' expected", 0);
                            lexer.get(false);
                            subexpressionStack.advance_back();
                            s.op2 = e;
                            goto foundColon;
                        }
                    } else
                        e = new(arena) BinaryExprNode(s.pos, s.kind, s.op1, e);
                }
            
            if (kind == ExprNode::none) {
                ASSERT(subexpressionStack.size() == 1);
                return e;
            }
        
            // Push the current operator onto the subexpressionStack.
            lexer.get(false);
            StackedSubexpression &s =
                *subexpressionStack.reserve_advance_back();
            s.kind = kind;
            s.precedence = binOpInfo.precedenceRight;
            s.pos = t.getPos();
            s.op1 = e;
            s.op2 = 0;
        }
    }

// Parse an opening parenthesis, an Expression, and a closing parenthesis.
// Return the Expression. If the first token was peeked, it should be have
// been done with preferRegExp set to true.
    ExprNode *
    Parser::parseParenthesizedExpression()
    {
        require(true, Token::openParenthesis);
        ExprNode *e = parseExpression(false);
        require(false, Token::closeParenthesis);
        return e;
    }
    
    
// Parse and return a TypeExpression.  If noIn is false, allow the in operator.
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true. After parseTypeExpression finishes, the next
// token might have been peeked with preferRegExp set to true.
    ExprNode *
    Parser::parseTypeExpression(bool noIn)
    {
        ExprNode *type = parseNonAssignmentExpression(noIn);
        if (lexer.peek(false).hasKind(Token::divideEquals))
            syntaxError("'/=' not allowed here", 0);
        lexer.redesignate(true);    // Safe: a '/' would have been interpreted
                                    // as an operator, so it can't be the next
                                    // token;
                                    // a '/=' was outlawed by the check above.
        return type;
    }


// Parse a TypedIdentifier.  Return the identifier's name.
// If a type was provided, set type to it; otherwise, set type to nil.
// After parseTypedIdentifier finishes, the next token might have been peeked
// with preferRegExp set to false.
    const StringAtom &
    Parser::parseTypedIdentifier(ExprNode *&type)
    {
        const Token &t = lexer.get(true);
        if (!t.hasIdentifierKind())
            syntaxError("Identifier expected");
        const StringAtom &name = t.getIdentifier();

        type = 0;
        if (lexer.eat(false, Token::colon))
            type = parseNonAssignmentExpression(false);
        return name;
    }

// If the next token has the given kind, eat it and parse and return the
// following TypeExpression; otherwise return nil.
// If noIn is false, allow the in operator.
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true.
// After parseTypeBinding finishes, the next token might have been peeked
// with preferRegExp set to true.
    ExprNode *
    Parser::parseTypeBinding(Token::Kind kind, bool noIn)
    {
        ExprNode *type = 0;
        if (lexer.eat(true, kind))
            type = parseTypeExpression(noIn);
        return type;
    }

// If the next token has the given kind, eat it and parse and return the
// following TypeExpressionList; otherwise return nil.
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true.
// After parseTypeListBinding finishes, the next token might have been peeked
// with preferRegExp set to true.
    ExprList *
    Parser::parseTypeListBinding(Token::Kind kind)
    {
        NodeQueue<ExprList> types;
        if (lexer.eat(true, kind))
            do types += new(arena) ExprList(parseTypeExpression(false));
            while (lexer.eat(true, Token::comma));
        return types.first;
    }

// Parse and return a VariableBinding.
// If noQualifiers is false, allow a QualifiedIdentifier as the variable name;
// otherwise, restrict the variable name to be a simple Identifier.
// If noIn is false, allow the in operator.
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true.
// After parseVariableBinding finishes, the next token might have been peeked
// with preferRegExp set to true.
    VariableBinding *
    Parser::parseVariableBinding(bool noQualifiers, bool noIn, bool constant)
    {
        const Token &t = lexer.get(true);
        uint32 pos = t.getPos();

        ExprNode *name;
        if (noQualifiers) {
            name = makeIdentifierExpression(t);
            if (!name)
                syntaxError("Identifier expected");
        } else
            name = parseQualifiedIdentifier(t, true);
    
        ExprNode *type = parseTypeBinding(Token::colon, noIn);
        
        ExprNode *initializer = 0;
        if (lexer.eat(true, Token::assignment)) {
            initializer = parseAssignmentExpression(noIn);
            lexer.redesignate(true);  // Safe: a '/' or a '/=' would have
                                      // been interpreted as an operator, so
                                      // it can't be the next token.
        }
        
        return new(arena) VariableBinding(pos, name, type, initializer,
                                          constant);
    }

// Parse a FunctionName and initialize fn with the result.
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true.
// After parseFunctionName finishes, the next token might have been peeked
// with preferRegExp set to true.
    void
    Parser::parseFunctionName(FunctionName &fn)
    {
        fn.prefix = FunctionName::normal;
        const Token *t = &lexer.get(true);
        if (t->hasKind(Token::Get) || t->hasKind(Token::Set)) {
            const Token *t2 = &lexer.peek(true);
            if (!lineBreakBefore(*t2) && t2->getFlag(Token::canFollowGet)) {
                fn.prefix = t->hasKind(Token::Get) ? FunctionName::Get :
                    FunctionName::Set;
                t = &lexer.get(true);
            }
        }
        
        fn.name = parseQualifiedIdentifier(*t, true);
    }


// Parse a FunctionSignature and initialize fd with the result.
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true.
// After parseFunctionSignature finishes, the next token might have been
// peeked with preferRegExp set to true.
    void
    Parser::parseFunctionSignature(FunctionDefinition &fd)
    {
        require(true, Token::openParenthesis);
        
        NodeQueue<VariableBinding> parameters;
        VariableBinding *optParameters = 0;
        VariableBinding *namedParameters = 0;
        VariableBinding *restParameter = 0;
#ifdef NEW_PARSER
        fd.optParameters = optParameters;
        fd.namedParameters = namedParameters;
        fd.restParameter = restParameter;
#endif
        if (!lexer.eat(true, Token::closeParenthesis)) {
#ifdef NEW_PARSER
            parseAllParameters(fd,parameters);
            match(Token::closeParenthesis);
#else
            while (true) {
                if (lexer.eat(true, Token::tripleDot)) {
                    const Token &t1 = lexer.peek(true);
                    if (t1.hasKind(Token::closeParenthesis))
                        restParameter = new(arena) VariableBinding(t1.getPos(),
                                                                   0, 0, 0,
                                                                   false);
                    else
                        restParameter = parseVariableBinding(true, false,
                                                             lexer.eat(true,
                                                                       Token::Const));
                    if (!optParameters)
                        optParameters = restParameter;
                    parameters += restParameter;
                    require(true, Token::closeParenthesis);
                    break;
                } else {
                    VariableBinding *b = parseVariableBinding(true, false,
                                                              lexer.eat(true,
                                                                        Token::Const));
                    if (b->initializer) {
                        if (!optParameters)
                            optParameters = b;
                    } else
                        if (optParameters)
                            syntaxError("'=' expected", 0);
                    parameters += b;
                    const Token &t = lexer.get(true);
                    if (!t.hasKind(Token::comma))
                        if (t.hasKind(Token::closeParenthesis))
                            break;
                        else
                            syntaxError("',' or ')' expected");
                }
            }
#endif
        }
        fd.parameters = parameters.first;
#ifndef NEW_PARSER
        fd.optParameters = optParameters;
        fd.restParameter = restParameter;
        fd.resultType = parseTypeBinding(Token::colon, false);
#endif
        fd.resultType = parseResultSignature();
    }
    
// Parse a list of statements ending with a '}'.  Return these statements as a
// linked list threaded through the StmtNodes' next fields.  The opening '{'
// has already been read. If noCloseBrace is true, an end-of-input terminates
// the block; the end-of-input token is not read.
// If inSwitch is true, allow case <expr>: and default: statements.
// If noCloseBrace is true, after parseBlock finishes the next token might
// have been peeked with preferRegExp set to true.
    StmtNode *
    Parser::parseBlock(bool inSwitch, bool noCloseBrace)
    {
        NodeQueue<StmtNode> q;
        SemicolonState semicolonState = semiNone;
        
        while (true) {
            const Token *t = &lexer.peek(true);
            if (t->hasKind(Token::semicolon) && semicolonState != semiNone) {
                lexer.get(true);
                semicolonState = semiNone;
                t = &lexer.peek(true);
            }
            if (noCloseBrace) {
                if (t->hasKind(Token::end))
                    return q.first;
            } else if (t->hasKind(Token::closeBrace)) {
                lexer.get(true);
                return q.first;
            }
            if (!(semicolonState == semiNone ||
                  semicolonState == semiInsertable && lineBreakBefore(*t)))
                syntaxError("';' expected", 0);
            
            StmtNode *s = parseStatement(!inSwitch, inSwitch, semicolonState);
            if (inSwitch && !q.first && !s->hasKind(StmtNode::Case))
                syntaxError("First statement in a switch block must be "
                            "'case expr:' or 'default:'", 0);
            q += s;
        }
    }
    
    
// Parse an optional block of statements beginning with a '{' and ending
// with a '}'. Return these statements as a BlockStmtNode.
// If semicolonState is nil, the block is required; otherwise, the block is
// optional and if it is omitted, *semicolonState is set to semiInsertable.
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true. After parseBody finishes, the next token might
// have been peeked with preferRegExp set to true.
    BlockStmtNode *
    Parser::parseBody(SemicolonState *semicolonState)
    {
        const Token *tBrace = lexer.eat(true, Token::openBrace);
        if (tBrace) {
            uint32 pos = tBrace->getPos();
            return new(arena) BlockStmtNode(pos, StmtNode::block, 0,
                                            parseBlock(false, false));
        } else {
            if (!semicolonState)
                syntaxError("'{' expected", 0);
            *semicolonState = semiInsertable;
            return 0;
        }
    }
        
// Parse and return a statement that takes zero or more initial attributes,
// which have already been parsed. If noIn is false, allow the in operator.
//
// If the statement ends with an optional semicolon, that semicolon is not
// parsed. Instead, parseAttributeStatement returns in semicolonState one of
// three values:
//    semiNone:             No semicolon is needed to close the statement
//    semiNoninsertable:    A NoninsertableSemicolon is needed to close the
//                          statement; a line break is not enough
//    semiInsertable:       A Semicolon is needed to close the statement; a
//                          line break is also sufficient
//
// pos is the position of the beginning of the statement (its first attribute
// if it has attributes). The first token of the statement has already been
// read and is provided in t. After parseAttributeStatement finishes, the next
// token might have been peeked with preferRegExp set to true.
    StmtNode *
    Parser::parseAttributeStatement(uint32 pos, IdentifierList *attributes,
                                    const Token &t, bool noIn,
                                    SemicolonState &semicolonState)
    {
        semicolonState = semiNone;
        StmtNode::Kind sKind;
        
        switch (t.getKind()) {
            case Token::openBrace:
                return new(arena) BlockStmtNode(pos, StmtNode::block,
                                                attributes,
                                                parseBlock(false, false));
                
            case Token::Const:
                sKind = StmtNode::Const;
                goto constOrVar;
            case Token::Var:
                sKind = StmtNode::Var;
          constOrVar:
                {
                    NodeQueue<VariableBinding> bindings;
                    
                    do bindings += parseVariableBinding(false, noIn,
                                                        sKind == StmtNode::Const);
                    while (lexer.eat(true, Token::comma));
                    semicolonState = semiInsertable;
                    return new(arena) VariableStmtNode(pos, sKind,
                                                       attributes,
                                                       bindings.first);
                }
                
            case Token::Function:
                sKind = StmtNode::Function;
                {
                    FunctionStmtNode *f =
                        new(arena) FunctionStmtNode(pos, sKind, attributes);
                    parseFunctionName(f->function);
                    parseFunctionSignature(f->function);
                    f->function.body = parseBody(&semicolonState);
                    return f;
                }
                
            case Token::Interface:
                sKind = StmtNode::Interface;
                goto classOrInterface;
            case Token::Class:
                sKind = StmtNode::Class;
          classOrInterface:
                {
                    ExprNode *name = parseQualifiedIdentifier(lexer.get(true),
                                                              true);
                    ExprNode *superclass = 0;
                    if (sKind == StmtNode::Class)
                        superclass = parseTypeBinding(Token::Extends, false);
                    ExprList *superinterfaces =
                        parseTypeListBinding(sKind == StmtNode::Class ?
                                             Token::Implements :
                                             Token::Extends);
                    BlockStmtNode *body =
                        parseBody(superclass || superinterfaces ?
                                  0 : &semicolonState);
                    return new(arena) ClassStmtNode(pos, sKind, attributes,
                                                    name, superclass,
                                                    superinterfaces, body);
          }

            case Token::Namespace:
            {
                const Token &t2 = lexer.get(false);
                ExprNode *name;
                if (lineBreakBefore(t2) ||
                    !(name = makeIdentifierExpression(t2)))
                    syntaxError("Namespace name expected");
                ExprList *supernamespaces =
                    parseTypeListBinding(Token::Extends);
                semicolonState = semiInsertable;
                return new(arena) NamespaceStmtNode(pos, StmtNode::Namespace,
                                                    attributes, name,
                                                    supernamespaces);
            }

            default:
                syntaxError("Bad declaration");
                return 0;
        }
    }
    
    
// Parse and return a statement that takes zero or more initial attributes.
// semicolonState behaves as in parseAttributeStatement.
//
// as restricts the kinds of statements that are allowed after the attributes:
//   asAny      Any statements that takes attributes can follow
//   asBlock    Only a block can follow
//   asConstVar Only a const or var declaration can follow, and the 'in'
//              operator is not allowed at its top level
//
// The first token of the statement has already been read and is provided in t.
// If the second token was peeked, it should be have been done with
// preferRegExp set to false; the second token should have been peeked only
// if t is an attribute.
// After parseAttributesAndStatement finishes, the next token might have
// been peeked with preferRegExp set to true.
    StmtNode *
    Parser::parseAttributesAndStatement(const Token *t, AttributeStatement as,
                                        SemicolonState &semicolonState)
    {
        uint32 pos = t->getPos();
        NodeQueue<IdentifierList> attributes;
        while (t->getFlag(Token::isAttribute)) {
            attributes += new(arena) IdentifierList(t->getIdentifier());
            t = &lexer.get(false);
            if (lineBreakBefore(*t))
                syntaxError("Line break not allowed here");
        }
        
        switch (as) {
            case asAny:
                break;
                
            case asBlock:
                if (!t->hasKind(Token::openBrace))
                    syntaxError("'{' expected");
                break;
                
            case asConstVar:
                if (!t->hasKind(Token::Const) && !t->hasKind(Token::Var))
                    syntaxError("const or var expected");
                break;
        }
        return parseAttributeStatement(pos, attributes.first, *t,
                                       as == asConstVar, semicolonState);
    }
    
    
// Parse and return an AnnotatedBlock.
    StmtNode *
    Parser::parseAnnotatedBlock()
    {
        const Token &t = lexer.get(true);
        SemicolonState semicolonState;

        // If package is the first attribute then it must be the only
        // attribute.
        if (t.hasKind(Token::Package) &&
            !lexer.peek(false).hasKind(Token::openBrace))
            syntaxError("'{' expected", 0);
        return parseAttributesAndStatement(&t, asBlock, semicolonState);
    }


// Parse and return a ForStatement.  The 'for' token has already been read;
// its position is pos. If the statement ends with an optional semicolon,
// that semicolon is not parsed. Instead, parseFor returns a semicolonState
// with the same meaning as that in parseStatement.
//
// After parseFor finishes, the next token might have been peeked with
// preferRegExp set to true.
    StmtNode *
    Parser::parseFor(uint32 pos, SemicolonState &semicolonState)
    {
        require(true, Token::openParenthesis);
        const Token &t = lexer.get(true);
        uint32 tPos = t.getPos();
        const Token *t2;
        StmtNode *initializer = 0;
        ExprNode *expr1 = 0;
        ExprNode *expr2 = 0;
        ExprNode *expr3 = 0;
        StmtNode::Kind sKind = StmtNode::For;
        
        switch (t.getKind()) {
            case Token::semicolon:
                goto threeExpr;
                
            case Token::Const:
            case Token::Var:
            case Token::Final:
            case Token::Static:
            case Token::Volatile:
              makeAttribute:
                initializer = parseAttributesAndStatement(&t, asConstVar,
                                                          semicolonState);
          break;
          
            case CASE_TOKEN_NONRESERVED:
            case Token::Public:
            case Token::Package:
            case Token::Private:
                t2 = &lexer.peek(false);
                if (!lineBreakBefore(*t2) &&
                    t2->getFlag(Token::canFollowAttribute))
                    goto makeAttribute;
            default:
                lexer.unget();
                expr1 = parseExpression(true);
                initializer = new(arena) ExprStmtNode(tPos,
                                                      StmtNode::expression,
                                                      expr1);
                lexer.redesignate(true); // Safe: a '/' or a '/=' would have
                                         // been interpreted as an operator,
                                         // so it can't be the next token.
                break;
        }
        
        if (lexer.eat(true, Token::semicolon))
            threeExpr: {
            if (!lexer.eat(true, Token::semicolon)) {
                expr2 = parseExpression(false);
                require(false, Token::semicolon);
            }
            if (lexer.peek(true).hasKind(Token::closeParenthesis))
                lexer.redesignate(false);   // Safe: the token is ')'.
            else
                expr3 = parseExpression(false);
        }
        else if (lexer.eat(true, Token::In)) {
            sKind = StmtNode::ForIn;
            if (expr1) {
                ASSERT(initializer->hasKind(StmtNode::expression));
                ensurePostfix(expr1);
            } else {
                ASSERT(initializer->hasKind(StmtNode::Const) ||
                       initializer->hasKind(StmtNode::Var));
                const VariableBinding *bindings =
                    static_cast<VariableStmtNode *>(initializer)->bindings;
                if (!bindings || bindings->next)
                    syntaxError("Only one variable binding can be used in a "
                                "for-in statement", 0);
            }
            expr2 = parseExpression(false);
        }
        else
            syntaxError("';' or 'in' expected", 0);
        
        require(false, Token::closeParenthesis);
        return new(arena) ForStmtNode(pos, sKind, initializer, expr2, expr3,
                                      parseStatement(false, false,
                                                     semicolonState));
    }
    
    
// Parse and return a TryStatement.  The 'try' token has already been read;
// its position is pos. After parseTry finishes, the next token might have
// been peeked with preferRegExp set to true.
    StmtNode *
    Parser::parseTry(uint32 pos)
    {
        StmtNode *tryBlock = parseAnnotatedBlock();
        NodeQueue<CatchClause> catches;
        const Token *t;
        
        while ((t = lexer.eat(true, Token::Catch)) != 0) {
            uint32 catchPos = t->getPos();
            require(true, Token::openParenthesis);
            ExprNode *type;
            const StringAtom &name = parseTypedIdentifier(type);
            require(false, Token::closeParenthesis);
            catches += new(arena) CatchClause(catchPos, name, type,
                                              parseAnnotatedBlock());
        }
        StmtNode *finally = 0;
        if (lexer.eat(true, Token::Finally))
            finally = parseAnnotatedBlock();
        else if (!catches.first)
            syntaxError("A try statement must be followed by at least one "
                        "catch or finally", 0);
        
        return new(arena) TryStmtNode(pos, tryBlock, catches.first, finally);
    }
    
    
// Parse and return a TopStatement.  If topLevel is false, allow only
// Statements.
// If inSwitch is true, allow case <expr>: and default: statements.
//
// If the statement ends with an optional semicolon, that semicolon is not
// parsed.
// Instead, parseStatement returns in semicolonState one of three values:
//    semiNone:             No semicolon is needed to close the statement
//    semiNoninsertable:    A NoninsertableSemicolon is needed to close the
//                          statement; a line break is not enough
//    semiInsertable:       A Semicolon is needed to close the statement; a
//                          line break is also sufficient
//
// If the first token was peeked, it should be have been done with
// preferRegExp set to true.
// After parseStatement finishes, the next token might have been peeked with
// preferRegExp set to true.
    StmtNode *
    Parser::parseStatement(bool /*topLevel*/, bool inSwitch,
                           SemicolonState &semicolonState)
    {
        StmtNode *s;
        ExprNode *e = 0;
        StmtNode::Kind sKind;
        const Token &t = lexer.get(true);
        const Token *t2;
        uint32 pos = t.getPos();
        semicolonState = semiNone;

        checkStackSize();
        switch (t.getKind()) {
            case Token::semicolon:
                s = new(arena) StmtNode(pos, StmtNode::empty);
                break;
                
            case Token::Namespace:
                t2 = &lexer.peek(false);
                if (lineBreakBefore(*t2) || !t2->hasIdentifierKind())
                    goto makeExpression;
            case Token::openBrace:
            case Token::Const:
            case Token::Var:
            case Token::Function:
            case Token::Class:
            case Token::Interface:
                s = parseAttributeStatement(pos, 0, t, false, semicolonState);
                break;
                
            case Token::If:
                e = parseParenthesizedExpression();
                s = parseStatementAndSemicolon(semicolonState);
                if (lexer.eat(true, Token::Else))
                    s = new(arena) BinaryStmtNode(pos, StmtNode::IfElse, e, s,
                                                  parseStatement(false, false,
                                                                 semicolonState));
                else {
                    sKind = StmtNode::If;
                    goto makeUnary;
                }
                break;
                
            case Token::Switch:
                e = parseParenthesizedExpression();
                require(true, Token::openBrace);
                s = new(arena) SwitchStmtNode(pos, e, parseBlock(true, false));
                break;
                
            case Token::Case:
                if (!inSwitch)
                    goto notInSwitch;
                e = parseExpression(false);
          makeSwitchCase:
                require(false, Token::colon);
                s = new(arena) ExprStmtNode(pos, StmtNode::Case, e);
                break;
                
            case Token::Default:
                if (inSwitch)
                    goto makeSwitchCase;
          notInSwitch:
                syntaxError("case and default may only be used inside a "
                            "switch statement");
                break;
                
            case Token::Do:
            {
                SemicolonState semiState2;
                // Ignore semiState2.
                s = parseStatementAndSemicolon(semiState2);
                require(true, Token::While);
                e = parseParenthesizedExpression();
                sKind = StmtNode::DoWhile;
                goto makeUnary;
            }
            break;
            
            case Token::With:
                sKind = StmtNode::With;
                goto makeWhileWith;
                
            case Token::While:
                sKind = StmtNode::While;
          makeWhileWith:
                e = parseParenthesizedExpression();
                s = parseStatement(false, false, semicolonState);
          makeUnary:
                s = new(arena) UnaryStmtNode(pos, sKind, e, s);
                break;
                
            case Token::For:
                s = parseFor(pos, semicolonState);
                break;
                
            case Token::Continue:
                sKind = StmtNode::Continue;
                goto makeGo;
                
            case Token::Break:
                sKind = StmtNode::Break;
          makeGo:
                {
                    const StringAtom *label = 0;
                    t2 = &lexer.peek(true);
                    if (t2->hasKind(Token::identifier) &&
                        !lineBreakBefore(*t2)) {
                        lexer.get(true);
                        label = &t2->getIdentifier();
                    }
                    s = new(arena) GoStmtNode(pos, sKind, label);
                }
                goto insertableSemicolon;
                
            case Token::Return:
                sKind = StmtNode::Return;
                t2 = &lexer.peek(true);
                if (lineBreakBefore(*t2) ||
                    t2->getFlag(Token::canFollowReturn))
                    goto makeExprStmtNode;
                goto makeExpressionNode;
                
            case Token::Throw:
                sKind = StmtNode::Throw;
                if (lineBreakBefore())
                    syntaxError("throw cannot be followed by a line break", 0);
                goto makeExpressionNode;
                
            case Token::Try:
                s = parseTry(pos);
                break;
                
            case Token::Debugger:
                s = new(arena) DebuggerStmtNode(pos, StmtNode::Debugger);
                break;
                
            case Token::Final:
            case Token::Static:
            case Token::Volatile:
              makeAttribute:
                s = parseAttributesAndStatement(&t, asAny, semicolonState);
                break;
          
            case Token::Public:
            case Token::Package:
            case Token::Private:
                t2 = &lexer.peek(false);
                goto makeExpressionOrAttribute;
                
            case CASE_TOKEN_NONRESERVED:
                t2 = &lexer.peek(false);
                if (t2->hasKind(Token::colon)) {
                    lexer.get(false);
                    // Must do this now because parseStatement can
                    // invalidate t.
                    const StringAtom &name = t.getIdentifier();
                    s = new(arena) LabelStmtNode(pos, name,
                                                 parseStatement(false, false,
                                                                semicolonState));
                    break;
                }
          makeExpressionOrAttribute:
                if (!lineBreakBefore(*t2) &&
                    t2->getFlag(Token::canFollowAttribute))
                    goto makeAttribute;
            default:
          makeExpression:
                lexer.unget();
                sKind = StmtNode::expression;
          makeExpressionNode:
                e = parseExpression(false);
                // Safe: a '/' or a '/=' would have been interpreted as an
                // operator, so it can't be the next token.
                lexer.redesignate(true);
          makeExprStmtNode:
                s = new(arena) ExprStmtNode(pos, sKind, e);
      insertableSemicolon:
                semicolonState = semiInsertable;
                break;
        }
        return s;
    }
    
    
// Same as parseStatement but also swallow the following semicolon if one
// is present.
    StmtNode *
    Parser::parseStatementAndSemicolon(SemicolonState &semicolonState)
    {
        StmtNode *s = parseStatement(false, false, semicolonState);
        if (semicolonState != semiNone && lexer.eat(true, Token::semicolon))
            semicolonState = semiNone;
        return s;
    }
    
// BEGIN NEW CODE

    bool
    Parser::lookahead(Token::Kind kind, bool preferRegExp)
    {
        const Token &t = lexer.peek(preferRegExp);
        if (t.getKind() != kind)
            return false;
        return true;
    }
    
    const Token *
    Parser::match(Token::Kind kind, bool preferRegExp)
    {
        const Token *t = lexer.eat(preferRegExp,kind);
        if (!t || t->getKind() != kind)
            return 0;
        return t;
    }
    
/**
 * Identifier   
 *     Identifier
 *     get
 *     set
 */
    
    ExprNode *
    Parser::parseIdentifier()
    {

        ExprNode *result = NULL;
        
        if(lookahead(Token::Get)) {
            result = NodeFactory::Identifier(*match(Token::Get));
        } else if(lookahead(Token::Set)) {
            result = NodeFactory::Identifier(*match(Token::Set));
        } else if(lookahead(Token::identifier)) {
            result = NodeFactory::Identifier(*match(Token::identifier));
        } else {
            // throw new Exception("Expecting an Identifier when encountering "
                // "input: " + scanner.getErrorText());
        }
        
        return result;
    }
    
/**
 * LiteralField 
 *     FieldName : AssignmentExpressionallowIn
 */

    ExprPairList *
    Parser::parseLiteralField()
    {

        ExprPairList *result=NULL;
        ExprNode *first;
        ExprNode *second;

        first = parseFieldName();
        match(Token::colon);
        second = parseAssignmentExpression();
        lexer.redesignate(true); // Safe: looking for non-slash punctuation.
        result = NodeFactory::LiteralField(first,second);
        
        return result;
    }

/**
 * FieldName    
 *     Identifier
 *     String
 *     Number
 *     ParenthesizedExpression
 */

    ExprNode *
    Parser::parseFieldName()
    {

        ExprNode *result;

        if( lookahead(Token::string) ) {
            const Token *t = match(Token::string);
            result = NodeFactory::LiteralString(t->getPos(), ExprNode::string,
                                                copyTokenChars(*t));
        } else if(lookahead(Token::number)) {
            const Token *t = match(Token::number);
            result = NodeFactory::LiteralNumber(*t);
        } else if( lookahead(Token::openParenthesis) ) {
            result = parseParenthesizedExpression();
        } else {
            result = parseIdentifier();
        }
        
        return result;
    }

/**
 * ArgumentList 
 *     <empty>
 *     LiteralField NamedArgumentListPrime
 *     AssignmentExpression[allowIn] ArgumentListPrime
 */

    ExprPairList *
    Parser::parseArgumentList(NodeQueue<ExprPairList> &args)
    {

        ExprPairList *result=NULL;
     
        if (lookahead(Token::closeParenthesis)) {
        } else {
            ExprNode *first;
            first = parseAssignmentExpression();
            lexer.redesignate(true); //Safe: looking for non-slash punctuation.
                //if( AssignmentExpressionNode.isFieldName(first) &&
                //lookahead(colon_token) ) {
            if( lookahead(Token::colon) ) {
                ExprNode *second;
                match(Token::colon);
                // Finish parsing the LiteralField.
                second = parseAssignmentExpression();
                // Safe: looking for non-slash punctuation.
                lexer.redesignate(true);
                args += NodeFactory::LiteralField(first,second);
                result = parseNamedArgumentListPrime(args);
            } else {
                args += NodeFactory::LiteralField(NULL,first);
                result = parseArgumentListPrime(args);
            }
        }
        return result;
    }

/**
 * ArgumentListPrime    
 *     <empty>
 *     , AssignmentExpression[allowIn] ArgumentListPrime
 *     , LiteralField NamedArgumentListPrime
 */

    ExprPairList *
    Parser::parseArgumentListPrime(NodeQueue<ExprPairList> &args)
    {

        ExprPairList *result;
        
        if(lookahead(Token::comma)) {

            match(Token::comma);
            ExprNode *first;
            first = parseAssignmentExpression();
            lexer.redesignate(true); //Safe: looking for non-slash punctuation.
                //if( AssignmentExpressionNode.isFieldName(second) &&
                // lookahead(colon_token) ) {
            if( lookahead(Token::colon) ) {
                ExprNode *second;
                
                match(Token::colon);
                // Finish parsing the literal field.
                second = parseAssignmentExpression();
                // Safe: looking for non-slash punctuation.
                lexer.redesignate(true);
                args += NodeFactory::LiteralField(first,second);
                result = parseNamedArgumentListPrime(args);
                
            } else {
                args += NodeFactory::LiteralField(NULL,first);
                result = parseArgumentListPrime(args);
                
            }
            
        } else {
            result = args.first;
        }
        
        return result;
    }
    
/**
 * NamedArgumentListPrime   
 *     <empty>
 *     , LiteralField NamedArgumentListPrime
 */

    ExprPairList *
    Parser::parseNamedArgumentListPrime(NodeQueue<ExprPairList> &args)
    {

        ExprPairList *result;
        
        if(lookahead(Token::comma)) {
            match(Token::comma);
            args += parseLiteralField();
            result = parseNamedArgumentListPrime(args);
        } else {
            result = args.first;
        }
        
        return result;
    }
    

/**
 * AllParameters    
 *     Parameter
 *     Parameter , AllParameters
 *     Parameter OptionalParameterPrime
 *     Parameter OptionalParameterPrime , OptionalNamedRestParameters
 *     | NamedRestParameters
 *     RestParameter
 *     RestParameter , | NamedParameters
 */

    VariableBinding *
    Parser::parseAllParameters(FunctionDefinition &fd,
                               NodeQueue<VariableBinding> &params)
    {

        VariableBinding *result;
    
        if(lookahead(Token::tripleDot)) {
            fd.restParameter = parseRestParameter();
            params += fd.restParameter;
            if( lookahead(Token::comma) ) {
                match(Token::comma);
                match(Token::bitwiseOr);
                result = parseNamedParameters(fd,params);
            } else {
                result = params.first;
            }
        } else if( lookahead(Token::bitwiseOr) ) {
            match(Token::bitwiseOr);
            result = parseNamedRestParameters(fd,params);
        } else {
            VariableBinding *first;
            first = parseParameter();
            if( lookahead(Token::comma) ) {
                params += first;
                match(Token::comma);
                result = parseAllParameters(fd,params);
            } else if( lookahead(Token::assignment) ) {
                first = parseOptionalParameterPrime(first);
                if (!fd.optParameters) {
                    fd.optParameters = first;
                }
                params += first;
                if( lookahead(Token::comma) ) {
                    match(Token::comma);
                    result = parseOptionalNamedRestParameters(fd,params);
                } 
            } else {
                params += first;
                result = params.first;
            }
        }
        
        return result;
    }
    
/**
 * OptionalNamedRestParameters  
 *     OptionalParameter
 *     OptionalParameter , OptionalNamedRestParameters
 *     | NamedRestParameters
 *     RestParameter
 *     RestParameter , | NamedParameters
 */

    VariableBinding *
    Parser::parseOptionalNamedRestParameters (FunctionDefinition &fd,
                                              NodeQueue<VariableBinding> &params)
    {

        VariableBinding *result;
    
        if( lookahead(Token::tripleDot) ) {
            fd.restParameter = parseRestParameter();
            params += fd.restParameter;
            if( lookahead(Token::comma) ) {
                match(Token::comma);
                match(Token::bitwiseOr);
                result = parseNamedParameters(fd,params);
            } else {
                result = params.first;
            }
        } else if( lookahead(Token::bitwiseOr) ) {
            match(Token::bitwiseOr);
            result = parseNamedRestParameters(fd,params);
        } else {
            VariableBinding *first;
            first = parseOptionalParameter();
            if (!fd.optParameters) {
                fd.optParameters = first;
            }
            params += first;
            if( lookahead(Token::comma) ) {
                match(Token::comma);
                result = parseOptionalNamedRestParameters(fd,params);
            } else {
                result = params.first;
            } 
        }
        
        return result;
    }
    
/**
 * NamedRestParameters  
 *     NamedParameter
 *     NamedParameter , NamedRestParameters
 *     RestParameter
 */

    VariableBinding *
    Parser::parseNamedRestParameters(FunctionDefinition &fd,
                                     NodeQueue<VariableBinding> &params)
    {

        VariableBinding *result;
    
        if (lookahead(Token::tripleDot)) {
            fd.restParameter = parseRestParameter();
            params += fd.restParameter;
            result = params.first;
        } else {
            NodeQueue<IdentifierList> aliases;
            VariableBinding *first;
            first = parseNamedParameter(aliases);
            // The following marks the position in the list that named
            // parameters may occur. It is not required that this
            // particular parameter has 
            // aliases associated with it.
            if (!fd.namedParameters) {
                fd.namedParameters = first;
            }
            if (!fd.optParameters && first->initializer) {
                fd.optParameters = first;
            }
            if( lookahead(Token::comma) ) {
                params += first;
                match(Token::comma);
                result = parseNamedRestParameters(fd,params);
            } else {
                params += first;
                result = params.first;
            }
        }
        
        return result;
    }
    
/**
 * NamedParameters  
 *     NamedParameter
 *     NamedParameter , NamedParameters
 */

    VariableBinding *
    Parser::parseNamedParameters(FunctionDefinition &fd,
                                 NodeQueue<VariableBinding> &params)
    {

        VariableBinding *result,*first;
        NodeQueue<IdentifierList> aliases;   // List of aliases.
        first = parseNamedParameter(aliases);
        // The following marks the position in the list that named parameters
        // may occur. It is not required that this particular parameter has 
        // aliases associated with it.
        if (!fd.namedParameters) {
            fd.namedParameters = first;
        }
        if (!fd.optParameters && first->initializer) {
            fd.optParameters = first;
        }
        if (lookahead(Token::comma)) {
            params += first;
            match(Token::comma);
            result = parseNamedParameters(fd,params);
        } else {
            params += first;
            result = params.first;
        }
        
        return result;
    }
    
/**
 * RestParameter    
 *     ...
 *     ... Parameter
 */

    VariableBinding *
    Parser::parseRestParameter()
    {

        VariableBinding *result;
        
        match(Token::tripleDot);
        if (lookahead(Token::closeParenthesis) ||
            lookahead(Token::comma)) {
            result = NodeFactory::Parameter(0, 0, false);
        } else {
            result = parseParameter();
        }
        
        return result;
    }
    
/**
 * Parameter    
 *     Identifier
 *     Identifier : TypeExpression[allowIn]
 */

    VariableBinding *
    Parser::parseParameter()
    {
        bool constant = lexer.eat (true, Token::Const);        
        ExprNode *first = parseIdentifier();
        ExprNode *second = parseTypeBinding (Token::colon, false);
        
        return NodeFactory::Parameter(first, second, constant);
    }
    
/**
 * OptionalParameter    
 *     Parameter = AssignmentExpression[allowIn]
 */

    VariableBinding *
    Parser::parseOptionalParameter()
    {

        VariableBinding *result,*first;
        
        first = parseParameter();
        result = parseOptionalParameterPrime(first);
        
        return result;
    }
    
    VariableBinding *
    Parser::parseOptionalParameterPrime(VariableBinding *first)
    {

        VariableBinding* result=NULL;
        
        match(Token::assignment);
        first->initializer = parseAssignmentExpression();
        lexer.redesignate(true); // Safe: looking for non-slash puncutation.
        result = first;
        
        return result;
    }

/**
 * NamedParameter   
 *     Parameter
 *     OptionalParameter
 *     String NamedParameter
 */

    VariableBinding *
    Parser::parseNamedParameter(NodeQueue<IdentifierList> &aliases)
    {

        VariableBinding *result;
        
        if(lookahead(Token::string)) {
            const Token *t = match(Token::string);
            aliases += NodeFactory::ListedIdentifier(copyTokenChars(*t));
            result = parseNamedParameter(aliases);
        } else {
            result = parseParameter();
            result->aliases = aliases.first;
            if(lookahead(Token::assignment)) {
                result = parseOptionalParameterPrime(result);
            } 
        }
        return result;
    }

/**
 * ResultSignature  
 *     <empty>
 *     : TypeExpression[allowIn]
 */

    ExprNode *
    Parser::parseResultSignature()
    {

        ExprNode* result=NULL;
        
        if (lookahead(Token::colon))
        {
            match(Token::colon);
            result = parseTypeExpression(); // allowIn is default.
        } else {
            result = NULL;
        }

        return result;
    }


    bool
    IdentifierList::contains(Token::Kind kind)
    { 
        if (name.tokenKind == kind) 
            return true; 
        else 
            return (next) ? next->contains(kind) : false; 
    }
    
//
// Parser Utilities
//

    // Print extra parentheses around subexpressions?
    const bool debugExprNodePrint = true;
    // Size of one level of statement indentation
    const int32 basicIndent = 4;
    // Indentation before a case or default statement
    const int32 caseIndent = basicIndent/2;
    // Indentation of var or const statement bindings
    const int32 varIndent = 2;
    // Size of one level of expression indentation
    const int32 subexpressionIndent = 4;
    // Indentation of function signature
    const int32 functionHeaderIndent = 9;
    // Indentation of class, interface, or namespace header
    const int32 namespaceHeaderIndent = 4;


    static const char functionPrefixNames[3][5] = {"", "get ", "set " };

// Print this onto f.  name must be non-nil.
    void
    FunctionName::print(PrettyPrinter &f) const
    {
        f << functionPrefixNames[prefix];
        f << name;
    }
    

// Print this onto f.  if printConst is false, inhibit printing of the
// const keyword.
    void
    VariableBinding::print(PrettyPrinter &f, bool printConst) const
    {
        PrettyPrinter::Block b(f);

#ifdef NEW_PARSER
        if (aliases) {
            IdentifierList *id = aliases;
            f << "| ";
            while (id) {
                f << "'";
                f << id->name;
                f << "' ";
                id = id->next;
            }
        }
#endif

        if (printConst && constant)
            f << "const ";
        
        if (name)
            f << name;
        PrettyPrinter::Indent i(f, subexpressionIndent);
        if (type) {
            f.fillBreak(0);
            f << ": ";
            f << type;
        }
        if (initializer) {
            f.linearBreak(1);
            f << "= ";
            f << initializer;
        }
    }
    
    
// Print this onto f.  If attributes is null, this is a function expression;
// if attributes is non-null, this is a function statement with the given
// attributes.
// When there is no function body, print a trailing semicolon unless noSemi is
// true.
    void
    FunctionDefinition::print(PrettyPrinter &f,
                              const AttributeStmtNode *attributes,
                              bool noSemi) const
    {
        PrettyPrinter::Block b(f);
        if (attributes)
            attributes->printAttributes(f);
        
        f << "function";
        
        if (name) {
            f << ' ';
            FunctionName::print(f);
        }
        {
            PrettyPrinter::Indent i(f, functionHeaderIndent);
            f.fillBreak(0);
            f << '(';
            {
                PrettyPrinter::Block b2(f);
                const VariableBinding *p = parameters;
                if (p)
                    while (true) {
                        if (p == restParameter) {
                            f << "...";
                            if (p->name)
                                f << ' ';
                        }
                        p->print(f, true);
                        p = p->next;
                        if (!p)
                            break;
                        f << ',';
                        f.fillBreak(1);
                    }
                f << ')';
            }
            if (resultType) {
                f.fillBreak(0);
                f << ": ";
                f << resultType;
            }
        }
        if (body) {
            bool loose = attributes != 0;
            f.linearBreak(1, loose);
            body->printBlock(f, loose);
        } else
            StmtNode::printSemi(f, noSemi);
    }
    
// Print a comma-separated ExprList on to f.  Since a method can't be called
// on nil, the list has at least one element.
    void
    ExprList::printCommaList(PrettyPrinter &f) const
    {
        PrettyPrinter::Block b(f);
        const ExprList *list = this;
        while (true) {
            f << list->expr;
            list = list->next;
            if (!list)
                break;
            f << ',';
            f.fillBreak(1);
        }
    }
    
    
// If list is nil, do nothing.  Otherwise, emit a linear line break of size 1,
// the name, a space, and the contents of list separated by commas.
    void
    ExprList::printOptionalCommaList(PrettyPrinter &f, const char *name,
                                     const ExprList *list)
    {
        if (list) {
            f.linearBreak(1);
            f << name << ' ';
            list->printCommaList(f);
        }
    }
    
    
//
// ExprNode
//

    const char *const ExprNode::kindNames[kindsEnd] = {
        "NIL",                  // none
        0,                      // identifier
        0,                      // number
        0,                      // string
        0,                      // regExp
        "null",                 // Null
        "true",                 // True
        "false",                // False
        "this",                 // This
        "super",                // Super
        
        0,                      // parentheses
        0,                      // numUnit
        0,                      // exprUnit
        "::",                   // qualify
        
        0,                      // objectLiteral
        0,                      // arrayLiteral
        0,                      // functionLiteral
        
        0,                      // call
        0,                      // New
        0,                      // index
        
        ".",                    // dot
        ".class",               // dotClass 
        ".(",                   // dotParen
        "@",                    // at
        
        "delete ",              // Delete
        "typeof ",              // Typeof
        "eval ",                // Eval
        "++ ",                  // preIncrement
        "-- ",                  // preDecrement
        " ++",                  // postIncrement
        " --",                  // postDecrement
        "+ ",                   // plus
        "- ",                   // minus
        "~ ",                   // complement
        "! ",                   // logicalNot

        "+",                    // add
        "-",                    // subtract
        "*",                    // multiply
        "/",                    // divide
        "%",                    // modulo
        "<<",                   // leftShift
        ">>",                   // rightShift
        ">>>",                  // logicalRightShift
        "&",                    // bitwiseAnd
        "^",                    // bitwiseXor
        "|",                    // bitwiseOr
        "&&",                   // logicalAnd
        "^^",                   // logicalXor
        "||",                   // logicalOr

        "==",                   // equal
        "!=",                   // notEqual
        "<",                    // lessThan
        "<=",                   // lessThanOrEqual
        ">",                    // greaterThan
        ">=",                   // greaterThanOrEqual
        "===",                  // identical
        "!==",                  // notIdentical
        "in",                   // In
        "instanceof",           // Instanceof

        "=",                    // assignment
        "+=",                   // addEquals
        "-=",                   // subtractEquals
        "*=",                   // multiplyEquals
        "/=",                   // divideEquals
        "%=",                   // moduloEquals
        "<<=",                  // leftShiftEquals
        ">>=",                  // rightShiftEquals
        ">>>=",                 // logicalRightShiftEquals
        "&=",                   // bitwiseAndEquals
        "^=",                   // bitwiseXorEquals
        "|=",                   // bitwiseOrEquals
        "&&=",                  // logicalAndEquals
        "^^=",                  // logicalXorEquals
        "||=",                  // logicalOrEquals

        "?",                    // conditional
        ","                     // comma
};


// Print this onto f.
    void
    ExprNode::print(PrettyPrinter &f) const
    {
        f << kindName(kind);
    }

    void
    IdentifierExprNode::print(PrettyPrinter &f) const
    {
        f << name;
    }
    
    void
    NumberExprNode::print(PrettyPrinter &f) const
    {
        f << value;
    }
    
    void
    StringExprNode::print(PrettyPrinter &f) const
    {
        quoteString(f, str, '"');
    }
    
    void
    RegExpExprNode::print(PrettyPrinter &f) const
    {
        f << '/' << re << '/' << flags;
    }

    void
    NumUnitExprNode::print(PrettyPrinter &f) const
    {
        f << numStr;
        StringExprNode::print(f);
    }
    
    void
    ExprUnitExprNode::print(PrettyPrinter &f) const
    {
        f << op;
        StringExprNode::print(f);
    }

    void
    FunctionExprNode::print(PrettyPrinter &f) const
    {
        function.print(f, 0, false);
    }
    
    void
    PairListExprNode::print(PrettyPrinter &f) const
    {
        char beginBracket;
        char endBracket;
        
        switch (getKind()) {
            case objectLiteral:
                beginBracket = '{';
                endBracket = '}';
                break;
                
            case arrayLiteral:
            case index:
                beginBracket = '[';
                endBracket = ']';
                break;
                
            case call:
            case New:
                beginBracket = '(';
                endBracket = ')';
                break;
                
            default:
                NOT_REACHED("Bad kind");
                return;
        }
        
        f << beginBracket;
        PrettyPrinter::Block b(f);
        const ExprPairList *p = pairs;
        if (p) 
            while (true) {
                const ExprNode *field = p->field;
                if (field) {
                    f << field << ':';
                    f.fillBreak(0);
                }
                
                const ExprNode *value = p->value;
                if (value)
                    f << value;
                
                p = p->next;
                if (!p)
                    break;
                f << ',';
                f.linearBreak(static_cast<uint32>(field || value));
            }
        f << endBracket;
    }
    
    void
    InvokeExprNode::print(PrettyPrinter &f) const
    {
        PrettyPrinter::Block b(f);
        if (hasKind(New))
            f << "new ";
        f << op;
        PrettyPrinter::Indent i(f, subexpressionIndent);
        f.fillBreak(0);
        PairListExprNode::print(f);
    }
    
    void
    UnaryExprNode::print(PrettyPrinter &f) const
    {
        if (hasKind(parentheses)) {
            f << '(';
            f << op;
            f << ')';
        } else {
            if (debugExprNodePrint)
                f << '(';
            const char *name = kindName(getKind());
            if (hasKind(postIncrement) || hasKind(postDecrement) ||
                hasKind(dotClass)) {
                f << op;
                f << name;
            } else {
                f << name;
                f << op;
            }
            if (debugExprNodePrint)
                f << ')';
        }
    }
    
    void
    BinaryExprNode::print(PrettyPrinter &f) const
    {
        if (debugExprNodePrint && !hasKind(qualify))
            f << '(';
        PrettyPrinter::Block b(f);
        f << op1;
        uint32 nSpaces = hasKind(dot) || hasKind(dotParen) || hasKind(at) ||
            hasKind(qualify) ? (uint32)0 : (uint32)1;
        f.fillBreak(nSpaces);
        f << kindName(getKind());
        f.fillBreak(nSpaces);
        f << op2;
        if (hasKind(dotParen))
            f << ')';
        if (debugExprNodePrint && !hasKind(qualify))
            f << ')';
    }
    
    void
    TernaryExprNode::print(PrettyPrinter &f) const
    {
        if (debugExprNodePrint)
            f << '(';
        PrettyPrinter::Block b(f);
        f << op1;
        f.fillBreak(1);
        f << '?';
        f.fillBreak(1);
        f << op2;
        f.fillBreak(1);
        f << ':';
        f.fillBreak(1);
        f << op3;
        if (debugExprNodePrint)
            f << ')';
    }
    
//
// StmtNode
//

// Print statements on separate lines onto f.  Do not print a line break
// after the last statement.
    void
    StmtNode::printStatements(PrettyPrinter &f, const StmtNode *statements)
    {
        if (statements) {
            PrettyPrinter::Block b(f);
            while (true) {
                const StmtNode *next = statements->next;
                statements->print(f, !next);
                statements = next;
                if (!statements)
                    break;
                f.requiredBreak();
            }
        }
    }

// Print statements as a block enclosed in curly braces onto f.
// If loose is false, do not insist on each statement being on a separate
// line; instead, make breaks between statements be linear breaks in the
// enclosing PrettyPrinter::Block scope. The caller must have placed this
// call inside a PrettyPrinter::Block scope.
    void
    StmtNode::printBlockStatements(PrettyPrinter &f,
                                   const StmtNode *statements, bool loose)
    {
        f << '{';
        if (statements) {
            {
                PrettyPrinter::Indent i(f, basicIndent);
                uint32 nSpaces = 0;
                while (statements) {
                    if (statements->hasKind(Case)) {
                        PrettyPrinter::Indent i2(f, caseIndent - basicIndent);
                        f.linearBreak(nSpaces, loose);
                        statements->print(f, false);
                    } else {
                        f.linearBreak(nSpaces, loose);
                        statements->print(f, !statements->next);
                    }
                    statements = statements->next;
                    nSpaces = 1;
                }
            }
            f.linearBreak(0, loose);
        } else
            f.fillBreak(0);
        f << '}';
    }
    
    
// Print a closing statement semicolon onto f unless noSemi is true.
    void
    StmtNode::printSemi(PrettyPrinter &f, bool noSemi)
    {
        if (!noSemi)
            f << ';';
    }
    
    
// Print this as a substatement of a statement such as if or with.
// If this statement is a block without attributes, begin it on the current
// line and do not indent it -- the block itself will provide the indent.
// Otherwise, begin this statement on a new line and indent it.
// If continuation is non-nil, it specifies a continuation such as 'else' or
// the 'while' of a do-while statement.  If this statement is a block without
// attributes, print a space and the continuation after the closing brace;
// otherwise print the continuation on a new line.
// If noSemi is true, do not print the semicolon unless it is required by the
// statement. The caller must have placed this call inside a
// PrettyPrinter::Block scope that encloses the containining statement.
    void
    StmtNode::printSubstatement(PrettyPrinter &f, bool noSemi,
                                const char *continuation) const
    {
        if (hasKind(block) &&
            !static_cast<const BlockStmtNode *>(this)->attributes) {
            f << ' ';
            static_cast<const BlockStmtNode *>(this)->printBlock(f, true);
            if (continuation)
                f << ' ' << continuation;
        } else {
            {
                PrettyPrinter::Indent i(f, basicIndent);
                f.requiredBreak();
                this->print(f, noSemi);
            }
            if (continuation) {
                f.requiredBreak();
                f << continuation;
            }
        }
    }
    
    
// Print the attributes on a single line separated with and followed by a
// space.
    void AttributeStmtNode::printAttributes(PrettyPrinter &f) const
    {
        for (const IdentifierList *a = attributes; a; a = a->next)
            f << a->name << ' ';
    }
    
    
// Print this block, including attributes, onto f.
// If loose is false, do not insist on each statement being on a separate
// line; instead, make breaks between statements be linear breaks in the
// enclosing PrettyPrinter::Block scope.
// The caller must have placed this call inside a PrettyPrinter::Block scope.
    void BlockStmtNode::printBlock(PrettyPrinter &f, bool loose) const
    {
        printAttributes(f);
        printBlockStatements(f, statements, loose);
    }
    
// Print this onto f.
// If noSemi is true, do not print the trailing semicolon unless it is
// required by the statement.
    void
    StmtNode::print(PrettyPrinter &f, bool /*noSemi*/) const
    {
        ASSERT(hasKind(empty));
        f << ';';
    }

    void
    ExprStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        const ExprNode *e = expr;
        
        switch (getKind()) {
            case Case:
                if (e) {
                    f << "case ";
                    f << e;
                } else
                    f << "default";
                f << ':';
                break;
                
            case Return:
                f << "return";
                if (e) {
                    f << ' ';
                    goto showExpr;
                } else
                    goto showSemicolon;
                
            case Throw:
                f << "throw ";
            case expression:
          showExpr:
          f << e;
          showSemicolon:
          printSemi(f, noSemi);
          break;
          
            default:
                NOT_REACHED("Bad kind");
        }
    }
    
    void
    DebuggerStmtNode::print(PrettyPrinter &f, bool) const
    {
        f << "debugger;";
    }
    
    void
    BlockStmtNode::print(PrettyPrinter &f, bool) const
    {
        PrettyPrinter::Block b(f, 0);
        printBlock(f, true);
    }
    
    void
    LabelStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        PrettyPrinter::Block b(f, basicIndent);
        f << name << ':';
        f.linearBreak(1);
        stmt->print(f, noSemi);
    }
    
    void
    UnaryStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        PrettyPrinter::Block b(f, 0);
        printContents(f, noSemi);
    }

// Same as print except that uses the caller's PrettyPrinter::Block.
    void
    UnaryStmtNode::printContents(PrettyPrinter &f, bool noSemi) const
    {
        ASSERT(stmt);
        const char *kindName = 0;
        
        switch (getKind()) {
            case If:
                kindName = "if";
                break;
                
            case While:
                kindName = "while";
                break;
                
            case DoWhile:
                f << "do";
                stmt->printSubstatement(f, true, "while (");
                f << expr << ')';
                printSemi(f, noSemi);
                return;
                
            case With:
                kindName = "with";
                break;
                
            default:
                NOT_REACHED("Bad kind");
        }
        
        f << kindName << " (";
        f << expr << ')';
        stmt->printSubstatement(f, noSemi);
    }
    
    void
    BinaryStmtNode::printContents(PrettyPrinter &f, bool noSemi) const
    {
        ASSERT(stmt && stmt2 && hasKind(IfElse));
        
        f << "if (";
        f << expr << ')';
        stmt->printSubstatement(f, true, "else");
        if (stmt2->hasKind(If) || stmt2->hasKind(IfElse)) {
            f << ' ';
            static_cast<const UnaryStmtNode *>(stmt2)->printContents(f,
                                                                     noSemi);
        } else
            stmt2->printSubstatement(f, noSemi);
    }
    
    void
    ForStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        ASSERT(stmt && (hasKind(For) || hasKind(ForIn)));
        
        PrettyPrinter::Block b(f, 0);
        f << "for (";
        {
            PrettyPrinter::Block b2(f);
            if (initializer)
                initializer->print(f, true);
            if (hasKind(ForIn)) {
                f.fillBreak(1);
                f << "in";
                f.fillBreak(1);
                ASSERT(expr2 && !expr3);
                f << expr2;
            } else {
                f << ';';
                if (expr2) {
                    f.linearBreak(1);
                    f << expr2;
                }
                f << ';';
                if (expr3) {
                    f.linearBreak(1);
                    f << expr3;
                }
            }
            f << ')';
        }
        stmt->printSubstatement(f, noSemi);
    }
    
    void
    SwitchStmtNode::print(PrettyPrinter &f, bool) const
    {
        PrettyPrinter::Block b(f);
        f << "switch (";
        f << expr << ") ";
        printBlockStatements(f, statements, true);
    }
    
    void
    GoStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        const char *kindName = 0;
        
        switch (getKind()) {
            case Break:
                kindName = "break";
                break;
                
            case Continue:
                kindName = "continue";
                break;
                
            default:
                NOT_REACHED("Bad kind");
        }
        
        f << kindName;
        if (name)
            f << " " << *name;
        printSemi(f, noSemi);
    }
    
    void
    TryStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        PrettyPrinter::Block b(f, 0);
        f << "try";
        const StmtNode *s = stmt;
        for (const CatchClause *c = catches; c; c = c->next) {
            s->printSubstatement(f, true, "catch (");
            PrettyPrinter::Block b2(f);
            f << c->name;
            ExprNode *t = c->type;
            if (t) {
                f << ':';
                f.linearBreak(1);
                f << t;
            }
            f << ')';
            s = c->stmt;
        }
        if (finally) {
            s->printSubstatement(f, true, "finally");
            s = finally;
        }
        s->printSubstatement(f, noSemi);
    }
    
    void
    VariableStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        printAttributes(f);
        ASSERT(hasKind(Const) || hasKind(Var));
        f << (hasKind(Const) ? "const" : "var");
        {
            PrettyPrinter::Block b(f, basicIndent);
            const VariableBinding *binding = bindings;
            if (binding)
                while (true) {
                    f.linearBreak(1);
                    binding->print(f, false);
                    binding = binding->next;
                    if (!binding)
                        break;
                    f << ',';
                }
        }
        printSemi(f, noSemi);
    }
    
    void
    FunctionStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        function.print(f, this, noSemi);
    }
    
    void
    NamespaceStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        printAttributes(f);
        ASSERT(hasKind(Namespace));
        
        PrettyPrinter::Block b(f, namespaceHeaderIndent);
        f << "namespace ";
        f << name;
        ExprList::printOptionalCommaList(f, "extends", supers);
        printSemi(f, noSemi);
    }
    
    void
    ClassStmtNode::print(PrettyPrinter &f, bool noSemi) const
    {
        printAttributes(f);
        ASSERT(hasKind(Class) || hasKind(Interface));
        
        {
            PrettyPrinter::Block b(f, namespaceHeaderIndent);
            const char *keyword;
            const char *superlistKeyword;
            if (hasKind(Class)) {
                keyword = "class ";
                superlistKeyword = "implements";
            } else {
                keyword = "interface ";
                superlistKeyword = "extends";
            }
            f << keyword;
            f << name;
            if (superclass) {
                f.linearBreak(1);
                f << "extends ";
                f << superclass;
            }
            ExprList::printOptionalCommaList(f, superlistKeyword, supers);
        }
        if (body) {
            f.requiredBreak();
            body->printBlock(f, true);
        } else
            printSemi(f, noSemi);
    }
}
