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
 * NumberResult
 * Represents the a number as the result of evaluating an Expr
**/

#include "ExprResult.h"

/**
 * Default Constructor
**/
NumberResult::NumberResult() {
    value = 0.0;
} //-- NumberResult

/**
 * Creates a new NumberResult with the value of the given double parameter
 * @param dbl the double to use for initialization of this NumberResult's value
**/
NumberResult::NumberResult(double dbl) {
    this->value = dbl;
} //-- NumberResult

/*
 * Virtual Methods from ExprResult
*/

short NumberResult::getResultType() {
    return ExprResult::NUMBER;
} //-- getResultType

void NumberResult::stringValue(String& str)  {
    Double::toString(value, str);
} //-- stringValue

MBool NumberResult::booleanValue() {
  // OG+
  // As per the XPath spec, the boolean value of a number is true if and only if
  // it is neither positive 0 nor negative 0 nor NaN
  return (MBool)(value != 0.0 && !Double::isNaN(value));
  // OG-
} //-- booleanValue

double NumberResult::numberValue() {
    return this->value;
} //-- numberValue

