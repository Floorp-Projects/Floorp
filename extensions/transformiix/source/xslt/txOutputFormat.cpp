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
    mVersion.Truncate();
    if (mEncoding.IsEmpty())
        mOmitXMLDeclaration = eNotSet;
    mStandalone = eNotSet;
    mPublicId.Truncate();
    mSystemId.Truncate();
    txListIterator iter(&mCDATASectionElements);
    while (iter.hasNext())
        delete (txExpandedName*)iter.next();
    mIndent = eNotSet;
    mMediaType.Truncate();
}

void txOutputFormat::merge(txOutputFormat& aOutputFormat)
{
    if (mMethod == eMethodNotSet)
        mMethod = aOutputFormat.mMethod;

    if (mVersion.IsEmpty())
        mVersion = aOutputFormat.mVersion;

    if (mEncoding.IsEmpty())
        mEncoding = aOutputFormat.mEncoding;

    if (mOmitXMLDeclaration == eNotSet)
        mOmitXMLDeclaration = aOutputFormat.mOmitXMLDeclaration;

    if (mStandalone == eNotSet)
        mStandalone = aOutputFormat.mStandalone;

    if (mPublicId.IsEmpty())
        mPublicId = aOutputFormat.mPublicId;

    if (mSystemId.IsEmpty())
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

    if (mMediaType.IsEmpty())
        mMediaType = aOutputFormat.mMediaType;
}

void txOutputFormat::setFromDefaults()
{
    switch (mMethod) {
        case eMethodNotSet:
        {
            mMethod = eXMLOutput;
            // Fall through
        }
        case eXMLOutput:
        {
            if (mVersion.IsEmpty())
                mVersion.AppendLiteral("1.0");

            if (mEncoding.IsEmpty())
                mEncoding.AppendLiteral("UTF-8");

            if (mOmitXMLDeclaration == eNotSet)
                mOmitXMLDeclaration = eFalse;

            if (mIndent == eNotSet)
                mIndent = eFalse;

            if (mMediaType.IsEmpty())
                mMediaType.AppendLiteral("text/xml");

            break;
        }
        case eHTMLOutput:
        {
            if (mVersion.IsEmpty())
                mVersion.AppendLiteral("4.0");

            if (mEncoding.IsEmpty())
                mEncoding.AppendLiteral("UTF-8");

            if (mIndent == eNotSet)
                mIndent = eTrue;

            if (mMediaType.IsEmpty())
                mMediaType.AppendLiteral("text/html");

            break;
        }
        case eTextOutput:
        {
            if (mEncoding.IsEmpty())
                mEncoding.AppendLiteral("UTF-8");

            if (mMediaType.IsEmpty())
                mMediaType.AppendLiteral("text/plain");

            break;
        }
    }
}
