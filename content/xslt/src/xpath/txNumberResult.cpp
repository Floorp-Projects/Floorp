/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NumberResult
 * Represents the a number as the result of evaluating an Expr
**/

#include "mozilla/FloatingPoint.h"

#include "txExprResult.h"

/**
 * Default Constructor
**/

/**
 * Creates a new NumberResult with the value of the given double parameter
 * @param dbl the double to use for initialization of this NumberResult's value
**/
NumberResult::NumberResult(double aValue, txResultRecycler* aRecycler)
    : txAExprResult(aRecycler), value(aValue)
{
} //-- NumberResult

/*
 * Virtual Methods from ExprResult
*/

short NumberResult::getResultType() {
    return txAExprResult::NUMBER;
} //-- getResultType

void
NumberResult::stringValue(nsString& aResult)
{
    txDouble::toString(value, aResult);
}

const nsString*
NumberResult::stringValuePointer()
{
    return nullptr;
}

bool NumberResult::booleanValue() {
  // OG+
  // As per the XPath spec, the boolean value of a number is true if and only if
  // it is neither positive 0 nor negative 0 nor NaN
  return (bool)(value != 0.0 && !MOZ_DOUBLE_IS_NaN(value));
  // OG-
} //-- booleanValue

double NumberResult::numberValue() {
    return this->value;
} //-- numberValue

