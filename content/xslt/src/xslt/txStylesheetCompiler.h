/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXSTYLESHEETCOMPILER_H
#define TRANSFRMX_TXSTYLESHEETCOMPILER_H

#include "txStack.h"
#include "txXSLTPatterns.h"
#include "txExpr.h"
#include "txIXPathContext.h"
#include "nsAutoPtr.h"
#include "txStylesheet.h"
#include "nsTArray.h"

extern bool
TX_XSLTFunctionAvailable(nsIAtom* aName, PRInt32 aNameSpaceID);

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
    txElementContext(const nsAString& aBaseURI);
    txElementContext(const txElementContext& aOther);

    bool mPreserveWhitespace;
    bool mForwardsCompatibleParsing;
    nsString mBaseURI;
    nsRefPtr<txNamespaceMap> mMappings;
    nsTArray<PRInt32> mInstructionNamespaces;
    PRInt32 mDepth;
};

class txACompileObserver
{
public:
    NS_IMETHOD_(nsrefcnt) AddRef() = 0;
    NS_IMETHOD_(nsrefcnt) Release() = 0;

    virtual nsresult loadURI(const nsAString& aUri,
                             const nsAString& aReferrerUri,
                             txStylesheetCompiler* aCompiler) = 0;
    virtual void onDoneCompiling(txStylesheetCompiler* aCompiler,
                                 nsresult aResult,
                                 const PRUnichar *aErrorText = nullptr,
                                 const PRUnichar *aParam = nullptr) = 0;
};

#define TX_DECL_ACOMPILEOBSERVER \
  nsresult loadURI(const nsAString& aUri, const nsAString& aReferrerUri, \
                   txStylesheetCompiler* aCompiler); \
  void onDoneCompiling(txStylesheetCompiler* aCompiler, nsresult aResult, \
                       const PRUnichar *aErrorText = nullptr, \
                       const PRUnichar *aParam = nullptr);

class txStylesheetCompilerState : public txIParseContext
{
public:
    txStylesheetCompilerState(txACompileObserver* aObserver);
    ~txStylesheetCompilerState();
    
    nsresult init(const nsAString& aStylesheetURI, txStylesheet* aStylesheet,
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
    nsresult addInstruction(nsAutoPtr<txInstruction> aInstruction);
    nsresult loadIncludedStylesheet(const nsAString& aURI);
    nsresult loadImportedStylesheet(const nsAString& aURI,
                                    txStylesheet::ImportFrame* aFrame);
    
    // misc
    nsresult addGotoTarget(txInstruction** aTargetPointer);
    nsresult addVariable(const txExpandedName& aName);

    // txIParseContext
    nsresult resolveNamespacePrefix(nsIAtom* aPrefix, PRInt32& aID);
    nsresult resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                 FunctionCall** aFunction);
    bool caseInsensitiveNameTests();

    /**
     * Should the stylesheet be parsed in forwards compatible parsing mode.
     */
    bool fcp()
    {
        return mElementContext->mForwardsCompatibleParsing;
    }

    void SetErrorOffset(PRUint32 aOffset);

    static void shutdown();


    nsRefPtr<txStylesheet> mStylesheet;
    txHandlerTable* mHandlerTable;
    nsAutoPtr<txElementContext> mElementContext;
    txPushNewContext* mSorter;
    nsAutoPtr<txList> mChooseGotoList;
    bool mDOE;
    bool mSearchingForFallback;

protected:
    nsRefPtr<txACompileObserver> mObserver;
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
    nsString mStylesheetURI;
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
    PRInt32 mNamespaceID;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mPrefix;
    nsString mValue;
};

class txStylesheetCompiler : private txStylesheetCompilerState,
                             public txACompileObserver
{
public:
    friend class txStylesheetCompilerState;
    friend bool TX_XSLTFunctionAvailable(nsIAtom* aName,
                                           PRInt32 aNameSpaceID);
    txStylesheetCompiler(const nsAString& aStylesheetURI,
                         txACompileObserver* aObserver);
    txStylesheetCompiler(const nsAString& aStylesheetURI,
                         txStylesheet* aStylesheet,
                         txListIterator* aInsertPosition,
                         txACompileObserver* aObserver);

    void setBaseURI(const nsString& aBaseURI);

    nsresult startElement(PRInt32 aNamespaceID, nsIAtom* aLocalName,
                          nsIAtom* aPrefix, txStylesheetAttr* aAttributes,
                          PRInt32 aAttrCount);
    nsresult startElement(const PRUnichar *aName,
                          const PRUnichar **aAtts,
                          PRInt32 aAttrCount, PRInt32 aIDOffset);
    nsresult endElement();
    nsresult characters(const nsAString& aStr);
    nsresult doneLoading();

    void cancel(nsresult aError, const PRUnichar *aErrorText = nullptr,
                const PRUnichar *aParam = nullptr);

    txStylesheet* getStylesheet();

    TX_DECL_ACOMPILEOBSERVER
    NS_INLINE_DECL_REFCOUNTING(txStylesheetCompiler)

private:
    nsresult startElementInternal(PRInt32 aNamespaceID, nsIAtom* aLocalName,
                                  nsIAtom* aPrefix,
                                  txStylesheetAttr* aAttributes,
                                  PRInt32 aAttrCount,
                                  PRInt32 aIDOffset = -1);

    nsresult flushCharacters();
    nsresult ensureNewElementContext();
    nsresult maybeDoneCompiling();

    nsString mCharacters;
    nsresult mStatus;
};

class txInScopeVariable {
public:
    txInScopeVariable(const txExpandedName& aName) : mName(aName), mLevel(1)
    {
    }
    txExpandedName mName;
    PRInt32 mLevel;
};

#endif
