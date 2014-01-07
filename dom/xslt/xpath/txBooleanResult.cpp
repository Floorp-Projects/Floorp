/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Boolean Expression result
*/

#include "txExprResult.h"

/**
 * Creates a new BooleanResult with the value of the given bool parameter
 * @param boolean the bool to use for initialization of this BooleanResult's value
**/
BooleanResult::BooleanResult(bool boolean)
    : txAExprResult(nullptr)
{
    this->value = boolean;
} //-- BooleanResult

/*
 * Virtual Methods from ExprResult
*/

short BooleanResult::getResultType() {
    return txAExprResult::BOOLEAN;
} //-- getResultType

void
BooleanResult::stringValue(nsString& aResult)
{
    if (value) {
        aResult.AppendLiteral("true");
    }
    else {
        aResult.AppendLiteral("false");
    }
}

const nsString*
BooleanResult::stringValuePointer()
{
    // In theory we could set strings containing "true" and "false" somewhere,
    // but most stylesheets never get the stringvalue of a bool so that won't
    // really buy us anything.
    return nullptr;
}

bool BooleanResult::booleanValue() {
   return this->value;
} //-- toBoolean

double BooleanResult::numberValue() {
    return ( value ) ? 1.0 : 0.0;
} //-- toNumber
