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
 * NumberResult
 * Represents the a number as the result of evaluating an Expr
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
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
   return (MBool)(this->value != 0.0);
} //-- booleanValue

double NumberResult::numberValue() {
    return this->value;
} //-- numberValue

