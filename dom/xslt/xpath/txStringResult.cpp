/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * StringResult
 * Represents a String as a Result of evaluating an Expr
**/
#include "txExprResult.h"

/**
 * Default Constructor
**/
StringResult::StringResult(txResultRecycler* aRecycler)
    : txAExprResult(aRecycler)
{
}

/**
 * Creates a new StringResult with the value of the given String parameter
 * @param str the String to use for initialization of this StringResult's value
**/
StringResult::StringResult(const nsAString& aValue, txResultRecycler* aRecycler)
    : txAExprResult(aRecycler), mValue(aValue)
{
}

/*
 * Virtual Methods from ExprResult
*/

short StringResult::getResultType() {
    return txAExprResult::STRING;
} //-- getResultType

void
StringResult::stringValue(nsString& aResult)
{
    aResult.Append(mValue);
}

const nsString*
StringResult::stringValuePointer()
{
    return &mValue;
}

bool StringResult::booleanValue() {
   return !mValue.IsEmpty();
} //-- booleanValue

double StringResult::numberValue() {
    return txDouble::toDouble(mValue);
} //-- numberValue

