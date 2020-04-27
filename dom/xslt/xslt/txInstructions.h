/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXINSTRUCTIONS_H
#define TRANSFRMX_TXINSTRUCTIONS_H

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "txCore.h"
#include "nsString.h"
#include "txXMLUtils.h"
#include "txExpandedName.h"
#include "txNamespaceMap.h"
#include "txXSLTNumber.h"
#include "nsTArray.h"

class nsAtom;
class txExecutionState;

class txInstruction : public txObject {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(txInstruction)

  MOZ_COUNTED_DTOR_OVERRIDE(txInstruction)

  virtual nsresult execute(txExecutionState& aEs) = 0;

  mozilla::UniquePtr<txInstruction> mNext;
};

#define TX_DECL_TXINSTRUCTION \
  virtual nsresult execute(txExecutionState& aEs) override;

class txApplyDefaultElementTemplate : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txApplyImportsEnd : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txApplyImportsStart : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txApplyTemplates : public txInstruction {
 public:
  explicit txApplyTemplates(const txExpandedName& aMode);

  TX_DECL_TXINSTRUCTION

  txExpandedName mMode;
};

class txAttribute : public txInstruction {
 public:
  txAttribute(mozilla::UniquePtr<Expr>&& aName,
              mozilla::UniquePtr<Expr>&& aNamespace, txNamespaceMap* aMappings);

  TX_DECL_TXINSTRUCTION

  mozilla::UniquePtr<Expr> mName;
  mozilla::UniquePtr<Expr> mNamespace;
  RefPtr<txNamespaceMap> mMappings;
};

class txCallTemplate : public txInstruction {
 public:
  explicit txCallTemplate(const txExpandedName& aName);

  TX_DECL_TXINSTRUCTION

  txExpandedName mName;
};

class txCheckParam : public txInstruction {
 public:
  explicit txCheckParam(const txExpandedName& aName);

  TX_DECL_TXINSTRUCTION

  txExpandedName mName;
  txInstruction* mBailTarget;
};

class txConditionalGoto : public txInstruction {
 public:
  txConditionalGoto(mozilla::UniquePtr<Expr>&& aCondition,
                    txInstruction* aTarget);

  TX_DECL_TXINSTRUCTION

  mozilla::UniquePtr<Expr> mCondition;
  txInstruction* mTarget;
};

class txComment : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txCopyBase : public txInstruction {
 protected:
  nsresult copyNode(const txXPathNode& aNode, txExecutionState& aEs);
};

class txCopy : public txCopyBase {
 public:
  txCopy();

  TX_DECL_TXINSTRUCTION

  txInstruction* mBailTarget;
};

class txCopyOf : public txCopyBase {
 public:
  explicit txCopyOf(mozilla::UniquePtr<Expr>&& aSelect);

  TX_DECL_TXINSTRUCTION

  mozilla::UniquePtr<Expr> mSelect;
};

class txEndElement : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txErrorInstruction : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txGoTo : public txInstruction {
 public:
  explicit txGoTo(txInstruction* aTarget);

  TX_DECL_TXINSTRUCTION

  txInstruction* mTarget;
};

class txInsertAttrSet : public txInstruction {
 public:
  explicit txInsertAttrSet(const txExpandedName& aName);

  TX_DECL_TXINSTRUCTION

  txExpandedName mName;
};

class txLoopNodeSet : public txInstruction {
 public:
  explicit txLoopNodeSet(txInstruction* aTarget);

  TX_DECL_TXINSTRUCTION

  txInstruction* mTarget;
};

class txLREAttribute : public txInstruction {
 public:
  txLREAttribute(int32_t aNamespaceID, nsAtom* aLocalName, nsAtom* aPrefix,
                 mozilla::UniquePtr<Expr>&& aValue);

  TX_DECL_TXINSTRUCTION

  int32_t mNamespaceID;
  RefPtr<nsAtom> mLocalName;
  RefPtr<nsAtom> mLowercaseLocalName;
  RefPtr<nsAtom> mPrefix;
  mozilla::UniquePtr<Expr> mValue;
};

class txMessage : public txInstruction {
 public:
  explicit txMessage(bool aTerminate);

  TX_DECL_TXINSTRUCTION

  bool mTerminate;
};

class txNumber : public txInstruction {
 public:
  txNumber(txXSLTNumber::LevelType aLevel,
           mozilla::UniquePtr<txPattern>&& aCount,
           mozilla::UniquePtr<txPattern>&& aFrom,
           mozilla::UniquePtr<Expr>&& aValue,
           mozilla::UniquePtr<Expr>&& aFormat,
           mozilla::UniquePtr<Expr>&& aGroupingSeparator,
           mozilla::UniquePtr<Expr>&& aGroupingSize);

  TX_DECL_TXINSTRUCTION

  txXSLTNumber::LevelType mLevel;
  mozilla::UniquePtr<txPattern> mCount;
  mozilla::UniquePtr<txPattern> mFrom;
  mozilla::UniquePtr<Expr> mValue;
  mozilla::UniquePtr<Expr> mFormat;
  mozilla::UniquePtr<Expr> mGroupingSeparator;
  mozilla::UniquePtr<Expr> mGroupingSize;
};

class txPopParams : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txProcessingInstruction : public txInstruction {
 public:
  explicit txProcessingInstruction(mozilla::UniquePtr<Expr>&& aName);

  TX_DECL_TXINSTRUCTION

  mozilla::UniquePtr<Expr> mName;
};

class txPushNewContext : public txInstruction {
 public:
  explicit txPushNewContext(mozilla::UniquePtr<Expr>&& aSelect);
  ~txPushNewContext();

  TX_DECL_TXINSTRUCTION

  nsresult addSort(mozilla::UniquePtr<Expr>&& aSelectExpr,
                   mozilla::UniquePtr<Expr>&& aLangExpr,
                   mozilla::UniquePtr<Expr>&& aDataTypeExpr,
                   mozilla::UniquePtr<Expr>&& aOrderExpr,
                   mozilla::UniquePtr<Expr>&& aCaseOrderExpr);

  struct SortKey {
    mozilla::UniquePtr<Expr> mSelectExpr;
    mozilla::UniquePtr<Expr> mLangExpr;
    mozilla::UniquePtr<Expr> mDataTypeExpr;
    mozilla::UniquePtr<Expr> mOrderExpr;
    mozilla::UniquePtr<Expr> mCaseOrderExpr;
  };

  nsTArray<SortKey> mSortKeys;
  mozilla::UniquePtr<Expr> mSelect;
  txInstruction* mBailTarget;
};

class txPushNullTemplateRule : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txPushParams : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txPushRTFHandler : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txPushStringHandler : public txInstruction {
 public:
  explicit txPushStringHandler(bool aOnlyText);

  TX_DECL_TXINSTRUCTION

  bool mOnlyText;
};

class txRemoveVariable : public txInstruction {
 public:
  explicit txRemoveVariable(const txExpandedName& aName);

  TX_DECL_TXINSTRUCTION

  txExpandedName mName;
};

class txReturn : public txInstruction {
 public:
  TX_DECL_TXINSTRUCTION
};

class txSetParam : public txInstruction {
 public:
  txSetParam(const txExpandedName& aName, mozilla::UniquePtr<Expr>&& aValue);

  TX_DECL_TXINSTRUCTION

  txExpandedName mName;
  mozilla::UniquePtr<Expr> mValue;
};

class txSetVariable : public txInstruction {
 public:
  txSetVariable(const txExpandedName& aName, mozilla::UniquePtr<Expr>&& aValue);

  TX_DECL_TXINSTRUCTION

  txExpandedName mName;
  mozilla::UniquePtr<Expr> mValue;
};

class txStartElement : public txInstruction {
 public:
  txStartElement(mozilla::UniquePtr<Expr>&& aName,
                 mozilla::UniquePtr<Expr>&& aNamespace,
                 txNamespaceMap* aMappings);

  TX_DECL_TXINSTRUCTION

  mozilla::UniquePtr<Expr> mName;
  mozilla::UniquePtr<Expr> mNamespace;
  RefPtr<txNamespaceMap> mMappings;
};

class txStartLREElement : public txInstruction {
 public:
  txStartLREElement(int32_t aNamespaceID, nsAtom* aLocalName, nsAtom* aPrefix);

  TX_DECL_TXINSTRUCTION

  int32_t mNamespaceID;
  RefPtr<nsAtom> mLocalName;
  RefPtr<nsAtom> mLowercaseLocalName;
  RefPtr<nsAtom> mPrefix;
};

class txText : public txInstruction {
 public:
  txText(const nsAString& aStr, bool aDOE);

  TX_DECL_TXINSTRUCTION

  nsString mStr;
  bool mDOE;
};

class txValueOf : public txInstruction {
 public:
  txValueOf(mozilla::UniquePtr<Expr>&& aExpr, bool aDOE);

  TX_DECL_TXINSTRUCTION

  mozilla::UniquePtr<Expr> mExpr;
  bool mDOE;
};

#endif  // TRANSFRMX_TXINSTRUCTIONS_H
