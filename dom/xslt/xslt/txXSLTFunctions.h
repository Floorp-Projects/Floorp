/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_XSLT_FUNCTIONS_H
#define TRANSFRMX_XSLT_FUNCTIONS_H

#include "txExpr.h"
#include "txXMLUtils.h"
#include "nsAutoPtr.h"
#include "txNamespaceMap.h"

class txPattern;
class txStylesheet;
class txKeyValueHashKey;
class txExecutionState;

/**
 * The definition for the XSLT document() function
**/
class DocumentFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new document() function call
    **/
    explicit DocumentFunctionCall(const nsAString& aBaseURI);

    TX_DECL_FUNCTION

private:
    nsString mBaseURI;
};

/*
 * The definition for the XSLT key() function
 */
class txKeyFunctionCall : public FunctionCall {

public:

    /*
     * Creates a new key() function call
     */
    explicit txKeyFunctionCall(txNamespaceMap* aMappings);

    TX_DECL_FUNCTION

private:
    nsRefPtr<txNamespaceMap> mMappings;
};

/**
 * The definition for the XSLT format-number() function
**/
class txFormatNumberFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new format-number() function call
    **/
    txFormatNumberFunctionCall(txStylesheet* aStylesheet, txNamespaceMap* aMappings);

    TX_DECL_FUNCTION

private:
    static const char16_t FORMAT_QUOTE;

    enum FormatParseState {
        Prefix,
        IntDigit,
        IntZero,
        FracZero,
        FracDigit,
        Suffix,
        Finished
    };
    
    txStylesheet* mStylesheet;
    nsRefPtr<txNamespaceMap> mMappings;
};

/**
 * DecimalFormat
 * A representation of the XSLT element <xsl:decimal-format>
 */
class txDecimalFormat {

public:
    /*
     * Creates a new decimal format and initilizes all properties with
     * default values
     */
    txDecimalFormat();
    bool isEqual(txDecimalFormat* other);
    
    char16_t       mDecimalSeparator;
    char16_t       mGroupingSeparator;
    nsString        mInfinity;
    char16_t       mMinusSign;
    nsString        mNaN;
    char16_t       mPercent;
    char16_t       mPerMille;
    char16_t       mZeroDigit;
    char16_t       mDigit;
    char16_t       mPatternSeparator;
};

/**
 * The definition for the XSLT current() function
**/
class CurrentFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new current() function call
    **/
    CurrentFunctionCall();

    TX_DECL_FUNCTION
};

/**
 * The definition for the XSLT generate-id() function
**/
class GenerateIdFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new generate-id() function call
    **/
    GenerateIdFunctionCall();

    TX_DECL_FUNCTION
};


/**
 * A system-property(), element-available() or function-available() function.
 */
class txXSLTEnvironmentFunctionCall : public FunctionCall
{
public:
    enum eType { SYSTEM_PROPERTY, ELEMENT_AVAILABLE, FUNCTION_AVAILABLE };

    txXSLTEnvironmentFunctionCall(eType aType, txNamespaceMap* aMappings)
        : mType(aType),
          mMappings(aMappings)
    {
    }

    TX_DECL_FUNCTION

private:
    eType mType;
    nsRefPtr<txNamespaceMap> mMappings; // Used to resolve prefixes
};

#endif
