/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXSTYLESHEETCOMPILER_H
#define TRANSFRMX_TXSTYLESHEETCOMPILER_H

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "txStack.h"
#include "txXSLTPatterns.h"
#include "txExpr.h"
#include "txIXPathContext.h"
#include "txStylesheet.h"
#include "nsTArray.h"

extern bool TX_XSLTFunctionAvailable(nsAtom* aName, int32_t aNameSpaceID);

class txHandlerTable;
class txElementContext;
class txInstructionContainer;
class txInstruction;
class txNamespaceMap;
class txToplevelItem;
class txPushNewContext;
class txStylesheetCompiler;

class txElementContext : public txObject {
 public:
  explicit txElementContext(const nsAString& aBaseURI);
  txElementContext(const txElementContext& aOther);

  bool mPreserveWhitespace;
  bool mForwardsCompatibleParsing;
  nsString mBaseURI;
  RefPtr<txNamespaceMap> mMappings;
  nsTArray<int32_t> mInstructionNamespaces;
  int32_t mDepth;
};

using mozilla::dom::ReferrerPolicy;

class txACompileObserver {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual nsresult loadURI(const nsAString& aUri, const nsAString& aReferrerUri,
                           ReferrerPolicy aReferrerPolicy,
                           txStylesheetCompiler* aCompiler) = 0;
  virtual void onDoneCompiling(txStylesheetCompiler* aCompiler,
                               nsresult aResult,
                               const char16_t* aErrorText = nullptr,
                               const char16_t* aParam = nullptr) = 0;
};

#define TX_DECL_ACOMPILEOBSERVER                                          \
  nsresult loadURI(const nsAString& aUri, const nsAString& aReferrerUri,  \
                   ReferrerPolicy aReferrerPolicy,                        \
                   txStylesheetCompiler* aCompiler) override;             \
  void onDoneCompiling(txStylesheetCompiler* aCompiler, nsresult aResult, \
                       const char16_t* aErrorText = nullptr,              \
                       const char16_t* aParam = nullptr) override;

class txInScopeVariable {
 public:
  explicit txInScopeVariable(const txExpandedName& aName)
      : mName(aName), mLevel(1) {}
  txExpandedName mName;
  int32_t mLevel;
};

class txStylesheetCompilerState : public txIParseContext {
 public:
  explicit txStylesheetCompilerState(txACompileObserver* aObserver);
  ~txStylesheetCompilerState();

  nsresult init(const nsAString& aStylesheetURI, ReferrerPolicy aReferrerPolicy,
                txStylesheet* aStylesheet, txListIterator* aInsertPosition);

  // Embedded stylesheets state
  bool handleEmbeddedSheet() { return mEmbedStatus == eInEmbed; }
  void doneEmbedding() { mEmbedStatus = eHasEmbed; }

  // Stack functions
  enum enumStackType {
    eElementHandler,
    eHandlerTable,
    eVariableItem,
    eCopy,
    eInstruction,
    ePushNewContext,
    eConditionalGoto,
    eCheckParam,
    ePushNullTemplateRule
  };
  void pushHandlerTable(txHandlerTable* aTable);
  void popHandlerTable();
  void pushSorter(txPushNewContext* aSorter);
  void popSorter();
  void pushChooseGotoList();
  void popChooseGotoList();
  void pushObject(txObject* aObject);
  txObject* popObject();
  void pushPtr(void* aPtr, enumStackType aType);
  void* popPtr(enumStackType aType);

  // stylesheet functions
  void addToplevelItem(txToplevelItem* aItem);
  nsresult openInstructionContainer(txInstructionContainer* aContainer);
  void closeInstructionContainer();
  txInstruction* addInstruction(
      mozilla::UniquePtr<txInstruction>&& aInstruction);
  template <class T>
  T* addInstruction(mozilla::UniquePtr<T> aInstruction) {
    return static_cast<T*>(addInstruction(
        mozilla::UniquePtr<txInstruction>(std::move(aInstruction))));
  }
  nsresult loadIncludedStylesheet(const nsAString& aURI);
  nsresult loadImportedStylesheet(const nsAString& aURI,
                                  txStylesheet::ImportFrame* aFrame);

  // misc
  void addGotoTarget(txInstruction** aTargetPointer);
  void addVariable(const txExpandedName& aName);

  // txIParseContext
  nsresult resolveNamespacePrefix(nsAtom* aPrefix, int32_t& aID) override;
  nsresult resolveFunctionCall(nsAtom* aName, int32_t aID,
                               FunctionCall** aFunction) override;
  bool caseInsensitiveNameTests() override;

  /**
   * Should the stylesheet be parsed in forwards compatible parsing mode.
   */
  bool fcp() { return mElementContext->mForwardsCompatibleParsing; }

  void SetErrorOffset(uint32_t aOffset) override;

  bool allowed(Allowed aAllowed) override { return !(mDisAllowed & aAllowed); }

  bool ignoreError(nsresult aResult) {
    // Some errors shouldn't be ignored even in forwards compatible parsing
    // mode.
    return aResult != NS_ERROR_XSLT_CALL_TO_KEY_NOT_ALLOWED && fcp();
  }

  RefPtr<txStylesheet> mStylesheet;
  txHandlerTable* mHandlerTable;
  mozilla::UniquePtr<txElementContext> mElementContext;
  txPushNewContext* mSorter;
  mozilla::UniquePtr<txList> mChooseGotoList;
  bool mDOE;
  bool mSearchingForFallback;
  uint16_t mDisAllowed;

 protected:
  RefPtr<txACompileObserver> mObserver;
  nsTArray<txInScopeVariable> mInScopeVariables;
  nsTArray<txStylesheetCompiler*> mChildCompilerList;
  // embed info, target information is the ID
  nsString mTarget;
  enum { eNoEmbed, eNeedEmbed, eInEmbed, eHasEmbed } mEmbedStatus;
  nsString mStylesheetURI;
  bool mIsTopCompiler;
  bool mDoneWithThisStylesheet;
  txStack mObjectStack;
  txStack mOtherStack;
  nsTArray<enumStackType> mTypeStack;

 private:
  mozilla::UniquePtr<txInstruction>* mNextInstrPtr;
  txListIterator mToplevelIterator;
  nsTArray<txInstruction**> mGotoTargetPointers;
  ReferrerPolicy mReferrerPolicy;
};

struct txStylesheetAttr {
  int32_t mNamespaceID;
  RefPtr<nsAtom> mLocalName;
  RefPtr<nsAtom> mPrefix;
  nsString mValue;
};

class txStylesheetCompiler final : private txStylesheetCompilerState,
                                   public txACompileObserver {
 public:
  friend class txStylesheetCompilerState;
  friend bool TX_XSLTFunctionAvailable(nsAtom* aName, int32_t aNameSpaceID);
  txStylesheetCompiler(const nsAString& aStylesheetURI,
                       ReferrerPolicy aReferrerPolicy,
                       txACompileObserver* aObserver);
  txStylesheetCompiler(const nsAString& aStylesheetURI,
                       txStylesheet* aStylesheet,
                       txListIterator* aInsertPosition,
                       ReferrerPolicy aReferrerPolicy,
                       txACompileObserver* aObserver);

  void setBaseURI(const nsString& aBaseURI);

  nsresult startElement(int32_t aNamespaceID, nsAtom* aLocalName,
                        nsAtom* aPrefix, txStylesheetAttr* aAttributes,
                        int32_t aAttrCount);
  nsresult startElement(const char16_t* aName, const char16_t** aAtts,
                        int32_t aAttrCount);
  nsresult endElement();
  nsresult characters(const nsAString& aStr);
  nsresult doneLoading();

  void cancel(nsresult aError, const char16_t* aErrorText = nullptr,
              const char16_t* aParam = nullptr);

  txStylesheet* getStylesheet();

  TX_DECL_ACOMPILEOBSERVER
  NS_INLINE_DECL_REFCOUNTING(txStylesheetCompiler, override)

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~txStylesheetCompiler() = default;

  nsresult startElementInternal(int32_t aNamespaceID, nsAtom* aLocalName,
                                nsAtom* aPrefix, txStylesheetAttr* aAttributes,
                                int32_t aAttrCount);

  nsresult flushCharacters();
  nsresult ensureNewElementContext();
  nsresult maybeDoneCompiling();

  nsString mCharacters;
  nsresult mStatus;
};

#endif
