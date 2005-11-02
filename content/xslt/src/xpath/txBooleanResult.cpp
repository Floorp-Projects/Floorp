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
 * $Id: txBooleanResult.cpp,v 1.7 2005/11/02 07:33:52 sicking%bigfoot.com Exp $
 */

/*
 * Boolean Expression result
*/

#include "ExprResult.h"

/**
 * Default Constructor
**/
BooleanResult::BooleanResult() {
    value = MB_FALSE;
} //-- BooleanResult

/**
 * Creates a new BooleanResult with the value of the given MBool parameter
 * @param boolean the MBool to use for initialization of this BooleanResult's value
**/
BooleanResult::BooleanResult(MBool boolean) {
    this->value = boolean;
} //-- BooleanResult

/*
 * Virtual Methods from ExprResult
*/

short BooleanResult::getResultType() {
    return ExprResult::BOOLEAN;
} //-- getResultType

void BooleanResult::stringValue(String& str)  {
    if ( value ) str.append("true");
    else str.append("false");
} //-- toString

MBool BooleanResult::booleanValue() {
   return this->value;
} //-- toBoolean

double BooleanResult::numberValue() {
    return ( value ) ? 1.0 : 0.0;
} //-- toNumber
