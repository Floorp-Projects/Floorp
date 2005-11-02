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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#ifndef TRANSFRMX_OUTPUTFORMAT_H
#define TRANSFRMX_OUTPUTFORMAT_H

#include "List.h"
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
