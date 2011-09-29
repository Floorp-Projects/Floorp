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

class txElementContext : public TxObject
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
    virtual void AddRef() = 0;
    virtual void Release() = 0;

    virtual nsresult loadURI(const nsAString& aUri,
                             const nsAString& aReferrerUri,
                             txStylesheetCompiler* aCompiler) = 0;
    virtual void onDoneCompiling(txStylesheetCompiler* aCompiler,
                                 nsresult aResult,
                                 const PRUnichar *aErrorText = nsnull,
                                 const PRUnichar *aParam = nsnull) = 0;
};

#define TX_DECL_ACOMPILEOBSERVER \
  nsresult loadURI(const nsAString& aUri, const nsAString& aReferrerUri, \
                   txStylesheetCompiler* aCompiler); \
  void onDoneCompiling(txStylesheetCompiler* aCompiler, nsresult aResult, \
                       const PRUnichar *aErrorText = nsnull, \
                       const PRUnichar *aParam = nsnull);

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
    nsresult pushHandlerTable(txHandlerTable* aTable);
    void popHandlerTable();
    nsresult pushSorter(txPushNewContext* aSorter);
    void popSorter();
    nsresult pushChooseGotoList();
    void popChooseGotoList();
    nsresult pushObject(TxObject* aObject);
    TxObject* popObject();
    nsresult pushPtr(void* aPtr);
    void* popPtr();

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

    void cancel(nsresult aError, const PRUnichar *aErrorText = nsnull,
                const PRUnichar *aParam = nsnull);

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
