/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXEXECUTIONSTATE_H
#define TRANSFRMX_TXEXECUTIONSTATE_H

#include "txCore.h"
#include "txStack.h"
#include "txXMLUtils.h"
#include "txIXPathContext.h"
#include "txVariableMap.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "txKey.h"
#include "txStylesheet.h"
#include "txXPathTreeWalker.h"
#include "nsTArray.h"

class txAOutputHandlerFactory;
class txAXMLEventHandler;
class txInstruction;
class txIOutputHandlerFactory;

class txLoadedDocumentEntry : public nsStringHashKey
{
public:
    txLoadedDocumentEntry(KeyTypePointer aStr) : nsStringHashKey(aStr),
                                                 mLoadResult(NS_OK)
    {
    }
    txLoadedDocumentEntry(const txLoadedDocumentEntry& aToCopy)
        : nsStringHashKey(aToCopy)
    {
        NS_ERROR("We're horked.");
    }
    ~txLoadedDocumentEntry()
    {
        if (mDocument) {
            txXPathNodeUtils::release(mDocument);
        }
    }
    bool LoadingFailed()
    {
        NS_ASSERTION(NS_SUCCEEDED(mLoadResult) || !mDocument,
                     "Load failed but we still got a document?");

        return NS_FAILED(mLoadResult);
    }

    nsAutoPtr<txXPathNode> mDocument;
    nsresult mLoadResult;
};

class txLoadedDocumentsHash : public nsTHashtable<txLoadedDocumentEntry>
{
public:
     txLoadedDocumentsHash()
        : mSourceDocument(nullptr)
     {
     }
    ~txLoadedDocumentsHash();
    void init(txXPathNode* aSourceDocument);

private:
    friend class txExecutionState;
    txXPathNode* mSourceDocument;
};


class txExecutionState : public txIMatchContext
{
public:
    txExecutionState(txStylesheet* aStylesheet, bool aDisableLoads);
    ~txExecutionState();
    nsresult init(const txXPathNode& aNode,
                  txOwningExpandedNameMap<txIGlobalParameter>* aGlobalParams);
    nsresult end(nsresult aResult);

    TX_DECL_MATCH_CONTEXT;

    /**
     * Struct holding information about a current template rule
     */
    class TemplateRule {
    public:
        txStylesheet::ImportFrame* mFrame;
        int32_t mModeNsId;
        nsCOMPtr<nsIAtom> mModeLocalName;
        txVariableMap* mParams;
    };

    // Stack functions
    nsresult pushEvalContext(txIEvalContext* aContext);
    txIEvalContext* popEvalContext();
    nsresult pushBool(bool aBool);
    bool popBool();
    nsresult pushResultHandler(txAXMLEventHandler* aHandler);
    txAXMLEventHandler* popResultHandler();
    void pushTemplateRule(txStylesheet::ImportFrame* aFrame,
                          const txExpandedName& aMode,
                          txVariableMap* aParams);
    void popTemplateRule();
    nsresult pushParamMap(txVariableMap* aParams);
    txVariableMap* popParamMap();

    // state-getting functions
    txIEvalContext* getEvalContext();
    const txXPathNode* retrieveDocument(const nsAString& aUri);
    nsresult getKeyNodes(const txExpandedName& aKeyName,
                         const txXPathNode& aRoot,
                         const nsAString& aKeyValue, bool aIndexIfNotFound,
                         txNodeSet** aResult);
    TemplateRule* getCurrentTemplateRule();
    const txXPathNode& getSourceDocument()
    {
        NS_ASSERTION(mLoadedDocuments.mSourceDocument,
                     "Need a source document!");

        return *mLoadedDocuments.mSourceDocument;
    }

    // state-modification functions
    txInstruction* getNextInstruction();
    nsresult runTemplate(txInstruction* aInstruction);
    nsresult runTemplate(txInstruction* aInstruction,
                         txInstruction* aReturnTo);
    void gotoInstruction(txInstruction* aNext);
    void returnFromTemplate();
    nsresult bindVariable(const txExpandedName& aName,
                          txAExprResult* aValue);
    void removeVariable(const txExpandedName& aName);

    txAXMLEventHandler* mOutputHandler;
    txAXMLEventHandler* mResultHandler;
    nsAutoPtr<txAXMLEventHandler> mObsoleteHandler;
    txAOutputHandlerFactory* mOutputHandlerFactory;

    nsAutoPtr<txVariableMap> mTemplateParams;

    nsRefPtr<txStylesheet> mStylesheet;

private:
    txStack mReturnStack;
    txStack mLocalVarsStack;
    txStack mEvalContextStack;
    nsTArray<bool> mBoolStack;
    txStack mResultHandlerStack;
    txStack mParamStack;
    txInstruction* mNextInstruction;
    txVariableMap* mLocalVariables;
    txVariableMap mGlobalVariableValues;
    nsRefPtr<txAExprResult> mGlobalVarPlaceholderValue;
    int32_t mRecursionDepth;

    AutoInfallibleTArray<TemplateRule, 10> mTemplateRules;

    txIEvalContext* mEvalContext;
    txIEvalContext* mInitialEvalContext;
    //Document* mRTFDocument;
    txOwningExpandedNameMap<txIGlobalParameter>* mGlobalParams;

    txLoadedDocumentsHash mLoadedDocuments;
    txKeyHash mKeyHash;
    nsRefPtr<txResultRecycler> mRecycler;
    bool mDisableLoads;

    static const int32_t kMaxRecursionDepth;
};

#endif
