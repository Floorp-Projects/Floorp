/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txOutputFormat.h"
#include "txXMLUtils.h"
#include "txExpandedName.h"

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
