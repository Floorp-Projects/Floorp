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

#ifndef TRANSFRMX_FUNCTIONLIB_H
#define TRANSFRMX_FUNCTIONLIB_H

#include "Expr.h"
#include "nsIAtom.h"

/**
 * The following are definitions for the XPath functions
 *
 * <PRE>
 * Modifications:
 * 20000418: Keith Visco
 *   -- added ExtensionFunctionCall
 *
 * 19990805: Keith Visco
 *   - added NodeSetFunctionCall
 *   - moved position() function into NodeSetFunctionCall
 *   - removed PositionFunctionCall
 * 19990806: Larry Fitzpatrick
 *   - changed constant short declarations for BooleanFunctionCall
 *     with enumerations
 * 19990806: Keith Visco
 *   - added StringFunctionCall
 *   - stated using Larry's enum suggestion instead of using static const shorts,
 *     as you can see, I am a Java developer! ;-)
 * </PRE>
 */

/**
 * Represents the Set of boolean functions
**/
class BooleanFunctionCall : public FunctionCall {

public:

    enum BooleanFunctions {
        TX_BOOLEAN,  // boolean()
        TX_FALSE,    // false()
        TX_LANG,     // lang()
        TX_NOT,      // not()
        TX_TRUE      // true()
    };

    /**
     * Creates a BooleanFunctionCall of the given type
    **/
    BooleanFunctionCall(BooleanFunctions aType);

    TX_DECL_FUNCTION;

private:
    BooleanFunctions mType;
}; //-- BooleanFunctionCall

/**
 * Error Function to be used for unknown extension functions and
 * forwards-compatible-processing (not implemented, bug XXX)
 *
 * txErrorFunctionCall returns NS_ERROR_XPATH_UNKNOWN_FUNCTION
 * (see FunctionCall.cpp)
 */
class txErrorFunctionCall : public FunctionCall
{
public:
    txErrorFunctionCall(nsIAtom* aLName, const PRInt32 aID)
        : mLName(aLName),
          mID(aID)
    {}

    TX_DECL_FUNCTION;

private:
    nsCOMPtr<nsIAtom> mLName;
    PRInt32 mID;
};

/*
 * A representation of the XPath NodeSet funtions
 */
class NodeSetFunctionCall : public FunctionCall {

public:

    enum NodeSetFunctions {
        COUNT,          // count()
        ID,             // id()
        LAST,           // last()
        LOCAL_NAME,     // local-name()
        NAMESPACE_URI,  // namespace-uri()
        NAME,           // name()
        POSITION        // position()
    };

    /*
     * Creates a NodeSetFunctionCall of the given type
     */
    NodeSetFunctionCall(NodeSetFunctions aType);

    TX_DECL_FUNCTION;

private:
    NodeSetFunctions mType;
};


/**
 * Represents the XPath String Function Calls
**/
class StringFunctionCall : public FunctionCall {

public:

    enum StringFunctions {
        CONCAT,            // concat()
        CONTAINS,          // contains()
        NORMALIZE_SPACE,   // normalize-space()
        STARTS_WITH,       // starts-with()
        STRING,            // string()
        STRING_LENGTH,     // string-length()
        SUBSTRING,         // substring()
        SUBSTRING_AFTER,   // substring-after()
        SUBSTRING_BEFORE,  // substring-before()
        TRANSLATE          // translate()
    };

    /**
     * Creates a String function of the given type
    **/
    StringFunctionCall(StringFunctions aType);

    TX_DECL_FUNCTION;

private:
    StringFunctions mType;
}; //-- StringFunctionCall


/*
 * Represents the XPath Number Function Calls
 */
class NumberFunctionCall : public FunctionCall {

public:

    enum NumberFunctions {
        NUMBER,   // number()
        ROUND,    // round()
        FLOOR,    // floor()
        CEILING,  // ceiling()
        SUM       // sum()
    };

    /*
     * Creates a Number function of the given type
     */
    NumberFunctionCall(NumberFunctions aType);

    TX_DECL_FUNCTION;

private:
    NumberFunctions mType;
};

#endif
