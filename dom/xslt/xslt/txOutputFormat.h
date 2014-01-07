/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_OUTPUTFORMAT_H
#define TRANSFRMX_OUTPUTFORMAT_H

#include "txList.h"
#include "nsString.h"

enum txOutputMethod {
    eMethodNotSet,
    eXMLOutput,
    eHTMLOutput,
    eTextOutput
};

enum txThreeState {
    eNotSet,
    eFalse,
    eTrue
};

class txOutputFormat {
public:
    txOutputFormat();
    ~txOutputFormat();

    // "Unset" all values
    void reset();

    // Merges in the values of aOutputFormat, members that already
    // have a value in this txOutputFormat will not be changed.
    void merge(txOutputFormat& aOutputFormat);

    // Sets members that have no value to their default value.
    void setFromDefaults();

    // The XSLT output method, which can be "xml", "html", or "text"
    txOutputMethod mMethod;

    // The xml version number that should be used when serializing
    // xml documents
    nsString mVersion;

    // The XML character encoding that should be used when serializing
    // xml documents
    nsString mEncoding;

    // Signals if we should output an XML declaration
    txThreeState mOmitXMLDeclaration;

    // Signals if we should output a standalone document declaration
    txThreeState mStandalone;

    // The public Id for creating a DOCTYPE
    nsString mPublicId;

    // The System Id for creating a DOCTYPE
    nsString mSystemId;

    // The elements whose text node children should be output as CDATA
    txList mCDATASectionElements;

    // Signals if output should be indented
    txThreeState mIndent;

    // The media type of the output
    nsString mMediaType;
};

#endif
