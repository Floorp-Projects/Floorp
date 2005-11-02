/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 */

/**
 * StringResult
 * Represents a String as a Result of evaluating an Expr
**/
#include "ExprResult.h"

/**
 * Default Constructor
**/
StringResult::StringResult() {
} //-- StringResult

/**
 * Creates a new StringResult with the value of the given String parameter
 * @param str the String to use for initialization of this StringResult's value
**/
StringResult::StringResult(const String& str) {
    //-- copy str
    this->value = str;
} //-- StringResult

/**
 * Creates a new StringResult with the value of the given String parameter
 * @param str the String to use for initialization of this StringResult's value
**/
StringResult::StringResult(const char* str) {
    //-- copy str
    this->value = str;
} //-- StringResult

/*
 * Virtual Methods from ExprResult
*/

short StringResult::getResultType() {
    return ExprResult::STRING;
} //-- getResultType

void StringResult::stringValue(String& str)  {
    str.append(this->value);
} //-- stringValue

MBool StringResult::booleanValue() {
   return !value.isEmpty();
} //-- booleanValue

double StringResult::numberValue() {
    return Double::toDouble(value);
} //-- numberValue

