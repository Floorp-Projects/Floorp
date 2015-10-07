/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXINSTRUCTIONS_H
#define TRANSFRMX_TXINSTRUCTIONS_H

#include "nsCOMPtr.h"
#include "txCore.h"
#include "nsString.h"
#include "txXMLUtils.h"
#include "txExpandedName.h"
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
    explicit txApplyTemplates(const txExpandedName& aMode);

    TX_DECL_TXINSTRUCTION
    
    txExpandedName mMode;
};

class txAttribute : public txInstruction
{
public:
    txAttribute(nsAutoPtr<Expr>&& aName, nsAutoPtr<Expr>&& aNamespace,
                txNamespaceMap* aMappings);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mName;
    nsAutoPtr<Expr> mNamespace;
    RefPtr<txNamespaceMap> mMappings;
};

class txCallTemplate : public txInstruction
{
public:
    explicit txCallTemplate(const txExpandedName& aName);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
};

class txCheckParam : public txInstruction
{
public:
    explicit txCheckParam(const txExpandedName& aName);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
    txInstruction* mBailTarget;
};

class txConditionalGoto : public txInstruction
{
public:
    txConditionalGoto(nsAutoPtr<Expr>&& aCondition, txInstruction* aTarget);

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
    explicit txCopyOf(nsAutoPtr<Expr>&& aSelect);

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
    explicit txGoTo(txInstruction* aTarget);

    TX_DECL_TXINSTRUCTION
    
    txInstruction* mTarget;
};

class txInsertAttrSet : public txInstruction
{
public:
    explicit txInsertAttrSet(const txExpandedName& aName);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
};

class txLoopNodeSet : public txInstruction
{
public:
    explicit txLoopNodeSet(txInstruction* aTarget);

    TX_DECL_TXINSTRUCTION
    
    txInstruction* mTarget;
};

class txLREAttribute : public txInstruction
{
public:
    txLREAttribute(int32_t aNamespaceID, nsIAtom* aLocalName,
                   nsIAtom* aPrefix, nsAutoPtr<Expr>&& aValue);

    TX_DECL_TXINSTRUCTION

    int32_t mNamespaceID;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mLowercaseLocalName;
    nsCOMPtr<nsIAtom> mPrefix;
    nsAutoPtr<Expr> mValue;
};

class txMessage : public txInstruction
{
public:
    explicit txMessage(bool aTerminate);

    TX_DECL_TXINSTRUCTION

    bool mTerminate;
};

class txNumber : public txInstruction
{
public:
    txNumber(txXSLTNumber::LevelType aLevel, nsAutoPtr<txPattern>&& aCount,
             nsAutoPtr<txPattern>&& aFrom, nsAutoPtr<Expr>&& aValue,
             nsAutoPtr<Expr>&& aFormat, nsAutoPtr<Expr>&& aGroupingSeparator,
             nsAutoPtr<Expr>&& aGroupingSize);

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
    explicit txProcessingInstruction(nsAutoPtr<Expr>&& aName);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mName;
};

class txPushNewContext : public txInstruction
{
public:
    explicit txPushNewContext(nsAutoPtr<Expr>&& aSelect);
    ~txPushNewContext();

    TX_DECL_TXINSTRUCTION
    
    
    nsresult addSort(nsAutoPtr<Expr>&& aSelectExpr,
                     nsAutoPtr<Expr>&& aLangExpr,
                     nsAutoPtr<Expr>&& aDataTypeExpr,
                     nsAutoPtr<Expr>&& aOrderExpr,
                     nsAutoPtr<Expr>&& aCaseOrderExpr);

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
    explicit txPushStringHandler(bool aOnlyText);

    TX_DECL_TXINSTRUCTION

    bool mOnlyText;
};

class txRemoveVariable : public txInstruction
{
public:
    explicit txRemoveVariable(const txExpandedName& aName);

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
    txSetParam(const txExpandedName& aName, nsAutoPtr<Expr>&& aValue);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
    nsAutoPtr<Expr> mValue;
};

class txSetVariable : public txInstruction
{
public:
    txSetVariable(const txExpandedName& aName, nsAutoPtr<Expr>&& aValue);

    TX_DECL_TXINSTRUCTION

    txExpandedName mName;
    nsAutoPtr<Expr> mValue;
};

class txStartElement : public txInstruction
{
public:
    txStartElement(nsAutoPtr<Expr>&& aName, nsAutoPtr<Expr>&& aNamespace,
                   txNamespaceMap* aMappings);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mName;
    nsAutoPtr<Expr> mNamespace;
    RefPtr<txNamespaceMap> mMappings;
};

class txStartLREElement : public txInstruction
{
public:
    txStartLREElement(int32_t aNamespaceID, nsIAtom* aLocalName,
                      nsIAtom* aPrefix);

    TX_DECL_TXINSTRUCTION

    int32_t mNamespaceID;
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
    txValueOf(nsAutoPtr<Expr>&& aExpr, bool aDOE);

    TX_DECL_TXINSTRUCTION

    nsAutoPtr<Expr> mExpr;
    bool mDOE;
};

#endif //TRANSFRMX_TXINSTRUCTIONS_H
