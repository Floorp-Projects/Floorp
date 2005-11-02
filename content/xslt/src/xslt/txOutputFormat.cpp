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
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
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

#include "txOutputFormat.h"
#include "XMLUtils.h"

txOutputFormat::txOutputFormat() : mMethod(eMethodNotSet),
                                   mOmitXMLDeclaration(eNotSet),
                                   mStandalone(eNotSet),
                                   mIndent(eNotSet)
{
}

txOutputFormat::~txOutputFormat()
{
    txListIterator iter(&mCDATASectionElements);
    while (iter.hasNext())
        delete (txExpandedName*)iter.next();
}

void txOutputFormat::reset()
{
    mMethod = eMethodNotSet;
    mVersion.clear();
    if (mEncoding.isEmpty())
        mOmitXMLDeclaration = eNotSet;
    mStandalone = eNotSet;
    mPublicId.clear();
    mSystemId.clear();
    txListIterator iter(&mCDATASectionElements);
    while (iter.hasNext())
        delete (txExpandedName*)iter.next();
    mIndent = eNotSet;
    mMediaType.clear();
}

void txOutputFormat::merge(txOutputFormat& aOutputFormat)
{
    if (mMethod == eMethodNotSet)
        mMethod = aOutputFormat.mMethod;

    if (mVersion.isEmpty())
        mVersion = aOutputFormat.mVersion;

    if (mEncoding.isEmpty())
        mEncoding = aOutputFormat.mEncoding;

    if (mOmitXMLDeclaration == eNotSet)
        mOmitXMLDeclaration = aOutputFormat.mOmitXMLDeclaration;

    if (mStandalone == eNotSet)
        mStandalone = aOutputFormat.mStandalone;

    if (mPublicId.isEmpty())
        mPublicId = aOutputFormat.mPublicId;

    if (mSystemId.isEmpty())
        mSystemId = aOutputFormat.mSystemId;

    txListIterator iter(&aOutputFormat.mCDATASectionElements);
    txExpandedName* qName;
    while ((qName = (txExpandedName*)iter.next())) {
        mCDATASectionElements.add(qName);
        // XXX We need txList.clear()
        iter.remove();
    }

    if (mIndent == eNotSet)
        mIndent = aOutputFormat.mIndent;

    if (mMediaType.isEmpty())
        mMediaType = aOutputFormat.mMediaType;
}

void txOutputFormat::setFromDefaults()
{
    if (mMethod == eMethodNotSet)
        mMethod = eXMLOutput;

    switch (mMethod) {
        case eXMLOutput:
        {
            if (mVersion.isEmpty())
                mVersion.append("1.0");

            if (mEncoding.isEmpty())
                mEncoding.append("UTF-8");

            if (mOmitXMLDeclaration == eNotSet)
                mOmitXMLDeclaration = eFalse;

            if (mIndent == eNotSet)
                mIndent = eFalse;

            if (mMediaType.isEmpty())
                mMediaType.append("text/xml");

            break;
        }
        case eHTMLOutput:
        {
            if (mVersion.isEmpty())
                mVersion.append("4.0");

            if (mEncoding.isEmpty())
                mEncoding.append("UTF-8");

            if (mIndent == eNotSet)
                mIndent = eTrue;

            if (mMediaType.isEmpty())
                mMediaType.append("text/html");

            break;
        }
        case eTextOutput:
        {
            if (mEncoding.isEmpty())
                mEncoding.append("UTF-8");

            if (mMediaType.isEmpty())
                mMediaType.append("text/plain");

            break;
        }
    }
}
