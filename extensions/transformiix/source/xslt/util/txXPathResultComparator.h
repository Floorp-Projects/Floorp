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
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
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

#ifndef TRANSFRMX_XPATHRESULTCOMPARATOR_H
#define TRANSFRMX_XPATHRESULTCOMPARATOR_H

#include "TxObject.h"
#include "TxString.h"
#ifndef TX_EXE
#include "nsCOMPtr.h"
#include "nsICollation.h"
#endif

class ExprResult;

/*
 * Result comparators
 */
class txXPathResultComparator
{
public:
    /*
     * Compares two XPath results. Returns -1 if val1 < val2,
     * 1 if val1 > val2 and 0 if val1 == val2.
     */
    virtual int compareValues(TxObject* val1, TxObject* val2) = 0;
    
    /*
     * Create a sortable value.
     */
    virtual TxObject* createSortableValue(ExprResult* exprRes) = 0;
};

/*
 * Compare results as stings (data-type="text")
 */
class txResultStringComparator : public txXPathResultComparator
{
public:
    txResultStringComparator(MBool aAscending, MBool aUpperFirst,
                             const String& aLanguage);
    virtual ~txResultStringComparator();

    int compareValues(TxObject* aVal1, TxObject* aVal2);
    TxObject* createSortableValue(ExprResult* aExprRes);
private:
#ifndef TX_EXE
    nsCOMPtr<nsICollation> mCollation;
    nsresult init(const String& aLanguage);
    nsresult createRawSortKey(const nsCollationStrength aStrength,
                              const nsString& aString,
                              PRUint8** aKey,
                              PRUint32* aLength);
#endif
    int mSorting;

    class StringValue : public TxObject
    {
    public:
#ifdef TX_EXE
        String mStr;
#else
        StringValue();
        ~StringValue();

        PRUint8* mKey;
        void* mCaseKey;
        PRUint32 mLength, mCaseLength;
#endif
    };
};

/*
 * Compare results as numbers (data-type="number")
 */
class txResultNumberComparator : public txXPathResultComparator
{
public:
    txResultNumberComparator(MBool aAscending);
    virtual ~txResultNumberComparator();

    int compareValues(TxObject* aVal1, TxObject* aVal2);
    TxObject* createSortableValue(ExprResult* aExprRes);

private:
    int mAscending;

    class NumberValue : public TxObject
    {
    public:
        double mVal;
    };
};

#endif
