/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- fixed a bug in CreateExpr (@xxx=/yyy was parsed as @xxx=@xxx/yyy)
 * Marina Mechtcheriakova
 *   -- fixed bug in ::parsePredicates,
 *      made sure we continue looking for more predicates.
 *
 * $Id: txExprParser.cpp,v 1.3 2005/11/02 07:33:27 Peter.VanderBeken%pandora.be Exp $
 */

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @see ExprLexer
 * @version $Revision: 1.3 $ $Date: 2005/11/02 07:33:27 $
**/

#include "ExprParser.h"

const String ExprParser::L_CURLY_BRACE = "{";
const String ExprParser::R_CURLY_BRACE = "}";

/**
 * Creates a new ExprParser
**/
ExprParser::ExprParser() {};

/**
 * Default Destructor
**/
ExprParser::~ExprParser() {};

/**
 * Creates an Attribute Value Template using the given value
**/
AttributeValueTemplate* ExprParser::createAttributeValueTemplate
    (const String& attValue)
{

    AttributeValueTemplate* avt = new AttributeValueTemplate();
    Int32 size = attValue.length();
    int cc = 0;
    String buffer;
    MBool inExpr    = MB_FALSE;
    MBool inLiteral = MB_FALSE;
    char endLiteral = '"';
    char prevCh = '\0';

    while ( cc < size) {
        UNICODE_CHAR ch = attValue.charAt(cc++);
        // if in literal just add ch to buffer
        if ( inLiteral && (ch != endLiteral) ) {
                buffer.append(ch);
                prevCh = ch;
                continue;
        }
        switch ( ch ) {
            case '\'' :
            case '"' :
                buffer.append(ch);
                if (inLiteral) inLiteral = MB_FALSE;
                else {
                    inLiteral = MB_TRUE;
                    endLiteral = ch;
                }
                break;
            case  '{' :
                // Ignore case where we find two { without a }
                if (!inExpr) {
                    //-- clear buffer
                    if ( buffer.length() > 0) {
                        avt->addExpr(new StringExpr(buffer));
                        buffer.clear();
                    }
                    inExpr = MB_TRUE;
                }
                else if (prevCh == ch) {
                    inExpr = MB_FALSE;
                    buffer.append(ch);
                }
                else {
                    buffer.append(ch); //-- simply append '{'
                    ch = '\0';
                }
                break;
            case '}':
                if (inExpr) {
                    inExpr = MB_FALSE;
                    avt->addExpr(createExpr(buffer));
                    buffer.clear();
                    //-- change in case another '}' follows
                    ch = '\0';
                }
                else if (prevCh != ch) {
                    if ( buffer.length() > 0) buffer.append('}');
                    else avt->addExpr(new StringExpr(R_CURLY_BRACE));
                }
                break;
            default:
                buffer.append(ch);
                break;
        }
        prevCh = ch;
    }
    if ( buffer.length() > 0) {
        if ( inExpr ) {
            //-- error
            String errMsg("#error evaluating AttributeValueTemplate. ");
            errMsg.append("Missing '}' after: ");
            errMsg.append(buffer);
            avt->addExpr(new StringExpr(errMsg));
        }
        else avt->addExpr(new StringExpr(buffer));
    }
    return avt;

} //-- createAttributeValueTemplate

Expr* ExprParser::createExpr(const String& pattern) {
    ExprLexer lexer(pattern);
    return createExpr(lexer);
} //-- createExpr

PatternExpr* ExprParser::createPatternExpr(const String& pattern) {
    ExprLexer lexer(pattern);
    return createUnionExpr(lexer);
} //-- createPatternExpr

LocationStep* ExprParser::createLocationStep(const String& path) {
    ExprLexer lexer(path);
    LocationStep* lstep = createLocationStep(lexer);
    return lstep;
} //-- createLocationPath

  //--------------------/
 //- Private Methods -/
//-------------------/

/**
 * Creates a binary Expr for the given operator
**/
Expr* ExprParser::createBinaryExpr   (Expr* left, Expr* right, Token* op) {
    if ( !op ) return 0;
    switch(op->type) {


        //-- additive ops
        case Token::ADDITION_OP :
            return new AdditiveExpr(left, right, AdditiveExpr::ADDITION);
        case Token::SUBTRACTION_OP:
            return new AdditiveExpr(left, right, AdditiveExpr::SUBTRACTION);

        //-- case boolean ops
        case Token::AND_OP:
            return new BooleanExpr(left, right, BooleanExpr::AND);
        case Token::OR_OP:
            return new BooleanExpr(left, right, BooleanExpr::OR);

        //-- equality ops
        case Token::EQUAL_OP :
            return new RelationalExpr(left, right, RelationalExpr::EQUAL);
        case Token::NOT_EQUAL_OP :
            return new RelationalExpr(left, right, RelationalExpr::NOT_EQUAL);

        //-- relational ops
        case Token::LESS_THAN_OP:
            return new RelationalExpr(left, right, RelationalExpr::LESS_THAN);
        case Token::GREATER_THAN_OP:
            return new RelationalExpr(left, right, RelationalExpr::GREATER_THAN);
        case Token::LESS_OR_EQUAL_OP:
            return new RelationalExpr(left, right, RelationalExpr::LESS_OR_EQUAL);
        case Token::GREATER_OR_EQUAL_OP:
            return new RelationalExpr(left, right, RelationalExpr::GREATER_OR_EQUAL);

        //-- multiplicative ops
        case Token::DIVIDE_OP :
            return new MultiplicativeExpr(left, right, MultiplicativeExpr::DIVIDE);
        case Token::MODULUS_OP :
            return new MultiplicativeExpr(left, right, MultiplicativeExpr::MODULUS);
        case Token::MULTIPLY_OP :
            return new MultiplicativeExpr(left, right, MultiplicativeExpr::MULTIPLY);
        default:
            break;

    }
    return 0;
    //return new ErrorExpr();
} //-- createBinaryExpr


Expr*  ExprParser::createExpr(ExprLexer& lexer) {

    MBool done = MB_FALSE;

    Expr* expr = 0;

    Stack exprs;
    Stack ops;

    while ( lexer.hasMoreTokens() && (!done)) {

        Token* tok = lexer.nextToken();
        switch ( tok->type ) {
            case Token::R_BRACKET:
            case Token::R_PAREN:
            case Token::COMMA :
                lexer.pushBack();
                done = MB_TRUE;
                break;
            case Token::L_PAREN: //-- Grouping Expression
                expr = createExpr(lexer);
                //-- look for end ')'
                if ( lexer.hasMoreTokens() &&
                        ( lexer.nextToken()->type == Token::R_PAREN ) ) break;
                else {
                    //-- error
                    delete expr;
                    expr = new StringExpr("missing ')' in expression");
                }
                break;
            case Token::ANCESTOR_OP:
            case Token::PARENT_OP:
                lexer.pushBack();
                if ( !expr ) expr = createPathExpr(lexer);
                else {
                    PathExpr* pathExpr = createPathExpr(lexer);
                    pathExpr->addPatternExpr(0, (PatternExpr*)expr,
                                                 PathExpr::RELATIVE_OP);
                    expr = pathExpr;
                }
                //done = MB_TRUE;
                break;
            case Token::UNION_OP :
            {
                UnionExpr* unionExpr = createUnionExpr(lexer);
                unionExpr->addPathExpr(0, (PathExpr*)expr );
                expr = unionExpr;
                done = MB_TRUE;
                break;
            }
            case Token::LITERAL :
                expr = new StringExpr(tok->value);
                break;
            case Token::NUMBER:
            {
                StringResult str(tok->value);
                expr = new NumberExpr(str.numberValue());
                break;
            }
            case Token::FUNCTION_NAME:
            {
                lexer.pushBack();
                expr = createFunctionCall(lexer);
                break;
            }
            case Token::VAR_REFERENCE:
                expr = new VariableRefExpr(tok->value);
                break;
            //-- additive ops
            case Token::ADDITION_OP:
            case Token::DIVIDE_OP:
            //-- boolean ops
            case Token::AND_OP :
            case Token::OR_OP :
            //-- equality ops
            case Token::EQUAL_OP:
            case Token::NOT_EQUAL_OP:
            //-- relational ops
            case Token::LESS_THAN_OP:
            case Token::GREATER_THAN_OP:
            case Token::LESS_OR_EQUAL_OP:
            case Token::GREATER_OR_EQUAL_OP:
            //-- multiplicative ops
            case Token::MODULUS_OP:
            case Token::MULTIPLY_OP:
            case Token::SUBTRACTION_OP:
            {
                if ( !exprs.empty() ) {
                    short ttype = ((Token*)ops.peek())->type;
                    if (precedenceLevel(tok->type) < precedenceLevel(ttype)) {
                        expr = createBinaryExpr((Expr*)exprs.pop(), expr,
                            (Token*)ops.pop());
                    }
                }
                exprs.push(expr);
                ops.push(tok);
                expr = 0; // OG, prevent reuse of expr
                break;
            }
            default:
                lexer.pushBack();
                expr = createPatternExpr(lexer);
                break;
        }
    }

    // make sure expr != 0, will this happen?
    if (( expr == 0 ) && (!exprs.empty()))
        expr = (Expr*) exprs.pop();

    while (!exprs.empty() ) {
        expr = createBinaryExpr((Expr*)exprs.pop(), expr, (Token*)ops.pop());
    }

    return expr;

} //-- createExpr

FilterExpr*  ExprParser::createFilterExpr(ExprLexer& lexer) {

    FilterExpr* filterExpr = new FilterExpr();
    Token* tok = lexer.nextToken();
    if ( !tok ) return filterExpr;

    Expr* expr = 0;
    switch ( tok->type ) {
        case Token::FUNCTION_NAME :
            expr = createFunctionCall(lexer);
            filterExpr->setExpr(expr);
            break;
        case Token::VAR_REFERENCE :
            expr = new VariableRefExpr(tok->value);
            filterExpr->setExpr(expr);
            break;
        case Token::L_PAREN:
            //-- primary group expr:
            expr = createExpr(lexer);
            tok = lexer.nextToken();
            if ( (!tok) || (tok->type != Token::R_PAREN ) ) {
                String errMsg("error: ");
                expr->toString(errMsg);
                errMsg.append(" - missing ')'");
                delete expr; //-- free up current expr
                expr = new ErrorFunctionCall(errMsg);
            }
            filterExpr->setExpr(expr);
            break;
        case Token::PARENT_NODE :
            expr = new ParentExpr();
            filterExpr->setExpr(expr);
            break;
        case Token::SELF_NODE :
            expr = new IdentityExpr();
            filterExpr->setExpr(expr);
            break;
        default:
            break;
    }

    //-- handle predicates
    parsePredicates(filterExpr, lexer);

    return filterExpr;

} //-- createFilterExpr

FunctionCall* ExprParser::createFunctionCall(ExprLexer& lexer) {

    if ( !lexer.hasMoreTokens() ) {
        //-- should never see this, I hope
        return new ErrorFunctionCall("no tokens, invalid function call");
    }

    FunctionCall* fnCall = 0;

    Token* tok = lexer.nextToken();

    if ( tok->type != Token::FUNCTION_NAME ) {
        return new ErrorFunctionCall("invalid function call");
    }

    String fnName = tok->value;

    //-- compare function names
    //-- * we should hash these names for speed

    if ( XPathNames::BOOLEAN_FN.isEqual(tok->value) ) {
        fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_BOOLEAN);
    }
    else if ( XPathNames::CONCAT_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::CONCAT);
    }
    else if ( XPathNames::CONTAINS_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::CONTAINS);
    }
    else if ( XPathNames::COUNT_FN.isEqual(tok->value) ) {
        fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::COUNT);
    }
    else if ( XPathNames::FALSE_FN.isEqual(tok->value) ) {
        fnCall = new BooleanFunctionCall();
    }
    else if ( XPathNames::LAST_FN.isEqual(tok->value) ) {
        fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::LAST);
    }
    else if ( XPathNames::LOCAL_NAME_FN.isEqual(tok->value) ) {
        fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::LOCAL_NAME);
    }
    else if ( XPathNames::NAME_FN.isEqual(tok->value) ) {
        fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::NAME);
    }
    else if ( XPathNames::NAMESPACE_URI_FN.isEqual(tok->value) ) {
        fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::NAMESPACE_URI);
    }
    else if ( XPathNames::NOT_FN.isEqual(tok->value) ) {
        fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_NOT);
    }
    else if ( XPathNames::POSITION_FN.isEqual(tok->value) ) {
        fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::POSITION);
    }
    else if ( XPathNames::STARTS_WITH_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::STARTS_WITH);
    }
    else if ( XPathNames::STRING_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::STRING);
    }
    else if ( XPathNames::STRING_LENGTH_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::STRING_LENGTH);
    }
    else if ( XPathNames::SUBSTRING_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::SUBSTRING);
    }
    else if ( XPathNames::SUBSTRING_AFTER_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::SUBSTRING_AFTER);
    }
    else if ( XPathNames::SUBSTRING_BEFORE_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::SUBSTRING_BEFORE);
    }
    else if ( XPathNames::TRANSLATE_FN.isEqual(tok->value) ) {
        fnCall = new StringFunctionCall(StringFunctionCall::TRANSLATE);
    }
    else if ( XPathNames::TRUE_FN.isEqual(tok->value) ) {
        fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_TRUE);
    }
    // OG+
    else if ( XPathNames::NUMBER_FN.isEqual(tok->value) ) {
        fnCall = new NumberFunctionCall(NumberFunctionCall::NUMBER);
    }
    else if ( XPathNames::ROUND_FN.isEqual(tok->value) ) {
        fnCall = new NumberFunctionCall(NumberFunctionCall::ROUND);
    }
    else if ( XPathNames::CEILING_FN.isEqual(tok->value) ) {
        fnCall = new NumberFunctionCall(NumberFunctionCall::CEILING);
    }
    else if ( XPathNames::FLOOR_FN.isEqual(tok->value) ) {
        fnCall = new NumberFunctionCall(NumberFunctionCall::FLOOR);
    }
    // OG-
    else {
        //-- create error function() for now, should be ext function
        String err = "not a valid function: ";
        err.append(tok->value);
        fnCall = new ErrorFunctionCall(err);
    }
    //-- handle parametes
    List params;
    String* errMsg = parseParameters(&params, lexer);
    if (errMsg) {
        String err("error with function call, \"");
        err.append(fnName);
        err.append("\" : ");
        err.append(*errMsg);
        fnCall = new ErrorFunctionCall(err);
        delete errMsg;
    }
    // copy params
    else if (params.getLength() > 0) {
        ListIterator* iter = params.iterator();
        while ( iter->hasNext() ) fnCall->addParam( (Expr*)iter->next() );
        delete iter;
    }
    return fnCall;
} //-- createFunctionCall

LocationStep* ExprParser::createLocationStep(ExprLexer& lexer) {

    LocationStep* lstep = new LocationStep();
    short axisIdentifier = LocationStep::CHILD_AXIS;

    //-- get Axis Identifier, if present
    Token* tok = lexer.peek();
    MBool setDefaultAxis = MB_TRUE;
    if ( tok->type == Token::AXIS_IDENTIFIER ) {
        //-- eat token
        lexer.nextToken();
        setDefaultAxis = MB_FALSE;

        //-- should switch to a hash here for speed if necessary
        if ( ANCESTOR_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::ANCESTOR_AXIS;
        else if ( ANCESTOR_OR_SELF_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::ANCESTOR_OR_SELF_AXIS;
        else if ( ATTRIBUTE_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::ATTRIBUTE_AXIS;
        else if ( CHILD_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::CHILD_AXIS;
        else if ( DESCENDANT_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::DESCENDANT_AXIS;
        else if ( DESCENDANT_OR_SELF_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::DESCENDANT_OR_SELF_AXIS;
        else if ( FOLLOWING_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::FOLLOWING_AXIS;
        else if ( FOLLOWING_SIBLING_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::FOLLOWING_SIBLING_AXIS;
        else if ( NAMESPACE_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::NAMESPACE_AXIS;
        else if ( PARENT_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::PARENT_AXIS;
        else if ( PRECEDING_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::PRECEDING_AXIS;
        else if ( PRECEDING_SIBLING_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::PRECEDING_SIBLING_AXIS;
        else if ( SELF_AXIS.isEqual(tok->value) )
            axisIdentifier = LocationStep::SELF_AXIS;
        //-- child axis is default
        else {
            //-- handle error gracefully, simply ignore invalid axis and
            //-- use default. Add error message when message observer
            //-- is implemented
            setDefaultAxis = MB_TRUE;
        }

    }

    //-- parse NodeExpr
    NodeExpr* nodeExpr = createNodeExpr(lexer);
    lstep->setNodeExpr(nodeExpr);


    //-- set default axis identifiers
    if ((setDefaultAxis) && (nodeExpr)) {
        switch ( nodeExpr->getType() ) {
            case NodeExpr::ATTRIBUTE_EXPR:
                axisIdentifier = LocationStep::ATTRIBUTE_AXIS;
                break;
            default:
                axisIdentifier = LocationStep::CHILD_AXIS;
        }
    }
    lstep->setAxisIdentifier(axisIdentifier);

    //-- handle predicates

    parsePredicates(lstep, lexer);

    //<debug>
    //String tmp;
    //lstep->toString(tmp);
    //cout << "returning LocationStep: "<< tmp <<endl;
    //</debug>

    return lstep;
} //-- createLocationPath

NodeExpr* ExprParser::createNodeExpr(ExprLexer& lexer) {

    //cout << "creating NodeExpr: "<<endl;
    if (!lexer.hasMoreTokens() )  cout << "Lexer has no Tokens"<<endl;

    if (!lexer.hasMoreTokens() )  return 0;

    NodeExpr* nodeExpr = 0;

    Token* tok = lexer.nextToken();
    //cout << "Token #" << tok->type <<endl;
    List params;

    String* errMsg = 0;

    switch ( tok->type ) {
        case Token::CNAME :
            nodeExpr = new ElementExpr(tok->value);
            break;
        case Token::WILD_CARD:
            nodeExpr = new WildCardExpr();
            break;
        case Token::COMMENT:
            nodeExpr = new BasicNodeExpr(NodeExpr::COMMENT_EXPR);
            errMsg = parseParameters(&params, lexer);
            //-- ignore errMsg for now
            delete errMsg;
            break;
        case Token::NODE :
            nodeExpr = new BasicNodeExpr();
            errMsg = parseParameters(&params, lexer);
            //-- ignore errMsg for now
            delete errMsg;
            break;
        case Token::PI :
            nodeExpr = new BasicNodeExpr(NodeExpr::PI_EXPR);
            errMsg = parseParameters(&params, lexer);
            //-- ignore errMsg for now
            delete errMsg;
            break;
        case Token::TEXT :
            nodeExpr = new TextExpr();
            errMsg = parseParameters(&params, lexer);
            //-- ignore errMsg for now
            delete errMsg;
            break;
        case Token::AT_SIGN:
            tok = lexer.nextToken();
            if ( !tok ) {
                //-- handle error
            }
            else if (tok->type == Token::CNAME) {
                nodeExpr = new AttributeExpr(tok->value);
            }
            else if ( tok->type == Token::WILD_CARD ) {
                AttributeExpr* attExpr = new AttributeExpr();
                attExpr->setWild(MB_TRUE);
                nodeExpr = attExpr;
            }
            else {
                //-- handle error
            }
            break;
        default:
            break;
    }
    return nodeExpr;
} //-- createNodeExpr

/**
 * Creates a PathExpr using the given ExprLexer
 * @param lexer the ExprLexer for retrieving Tokens
**/
PathExpr* ExprParser::createPathExpr(ExprLexer& lexer) {


    //-- check for RootExpr
    if ( lexer.countRemainingTokens() == 1 ) {
        if ( lexer.peek()->type == Token::PARENT_OP ) {
            lexer.nextToken(); //-- eat token
            return new RootExpr();
        }
    }

    PathExpr* pathExpr = new PathExpr();
    short ancestryOp = PathExpr::RELATIVE_OP;

    while ( lexer.hasMoreTokens() ) {
        Token* tok = lexer.nextToken();
        if ( lexer.isOperatorToken(tok) ) {
            lexer.pushBack();
            return pathExpr;
        }
        switch ( tok->type ) {
            case Token::R_PAREN:
            case Token::R_BRACKET:
            case Token::UNION_OP:
                lexer.pushBack();
                return pathExpr;
            case Token::ANCESTOR_OP :
                ancestryOp = PathExpr::ANCESTOR_OP;
                break;
            case Token::PARENT_OP :
                ancestryOp = PathExpr::PARENT_OP;
                break;
            default:
                lexer.pushBack();
                pathExpr->addPatternExpr(createPatternExpr(lexer), ancestryOp);
                ancestryOp = PathExpr::RELATIVE_OP;
                break;
        }
    }

    /* <debug> *
    String tmp;
    pathExpr->toString(tmp);
    cout << "creating pathExpr: " << tmp << endl;
    /* </debug> */

    return pathExpr;
} //-- createPathExpr

/**
 * Creates a PatternExpr using the given ExprLexer
 * @param lexer the ExprLexer for retrieving Tokens
**/
PatternExpr* ExprParser::createPatternExpr(ExprLexer& lexer) {

    PatternExpr* pExpr = 0;
    Token* tok = lexer.peek();
    if ( isLocationStepToken(tok) ) {
        pExpr = createLocationStep(lexer);
    }
    else if ( isFilterExprToken(tok) ) {
        pExpr = createFilterExpr(lexer);
    }
    else {
        cout << "invalid token: " << tok->value << endl;
        //-- eat token for now
        lexer.nextToken();
    }
    return pExpr;
} //-- createPatternExpr

/**
 * Creates a PathExpr using the given ExprLexer
 * @param lexer the ExprLexer for retrieving Tokens
**/
UnionExpr* ExprParser::createUnionExpr(ExprLexer& lexer) {
    UnionExpr* unionExpr = new UnionExpr();

    while ( lexer.hasMoreTokens() ) {
        Token* tok = lexer.nextToken();
        switch ( tok->type ) {

            case Token::R_PAREN:
            case Token::R_BRACKET:
                lexer.pushBack();
                return unionExpr;
            case Token::UNION_OP :
                //-- eat token
                break;
            default:
                lexer.pushBack();
                unionExpr->addPathExpr(createPathExpr(lexer));
                break;
        }
    }
    //String tmp;
    //unionExpr->toString(tmp);
    //cout << "creating UnionExpr: " << tmp << endl;
    return unionExpr;
} //-- createUnionExpr

MBool ExprParser::isFilterExprToken(Token* token) {
    if ( !token ) return MB_FALSE;
    switch (token->type) {
        case Token::LITERAL:
        case Token::NUMBER:
        case Token::FUNCTION_NAME:
        case Token::VAR_REFERENCE:
        case Token::L_PAREN:            // grouping expr
        case Token::PARENT_NODE:
        case Token::SELF_NODE :
            return MB_TRUE;
        default:
            return MB_FALSE;
    }
} //-- isFilterExprToken

MBool ExprParser::isLocationStepToken(Token* token) {
    if (!token) return MB_FALSE;
    return ((token->type == Token::AXIS_IDENTIFIER) || isNodeTypeToken(token));
} //-- isLocationStepToken

MBool ExprParser::isNodeTypeToken(Token* token) {
    if (!token) return MB_FALSE;

    switch ( token->type ) {
        case Token::AT_SIGN:
        case Token::CNAME:
        case Token::WILD_CARD:
        case Token::COMMENT:
        case Token::NODE :
        case Token::PI :
        case Token::TEXT :
            return MB_TRUE;
        default:
            return MB_FALSE;
    }
} //-- isLocationStepToken

/**
 * Using the given lexer, parses the tokens if they represent a predicate list
 * If an error occurs a non-zero String pointer will be returned containing the
 * error message.
 * @param predicateList, the PredicateList to add predicate expressions to
 * @param lexer the ExprLexer to use for parsing tokens
 * @return 0 if successful, or a String pointer to the error message
**/
String* ExprParser::parsePredicates(PredicateList* predicateList, ExprLexer& lexer) {

    String* errorMsg = 0;

    Token* tok = lexer.peek();

    if ( !tok ) return 0; //-- no predicates
    if ( tok->type != Token::L_BRACKET ) return 0; //-- not start of predicate list

    lexer.nextToken();

    while ( lexer.hasMoreTokens() ) {
        tok = lexer.peek();
        if(!tok) {
            //-- error missing ']'
            errorMsg = new String("missing close of predicate expression ']'");
            break;
        }
        if ( tok->type == Token::R_BRACKET) {
            lexer.nextToken(); //-- eat ']'


            //-- Fix: look ahead at next token for mulitple predicates - Marina M.
            tok = lexer.peek();
            if ((!tok) || ( tok->type != Token::L_BRACKET )) break;
            //-- /Fix
        }

        //-- Fix: handle multiple predicates - Marina M.
        if (tok->type == Token::L_BRACKET)
            lexer.nextToken(); //-- swallow '['
        //-- /Fix

        Expr* expr = createExpr(lexer);
        predicateList->add(expr);
    }
    return errorMsg;

} //-- parsePredicates


/**
 * Using the given lexer, parses the tokens if they represent a parameter list
 * If an error occurs a non-zero String pointer will be returned containing the
 * error message.
 * @param list, the List to add parameter expressions to
 * @param lexer the ExprLexer to use for parsing tokens
 * @return 0 if successful, or a String pointer to the error message
**/
String* ExprParser::parseParameters(List* list, ExprLexer& lexer) {

    String* errorMsg = 0;

    Token* tok = lexer.peek();

    if ( !tok ) return 0; //-- no params
    if ( tok->type != Token::L_PAREN ) return 0; //-- not start of param list

    lexer.nextToken(); //-- eat L_PAREN

    MBool done     = MB_FALSE;
    MBool foundSep = MB_FALSE;

    while ( lexer.hasMoreTokens() && !done) {
        tok = lexer.peek();
        switch ( tok->type ) {
            case Token::R_PAREN :
                if (foundSep) errorMsg = new String("missing expression after ','");
                lexer.nextToken(); //-- eat R_PAREN
                done = MB_TRUE;
                break;
            case Token::COMMA: //-- param separator
                //-- eat COMMA
                lexer.nextToken();
                foundSep = MB_TRUE;
                break;
            default:
                if ((list->getLength() > 0) && (!foundSep)) {
                    errorMsg = new String("missing ',' or ')'");
                    done = MB_TRUE;
                    break;
                }
                foundSep = MB_FALSE;
                Expr* expr = createExpr(lexer);
                list->add(expr);
                break;
        }
    }
    return errorMsg;

} //-- parseParameters

short ExprParser::precedenceLevel(short tokenType) {
    switch(tokenType) {
        case Token::OR_OP:
            return 1;
        case Token::AND_OP:
            return 2;
        //-- equality
        case Token::EQUAL_OP:
        case Token::NOT_EQUAL_OP:
            return 3;
        //-- relational
        case Token::LESS_THAN_OP:
        case Token::GREATER_THAN_OP:
        case Token::LESS_OR_EQUAL_OP:
        case Token::GREATER_OR_EQUAL_OP:
            return 4;
        //-- additive operators
        case Token::ADDITION_OP:
        case Token::SUBTRACTION_OP:
            return 5;
        //-- multiplicative
        case Token::DIVIDE_OP:
        case Token::MULTIPLY_OP:
        case Token::MODULUS_OP:
            return 6;
        default:
            break;
    }
    return 0;
} //-- precedenceLevel

