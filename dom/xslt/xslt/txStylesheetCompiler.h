/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXSTYLESHEETCOMPILER_H
#define TRANSFRMX_TXSTYLESHEETCOMPILER_H

#include "mozilla/Attributes.h"
#include "txStack.h"
#include "txXSLTPatterns.h"
#include "txExpr.h"
#include "txIXPathContext.h"
#include "nsAutoPtr.h"
#include "txStylesheet.h"
#include "nsTArray.h"
#include "mozilla/net/ReferrerPolicy.h"

extern bool
TX_XSLTFunctionAvailable(nsIAtom* aName, int32_t aNameSpaceID);

class txHandlerTable;
class txElementContext;
class txInstructionContainer;
class txInstruction;
class txNamespaceMap;
class txToplevelItem;
class txPushNewContext;
class txStylesheetCompiler;
class txInScopeVariable;

class txElementContext : public txObject
{
public:
    explicit txElementContext();
    txElementContext(const txElementContext& aOther);

    bool mPreserveWhitespace;
    bool mForwardsCompatibleParsing;
    nsCOMPtr<nsIURI> mBaseURI;
    RefPtr<txNamespaceMap> mMappings;
    nsTArray<int32_t> mInstructionNamespaces;
    int32_t mDepth;
};

class txACompileObserver
{
public:
    NS_IMETHOD_(MozExternalRefCountType) AddRef() = 0;
    NS_IMETHOD_(MozExternalRefCountType) Release() = 0;

    virtual nsresult loadURI(nsIURI* aUri,
                             nsIPrincipal* aReferrerPrincipal,
                             txStylesheetCompiler* aCompiler) = 0;
    virtual void onDoneCompiling(txStylesheetCompiler* aCompiler,
                                 nsresult aResult,
                                 const char16_t *aErrorText = nullptr,
                                 const char16_t *aParam = nullptr) = 0;
};

#define TX_DECL_ACOMPILEOBSERVER \
  nsresult loadURI(nsIURI* aUri, \
                   nsIPrincipal* aReferrerPrincipal, \
                   txStylesheetCompiler* aCompiler); \
  void onDoneCompiling(txStylesheetCompiler* aCompiler, nsresult aResult, \
                       const char16_t *aErrorText = nullptr, \
                       const char16_t *aParam = nullptr);

class txStylesheetCompilerState : public txIParseContext
{
public:
    explicit txStylesheetCompilerState(txACompileObserver* aObserver);
    ~txStylesheetCompilerState();
    
    nsresult init(const nsString& aFragment, txStylesheet* aStylesheet,
                  txListIterator* aInsertPosition);

    // Embedded stylesheets state
    bool handleEmbeddedSheet()
    {
        return mEmbedStatus == eInEmbed;
    }
    void doneEmbedding()
    {
        mEmbedStatus = eHasEmbed;
    }

    // Stack functions
    enum enumStackType
    {
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
    nsresult pushHandlerTable(txHandlerTable* aTable);
    void popHandlerTable();
    nsresult pushSorter(txPushNewContext* aSorter);
    void popSorter();
    nsresult pushChooseGotoList();
    void popChooseGotoList();
    nsresult pushObject(txObject* aObject);
    txObject* popObject();
    nsresult pushPtr(void* aPtr, enumStackType aType);
    void* popPtr(enumStackType aType);
    
    // stylesheet functions
    nsresult addToplevelItem(txToplevelItem* aItem);
    nsresult openInstructionContainer(txInstructionContainer* aContainer);
    void closeInstructionContainer();
    nsresult addInstruction(nsAutoPtr<txInstruction>&& aInstruction);
    nsresult loadIncludedStylesheet(nsIURI* aURI);
    nsresult loadImportedStylesheet(nsIURI* aURI,
                                    txStylesheet::ImportFrame* aFrame);
    
    // misc
    nsresult addGotoTarget(txInstruction** aTargetPointer);
    nsresult addVariable(const txExpandedName& aName);

    // txIParseContext
    nsresult resolveNamespacePrefix(nsIAtom* aPrefix, int32_t& aID) override;
    nsresult resolveFunctionCall(nsIAtom* aName, int32_t aID,
                                 FunctionCall** aFunction) override;
    bool caseInsensitiveNameTests() override;

    /**
     * Should the stylesheet be parsed in forwards compatible parsing mode.
     */
    bool fcp()
    {
        return mElementContext->mForwardsCompatibleParsing;
    }

    void SetErrorOffset(uint32_t aOffset) override;

    bool allowed(Allowed aAllowed) override
    {
        return !(mDisAllowed & aAllowed);
    }

    bool ignoreError(nsresult aResult)
    {
        // Some errors shouldn't be ignored even in forwards compatible parsing
        // mode.
        return aResult != NS_ERROR_XSLT_CALL_TO_KEY_NOT_ALLOWED &&
               fcp();
    }

    static void shutdown();


    RefPtr<txStylesheet> mStylesheet;
    txHandlerTable* mHandlerTable;
    nsAutoPtr<txElementContext> mElementContext;
    txPushNewContext* mSorter;
    nsAutoPtr<txList> mChooseGotoList;
    bool mDOE;
    bool mSearchingForFallback;
    uint16_t mDisAllowed;

protected:
    nsCOMPtr<nsIPrincipal> mStylesheetPrincipal;
    RefPtr<txACompileObserver> mObserver;
    nsTArray<txInScopeVariable*> mInScopeVariables;
    nsTArray<txStylesheetCompiler*> mChildCompilerList;
    // embed info, target information is the ID
    nsString mTarget;
    enum 
    {
        eNoEmbed,
        eNeedEmbed,
        eInEmbed,
        eHasEmbed
    } mEmbedStatus;
    bool mIsTopCompiler;
    bool mDoneWithThisStylesheet;
    txStack mObjectStack;
    txStack mOtherStack;
    nsTArray<enumStackType> mTypeStack;

private:
    txInstruction** mNextInstrPtr;
    txListIterator mToplevelIterator;
    nsTArray<txInstruction**> mGotoTargetPointers;
};

struct txStylesheetAttr
{
    int32_t mNamespaceID;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mPrefix;
    nsString mValue;
};

class txStylesheetCompiler final : private txStylesheetCompilerState,
                                   public txACompileObserver
{
public:
    friend class txStylesheetCompilerState;
    friend bool TX_XSLTFunctionAvailable(nsIAtom* aName,
                                           int32_t aNameSpaceID);
    txStylesheetCompiler(const nsString& aFragment,
                         txACompileObserver* aObserver);
    txStylesheetCompiler(const nsString& aFragment,
                         txStylesheet* aStylesheet,
                         txListIterator* aInsertPosition,
                         txACompileObserver* aObserver);

    void setBaseURI(nsIURI* aBaseURI);
    void setPrincipal(nsIPrincipal* aPrincipal);

    nsresult startElement(int32_t aNamespaceID, nsIAtom* aLocalName,
                          nsIAtom* aPrefix, txStylesheetAttr* aAttributes,
                          int32_t aAttrCount);
    nsresult startElement(const char16_t *aName,
                          const char16_t **aAtts,
                          int32_t aAttrCount);
    nsresult endElement();
    nsresult characters(const nsAString& aStr);
    nsresult doneLoading();

    void cancel(nsresult aError, const char16_t *aErrorText = nullptr,
                const char16_t *aParam = nullptr);

    txStylesheet* getStylesheet();

    TX_DECL_ACOMPILEOBSERVER
    NS_INLINE_DECL_REFCOUNTING(txStylesheetCompiler)

private:
    // Private destructor, to discourage deletion outside of Release():
    ~txStylesheetCompiler()
    {
    }

    nsresult startElementInternal(int32_t aNamespaceID, nsIAtom* aLocalName,
                                  nsIAtom* aPrefix,
                                  txStylesheetAttr* aAttributes,
                                  int32_t aAttrCount);

    nsresult flushCharacters();
    nsresult ensureNewElementContext();
    nsresult maybeDoneCompiling();

    nsString mCharacters;
    nsresult mStatus;
};

class txInScopeVariable {
public:
    explicit txInScopeVariable(const txExpandedName& aName) : mName(aName), mLevel(1)
    {
    }
    txExpandedName mName;
    int32_t mLevel;
};

#endif
