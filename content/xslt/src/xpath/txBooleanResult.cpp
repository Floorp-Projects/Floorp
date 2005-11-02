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
 * $Id: txBooleanResult.cpp,v 1.2 2005/11/02 07:33:47 Peter.VanderBeken%pandora.be Exp $
 */

/**
 * Boolean Expression result
 * @author <A href="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.2 $ $Date: 2005/11/02 07:33:47 $
**/

#include "ExprResult.h"

/**
 * Default Constructor
**/
BooleanResult::BooleanResult() {
    value = MB_FALSE;
} //-- BooleanResult

BooleanResult::BooleanResult(const BooleanResult& boolResult) {
    this->value = boolResult.getValue();
} //-- BooleanResult

/**
 * Creates a new BooleanResult with the value of the given MBool parameter
 * @param boolean the MBool to use for initialization of this BooleanResult's value
**/
BooleanResult::BooleanResult(MBool boolean) {
    this->value = boolean;
} //-- BooleanResult

/**
 * Returns the value of this BooleanResult
 * @return the value of this BooleanResult
**/
MBool BooleanResult::getValue() const {
    return this->value;
} //-- getValue

/**
 * Sets the value of this BooleanResult
 * @param boolean the MBool to use for this BooleanResult's value
**/
void BooleanResult::setValue(MBool boolean) {
    this->value = boolean;
} //-- setValue

/**
 * Sets the value of this BooleanResult
 * @param boolResult the BooleanResult to use for setting this BooleanResult's value
**/
void BooleanResult::setValue(const BooleanResult& boolResult) {
    this->value = boolResult.getValue();
} //-- setValue

/*
 * Virtual Methods from ExprResult
*/

short BooleanResult::getResultType() {
    return ExprResult::BOOLEAN;
} //-- getResultType

void BooleanResult::stringValue(DOMString& str)  {
    if ( value ) str.append("true");
    else str.append("false");
} //-- toString

MBool BooleanResult::booleanValue() {
   return this->value;
} //-- toBoolean

double BooleanResult::numberValue() {
    return ( value ) ? 1.0 : 0.0;
} //-- toNumber
