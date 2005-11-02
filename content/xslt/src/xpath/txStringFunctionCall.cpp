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
 *
 * $Id: txStringFunctionCall.cpp,v 1.5 2005/11/02 07:33:40 axel%pike.org Exp $
 */

/**
 * StringFunctionCall
 * A representation of the XPath String funtions
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.5 $ $Date: 2005/11/02 07:33:40 $
**/

#include "FunctionLib.h"

/**
 * Creates a default StringFunctionCall. The string() function
 * is the default
**/
StringFunctionCall::StringFunctionCall() : FunctionCall(XPathNames::STRING_FN) {
    type = STRING;
} //-- StringFunctionCall

/**
 * Creates a StringFunctionCall of the given type
**/
StringFunctionCall::StringFunctionCall(short type) : FunctionCall() {
    this->type = type;
    switch ( type ) {
        case CONCAT:
            FunctionCall::setName(XPathNames::CONCAT_FN);
            break;
        case CONTAINS:
            FunctionCall::setName(XPathNames::CONTAINS_FN);
            break;
        case STARTS_WITH:
            FunctionCall::setName(XPathNames::STARTS_WITH_FN);
            break;
        case STRING_LENGTH:
            FunctionCall::setName(XPathNames::STRING_LENGTH_FN);
            break;
        case SUBSTRING:
            FunctionCall::setName(XPathNames::SUBSTRING_FN);
            break;
        case SUBSTRING_AFTER:
            FunctionCall::setName(XPathNames::SUBSTRING_AFTER_FN);
            break;
        case SUBSTRING_BEFORE:
            FunctionCall::setName(XPathNames::SUBSTRING_BEFORE_FN);
            break;
        case TRANSLATE:
            FunctionCall::setName(XPathNames::TRANSLATE_FN);
            break;
        default:
            FunctionCall::setName(XPathNames::STRING_FN);
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
                    if (dbl == Double::POSITIVE_INFINITY) ++endIdx;
                    else if ( dbl == Double::NEGATIVE_INFINITY ) endIdx = 0;
                    else endIdx = (Int32)floor(dbl);
                }
                String resultStr;
                //-- strings are indexed starting at 1 for XSL
                //-- adjust to a 0-based index
                if (startIdx > 0) --startIdx;
                else if (startIdx == 0 ) --endIdx;
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
                UNICODE_CHAR* chars = src.toUnicode(new UNICODE_CHAR[size]);
                src.clear();
                Int32 newIdx = 0;
                Int32 i;
                for (i = 0; i < size; i++) {
                    Int32 idx = oldChars.indexOf(chars[i]);
                    if (idx >= 0) {
                        char nchar = newChars.charAt(idx);
                        if (nchar != -1) src.append(nchar);
                    }
                    else src.append(chars[i]);
                }
                delete chars;
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
                    XMLDOMUtils::getNodeValue(context, &resultStr);
                    if ( cs->isStripSpaceAllowed(context) &&
                         XMLUtils::shouldStripTextnode(resultStr))
                        resultStr = "";
                }
                result = new StringResult(resultStr);
            }
            else result = new StringResult("");
            break;
    }
    delete iter;
    return result;
} //-- evaluate

