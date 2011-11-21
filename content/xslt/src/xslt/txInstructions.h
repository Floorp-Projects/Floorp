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
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc>
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

#ifndef TRANSFRMX_TXINSTRUCTIONS_H
#define TRANSFRMX_TXINSTRUCTIONS_H

#include "nsCOMPtr.h"
#include "txCore.h"
#include "nsString.h"
#include "txXMLUtils.h"
#include "txNamespaceMap.h"
#include "nsAutoPtr.h"
#include "txXSLTNumber.h"
#include "nsTArray.h"

class nsIAtom;
class txExecutionState;

class txInstruction : public txObject
{
public:
    txInstruction()
    {
        MOZ_COUNT_CTOR(txInstruction);
    }

    virtual ~txInstruction()
    {
        MOZ_COUNT_DTOR(txInstruction);
    }

    virtual nsresult execute(txExecutionState& aEs) = 0;

    nsAutoPtr<txInstruction> mNext;
};

#define TX_DECL_TXINSTRUCTION  \
    virtual nsresult execute(txExecutionState& aEs);


class txApplyDefaultElementTemplate : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txApplyImportsEnd : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txApplyImportsStart : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txApplyTemplates : public txInstruction
{
public:
    txApplyTemplates(const txExpandedName& aMode);

    TX_DECL_TXINSTRUCTION
    
    txExpandedName mMode;
};

class txAttribute : public txInstruction
{
public:
    txAttribute(nsAutoPtr<Expr> aName, nsAutoPtr<Expr> aNamespace,
                txNamespaceMap* aMappings);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mName;
    nsAutoPtr<Expr> mNamespace;
    nsRefPtr<txNamespaceMap> mMappings;
};

class txCallTemplate : public txInstruction
{
public:
    txCallTemplate(const txExpandedName& aName);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
};

class txCheckParam : public txInstruction
{
public:
    txCheckParam(const txExpandedName& aName);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
    txInstruction* mBailTarget;
};

class txConditionalGoto : public txInstruction
{
public:
    txConditionalGoto(nsAutoPtr<Expr> aCondition, txInstruction* aTarget);

    TX_DECL_TXINSTRUCTION
    
    nsAutoPtr<Expr> mCondition;
    txInstruction* mTarget;
};

class txComment : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txCopyBase : public txInstruction
{
protected:
    nsresult copyNode(const txXPathNode& aNode, txExecutionState& aEs);
};

class txCopy : public txCopyBase
{
public:
    txCopy();

    TX_DECL_TXINSTRUCTION
    
    txInstruction* mBailTarget;
};

class txCopyOf : public txCopyBase
{
public:
    txCopyOf(nsAutoPtr<Expr> aSelect);

    TX_DECL_TXINSTRUCTION
    
    nsAutoPtr<Expr> mSelect;
};

class txEndElement : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txErrorInstruction : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txGoTo : public txInstruction
{
public:
    txGoTo(txInstruction* aTarget);

    TX_DECL_TXINSTRUCTION
    
    txInstruction* mTarget;
};

class txInsertAttrSet : public txInstruction
{
public:
    txInsertAttrSet(const txExpandedName& aName);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
};

class txLoopNodeSet : public txInstruction
{
public:
    txLoopNodeSet(txInstruction* aTarget);

    TX_DECL_TXINSTRUCTION
    
    txInstruction* mTarget;
};

class txLREAttribute : public txInstruction
{
public:
    txLREAttribute(PRInt32 aNamespaceID, nsIAtom* aLocalName,
                   nsIAtom* aPrefix, nsAutoPtr<Expr> aValue);

    TX_DECL_TXINSTRUCTION

    PRInt32 mNamespaceID;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mLowercaseLocalName;
    nsCOMPtr<nsIAtom> mPrefix;
    nsAutoPtr<Expr> mValue;
};

class txMessage : public txInstruction
{
public:
    txMessage(bool aTerminate);

    TX_DECL_TXINSTRUCTION

    bool mTerminate;
};

class txNumber : public txInstruction
{
public:
    txNumber(txXSLTNumber::LevelType aLevel, nsAutoPtr<txPattern> aCount,
             nsAutoPtr<txPattern> aFrom, nsAutoPtr<Expr> aValue,
             nsAutoPtr<Expr> aFormat, nsAutoPtr<Expr> aGroupingSeparator,
             nsAutoPtr<Expr> aGroupingSize);

    TX_DECL_TXINSTRUCTION

    txXSLTNumber::LevelType mLevel;
    nsAutoPtr<txPattern> mCount;
    nsAutoPtr<txPattern> mFrom;
    nsAutoPtr<Expr> mValue;
    nsAutoPtr<Expr> mFormat;
    nsAutoPtr<Expr> mGroupingSeparator;
    nsAutoPtr<Expr> mGroupingSize;
};

class txPopParams : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txProcessingInstruction : public txInstruction
{
public:
    txProcessingInstruction(nsAutoPtr<Expr> aName);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mName;
};

class txPushNewContext : public txInstruction
{
public:
    txPushNewContext(nsAutoPtr<Expr> aSelect);
    ~txPushNewContext();

    TX_DECL_TXINSTRUCTION
    
    
    nsresult addSort(nsAutoPtr<Expr> aSelectExpr, nsAutoPtr<Expr> aLangExpr,
                     nsAutoPtr<Expr> aDataTypeExpr, nsAutoPtr<Expr> aOrderExpr,
                     nsAutoPtr<Expr> aCaseOrderExpr);

    struct SortKey {
        nsAutoPtr<Expr> mSelectExpr;
        nsAutoPtr<Expr> mLangExpr;
        nsAutoPtr<Expr> mDataTypeExpr;
        nsAutoPtr<Expr> mOrderExpr;
        nsAutoPtr<Expr> mCaseOrderExpr;
    };
    
    nsTArray<SortKey> mSortKeys;
    nsAutoPtr<Expr> mSelect;
    txInstruction* mBailTarget;
};

class txPushNullTemplateRule : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txPushParams : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txPushRTFHandler : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txPushStringHandler : public txInstruction
{
public:
    txPushStringHandler(bool aOnlyText);

    TX_DECL_TXINSTRUCTION

    bool mOnlyText;
};

class txRemoveVariable : public txInstruction
{
public:
    txRemoveVariable(const txExpandedName& aName);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
};

class txReturn : public txInstruction
{
public:
    TX_DECL_TXINSTRUCTION
};

class txSetParam : public txInstruction
{
public:
    txSetParam(const txExpandedName& aName, nsAutoPtr<Expr> aValue);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
    nsAutoPtr<Expr> mValue;
};

class txSetVariable : public txInstruction
{
public:
    txSetVariable(const txExpandedName& aName, nsAutoPtr<Expr> aValue);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
    nsAutoPtr<Expr> mValue;
};

class txStartElement : public txInstruction
{
public:
    txStartElement(nsAutoPtr<Expr> aName, nsAutoPtr<Expr> aNamespace,
                   txNamespaceMap* aMappings);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mName;
    nsAutoPtr<Expr> mNamespace;
    nsRefPtr<txNamespaceMap> mMappings;
};

class txStartLREElement : public txInstruction
{
public:
    txStartLREElement(PRInt32 aNamespaceID, nsIAtom* aLocalName,
                      nsIAtom* aPrefix);

    TX_DECL_TXINSTRUCTION

    PRInt32 mNamespaceID;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mLowercaseLocalName;
    nsCOMPtr<nsIAtom> mPrefix;
};

class txText : public txInstruction
{
public:
    txText(const nsAString& aStr, bool aDOE);

    TX_DECL_TXINSTRUCTION

    nsString mStr;
    bool mDOE;
};

class txValueOf : public txInstruction
{
public:
    txValueOf(nsAutoPtr<Expr> aExpr, bool aDOE);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mExpr;
    bool mDOE;
};

#endif //TRANSFRMX_TXINSTRUCTIONS_H
