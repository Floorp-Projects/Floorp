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
 * Boolean Expression result
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
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
