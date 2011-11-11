/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Boolean Expression result
*/

#include "txExprResult.h"

/**
 * Creates a new BooleanResult with the value of the given bool parameter
 * @param boolean the bool to use for initialization of this BooleanResult's value
**/
BooleanResult::BooleanResult(bool boolean)
    : txAExprResult(nsnull)
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
    return nsnull;
}

bool BooleanResult::booleanValue() {
   return this->value;
} //-- toBoolean

double BooleanResult::numberValue() {
    return ( value ) ? 1.0 : 0.0;
} //-- toNumber
