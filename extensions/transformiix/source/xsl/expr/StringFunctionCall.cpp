/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * StringFunctionCall
 * A representation of the XPath String funtions
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "FunctionLib.h"

/**
 * Creates a default StringFunctionCall. The string() function
 * is the default
**/
StringFunctionCall::StringFunctionCall() : FunctionCall(STRING_FN) {
    type = STRING;
} //-- StringFunctionCall

/**
 * Creates a StringFunctionCall of the given type
**/
StringFunctionCall::StringFunctionCall(short type) : FunctionCall() {
    this->type = type;
    switch ( type ) {
        case CONCAT:
            FunctionCall::setName(CONCAT_FN);
            break;
        case CONTAINS:
            FunctionCall::setName(CONTAINS_FN);
            break;
        case STARTS_WITH:
            FunctionCall::setName(STARTS_WITH_FN);
            break;
        case STRING_LENGTH:
            FunctionCall::setName(STRING_LENGTH_FN);
            break;
        case SUBSTRING:
            FunctionCall::setName(SUBSTRING_FN);
            break;
        case SUBSTRING_AFTER:
            FunctionCall::setName(SUBSTRING_AFTER_FN);
            break;
        case SUBSTRING_BEFORE:
            FunctionCall::setName(SUBSTRING_BEFORE_FN);
            break;
        case TRANSLATE:
            FunctionCall::setName(TRANSLATE_FN);
            break;
        default:
            FunctionCall::setName(STRING_FN);
            break;
    }
} //-- StringFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* StringFunctionCall::evaluate(Node* context, ContextState* cs) {
    ListIterator* iter = params.iterator();
    Int32 argc = params.getLength();
    String err;
    ExprResult* result = 0;
    switch ( type ) {
        case CONCAT :
            if ( requireParams(2, cs) ) {
                String resultStr;
                while(iter->hasNext()) {
                    evaluateToString((Expr*)iter->next(),context, cs, resultStr);
                }
                result  = new StringResult(resultStr);
            }
            else result = new StringResult("");
            break;
        case CONTAINS :
            if ( requireParams(2, 2, cs) ) {
                String arg1, arg2;
                evaluateToString((Expr*)iter->next(),context, cs, arg1);
                evaluateToString((Expr*)iter->next(),context, cs, arg2);
                result  = new BooleanResult((MBool)(arg1.indexOf(arg2) >= 0));
            }
            else result = new BooleanResult(MB_FALSE);
            break;

        case STARTS_WITH :
            if ( requireParams(2, 2, cs) ) {
                String arg1, arg2;
                evaluateToString((Expr*)iter->next(),context, cs, arg1);
                evaluateToString((Expr*)iter->next(),context, cs, arg2);
                result  = new BooleanResult((MBool)(arg1.indexOf(arg2) == 0));
            }
            else result = new BooleanResult(MB_FALSE);
            break;
        case STRING_LENGTH:
            if ( requireParams(0, 1, cs) ) {
                String resultStr;
                if ( argc == 1) {
                    evaluateToString((Expr*)iter->next(),context, cs, resultStr);
                }
                else XMLDOMUtils::getNodeValue(context, &resultStr);
                result  = new NumberResult( (double) resultStr.length());
            }
            else result = new NumberResult(0.0);
            break;
        case SUBSTRING:
            if ( requireParams(2, 3, cs) ) {
                String src;
                evaluateToString((Expr*)iter->next(),context, cs, src);
                double dbl = evaluateToNumber((Expr*)iter->next(), context, cs);

                //-- check for NaN
                if ( Double::isNaN(dbl)) {
                    result = new StringResult("");
                    break;
                }

                Int32 startIdx = (Int32)ceil(dbl);
                Int32 endIdx = src.length();
                if ( argc == 3) {
                    dbl += evaluateToNumber((Expr*)iter->next(),context, cs);
                    if (dbl == Double::POSITIVE_INFINITY) endIdx++;
                    else if ( dbl == Double::NEGATIVE_INFINITY ) endIdx = 0;
                    else endIdx = (Int32)floor(dbl);
                }
                String resultStr;
                //-- strings are indexed starting at 1 for XSL
                //-- adjust to a 0-based index
                if (startIdx > 0) startIdx--;
                else if (startIdx == 0 ) endIdx--;
                else startIdx=0;
                src.subString(startIdx,endIdx,resultStr);
                result  = new StringResult(resultStr);
            }
            else result = new StringResult("");
            break;

        case SUBSTRING_AFTER:
            if ( requireParams(2, 2, cs) ) {
                String arg1, arg2;
                evaluateToString((Expr*)iter->next(),context, cs, arg1);
                evaluateToString((Expr*)iter->next(),context, cs, arg2);
                Int32 idx = arg1.indexOf(arg2);
                if ((idx >= 0)&&(idx<arg1.length())) {
                    Int32 len = arg2.length();
                    arg2.clear();
                    arg1.subString(idx+len,arg2);
                    result  = new StringResult(arg2);
                    break;
                }
            }
            result = new StringResult("");
            break;
        case SUBSTRING_BEFORE:
            if ( requireParams(2, 2, cs) ) {
                String arg1, arg2;
                evaluateToString((Expr*)iter->next(),context, cs, arg1);
                evaluateToString((Expr*)iter->next(),context, cs, arg2);
                Int32 idx = arg1.indexOf(arg2);
                if ((idx >= 0)&&(idx<arg1.length())) {
                    arg2.clear();
                    arg1.subString(0,idx,arg2);
                    result  = new StringResult(arg2);
                    break;
                }
            }
            result = new StringResult("");
            break;
        case TRANSLATE:
            if ( requireParams(3, 3, cs) ) {
                String src, oldChars, newChars;
                evaluateToString((Expr*)iter->next(),context, cs, src);
                evaluateToString((Expr*)iter->next(),context, cs, oldChars);
                evaluateToString((Expr*)iter->next(),context, cs, newChars);
                Int32 size = src.length();
                char* chars = new char[size+1];
                src.toChar(chars);
                src.clear();
                Int32 newIdx = 0;                
                for (Int32 i = 0; i < size; i++) {
                    Int32 idx = oldChars.indexOf(chars[i]);
                    if (idx >= 0) {
                        UNICODE_CHAR nchar = newChars.charAt(idx);
                        if (nchar != -1) src.append(nchar);
                    }
                    else src.append(chars[i]);
                }
                return new StringResult(src);
            }
            result = new StringResult("");
            break;

        default : //-- string( object? )
            if ( requireParams(0, 1, cs) ) {
                String resultStr;
                if (iter->hasNext()) {
                    evaluateToString((Expr*)iter->next(),context, cs, resultStr);
                }
                else {
                    String temp;
                    XMLDOMUtils::getNodeValue(context, &temp);
                    if ( cs->isStripSpaceAllowed(context) ) {
                        XMLUtils::stripSpace(temp, resultStr);
                    }
                    else resultStr.append(temp);
                }
                result = new StringResult(resultStr);
            }
            else result = new StringResult("");
            break;
    }
    delete iter;
    return result;
} //-- evaluate

