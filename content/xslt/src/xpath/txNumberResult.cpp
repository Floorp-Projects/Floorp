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
 * $Id: txNumberResult.cpp,v 1.1 2005/11/02 07:33:56 kvisco%ziplink.net Exp $
 */

/**
 * NumberResult
 * Represents the a number as the result of evaluating an Expr
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.1 $ $Date: 2005/11/02 07:33:56 $
**/

#include "ExprResult.h"

/**
 * Default Constructor
**/
NumberResult::NumberResult() {
    value = 0.0;
} //-- NumberResult

NumberResult::NumberResult(const NumberResult& nbrResult) {
    this->value = nbrResult.getValue();
} //-- NumberResult

/**
 * Creates a new NumberResult with the value of the given double parameter
 * @param dbl the double to use for initialization of this NumberResult's value
**/
NumberResult::NumberResult(double dbl) {
    this->value = dbl;
} //-- NumberResult

/**
 * Returns the value of this NumberResult
 * @return the value of this NumberResult
**/
double NumberResult::getValue() const {
    return this->value;
} //-- getValue

/**
 *
**/
MBool NumberResult::isNaN() const {
    return Double::isNaN(value);
} //-- isNaN
/**
 * Sets the value of this NumberResult
 * @param dbl the double to use for this NumberResult's value
**/
void NumberResult::setValue(double dbl) {
    this->value = dbl;
} //-- setValue

/**
 * Sets the value of this NumberResult
 * @param nbrResult the NumberResult to use for setting this NumberResult's value
**/
void NumberResult::setValue(const NumberResult& nbrResult) {
    this->value = nbrResult.getValue();
} //-- setValue

/*
 * Virtual Methods from ExprResult
*/

short NumberResult::getResultType() {
    return ExprResult::NUMBER;
} //-- getResultType

void NumberResult::stringValue(String& str)  {
    int intVal = (int)value;
    if (intVal == value) { //-- no fraction
        Integer::toString(intVal, str);
    }
    else Double::toString(value, str);
} //-- stringValue

MBool NumberResult::booleanValue() {
  // OG+
  // As per the XPath spec, the boolean value of a number is true if and only if
  // it is neither positive 0 nor negative 0 nor NaN
  return (MBool)(this->value != 0.0 && this->value != -0.0 && ! isNaN());
  // OG-
} //-- booleanValue

double NumberResult::numberValue() {
    return this->value;
} //-- numberValue

