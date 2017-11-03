/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExecutionState.h"
#include "txSingleNodeContext.h"
#include "txInstructions.h"
#include "txStylesheet.h"
#include "txVariableMap.h"
#include "txRtfHandler.h"
#include "txXSLTProcessor.h"
#include "txLog.h"
#include "txURIUtils.h"
#include "txXMLParser.h"

const int32_t txExecutionState::kMaxRecursionDepth = 20000;

nsresult
txLoadedDocumentsHash::init(const txXPathNode& aSource)
{
    mSourceDocument = txXPathNodeUtils::getOwnerDocument(aSource);

    nsAutoString baseURI;
    nsresult rv = txXPathNodeUtils::getBaseURI(*mSourceDocument, baseURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
    }

    // Technically the hash holds documents, but we allow any node that we're transforming
    // from. In particular, the document() function uses this hash and it can return the
    // source document, but if we're transforming from a document fragment (through
    // txMozillaXSLTProcessor::SetSourceContentModel/txMozillaXSLTProcessor::DoTransform)
    // or from another type of node (through txMozillaXSLTProcessor::TransformToDocument
    // or txMozillaXSLTProcessor::TransformToFragment) it makes more sense to return the
    // real root of the source tree, which is the node where the transform started.
    PutEntry(baseURI)->mDocument = txXPathNativeNode::createXPathNode(txXPathNativeNode::getNode(aSource));
    return NS_OK;
}

txLoadedDocumentsHash::~txLoadedDocumentsHash()
{
    if (mSourceDocument) {
        nsAutoString baseURI;
        nsresult rv = txXPathNodeUtils::getBaseURI(*mSourceDocument, baseURI);
        if (NS_SUCCEEDED(rv)) {
	    txLoadedDocumentEntry* entry = GetEntry(baseURI);
	    if (entry) {
	        delete entry->mDocument.forget();
	    }
       }
    }
}

txExecutionState::txExecutionState(txStylesheet* aStylesheet,
                                   bool aDisableLoads)
    : mOutputHandler(nullptr),
      mResultHandler(nullptr),
      mStylesheet(aStylesheet),
      mNextInstruction(nullptr),
      mLocalVariables(nullptr),
      mRecursionDepth(0),
      mEvalContext(nullptr),
      mInitialEvalContext(nullptr),
      mGlobalParams(nullptr),
      mKeyHash(aStylesheet->getKeyMap()),
      mDisableLoads(aDisableLoads)
{
    MOZ_COUNT_CTOR(txExecutionState);
}

txExecutionState::~txExecutionState()
{
    MOZ_COUNT_DTOR(txExecutionState);

    delete mResultHandler;
    delete mLocalVariables;
    if (mEvalContext != mInitialEvalContext) {
        delete mEvalContext;
    }

    txStackIterator varsIter(&mLocalVarsStack);
    while (varsIter.hasNext()) {
        delete (txVariableMap*)varsIter.next();
    }

    txStackIterator contextIter(&mEvalContextStack);
    while (contextIter.hasNext()) {
        txIEvalContext* context = (txIEvalContext*)contextIter.next();
        if (context != mInitialEvalContext) {
            delete context;
        }
    }

    txStackIterator handlerIter(&mResultHandlerStack);
    while (handlerIter.hasNext()) {
        delete (txAXMLEventHandler*)handlerIter.next();
    }

    delete mInitialEvalContext;
}

nsresult
txExecutionState::init(const txXPathNode& aNode,
                       txOwningExpandedNameMap<txIGlobalParameter>* aGlobalParams)
{
    nsresult rv = NS_OK;

    mGlobalParams = aGlobalParams;

    // Set up initial context
    mEvalContext = new txSingleNodeContext(aNode, this);
    mInitialEvalContext = mEvalContext;

    // Set up output and result-handler
    txAXMLEventHandler* handler;
    rv = mOutputHandlerFactory->
        createHandlerWith(mStylesheet->getOutputFormat(), &handler);
    NS_ENSURE_SUCCESS(rv, rv);

    mOutputHandler = handler;
    mResultHandler = handler;
    mOutputHandler->startDocument();

    // Set up loaded-documents-hash
    rv = mLoadedDocuments.init(aNode);
    NS_ENSURE_SUCCESS(rv, rv);

    // Init members
    rv = mKeyHash.init();
    NS_ENSURE_SUCCESS(rv, rv);

    mRecycler = new txResultRecycler;

    // The actual value here doesn't really matter since noone should use this
    // value. But lets put something errorlike in just in case
    mGlobalVarPlaceholderValue = new StringResult(NS_LITERAL_STRING("Error"), nullptr);

    // Initiate first instruction. This has to be done last since findTemplate
    // might use us.
    txStylesheet::ImportFrame* frame = 0;
    txExpandedName nullName;
    txInstruction* templ;
    rv = mStylesheet->findTemplate(aNode, nullName, this, nullptr, &templ,
                                   &frame);
    NS_ENSURE_SUCCESS(rv, rv);

    pushTemplateRule(frame, nullName, nullptr);

    return runTemplate(templ);
}

nsresult
txExecutionState::end(nsresult aResult)
{
    NS_ASSERTION(NS_FAILED(aResult) || mTemplateRules.Length() == 1,
                 "Didn't clean up template rules properly");
    if (NS_SUCCEEDED(aResult)) {
        popTemplateRule();
    }
    else if (!mOutputHandler) {
        return NS_OK;
    }
    return mOutputHandler->endDocument(aResult);
}

void
txExecutionState::popAndDeleteEvalContextUntil(txIEvalContext* aContext)
{
  auto ctx = popEvalContext();
  while (ctx && ctx != aContext) {
    MOZ_RELEASE_ASSERT(ctx != mInitialEvalContext);
    delete ctx;
    ctx = popEvalContext();
  }
}

nsresult
txExecutionState::getVariable(int32_t aNamespace, nsAtom* aLName,
                              txAExprResult*& aResult)
{
    nsresult rv = NS_OK;
    txExpandedName name(aNamespace, aLName);

    // look for a local variable
    if (mLocalVariables) {
        mLocalVariables->getVariable(name, &aResult);
        if (aResult) {
            return NS_OK;
        }
    }

    // look for an evaluated global variable
    mGlobalVariableValues.getVariable(name, &aResult);
    if (aResult) {
        if (aResult == mGlobalVarPlaceholderValue) {
            // XXX ErrorReport: cyclic variable-value
            NS_RELEASE(aResult);
            return NS_ERROR_XSLT_BAD_RECURSION;
        }
        return NS_OK;
    }

    // Is there perchance a global variable not evaluated yet?
    txStylesheet::GlobalVariable* var = mStylesheet->getGlobalVariable(name);
    if (!var) {
        // XXX ErrorReport: variable doesn't exist in this scope
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION((var->mExpr && !var->mFirstInstruction) ||
                 (!var->mExpr && var->mFirstInstruction),
                 "global variable should have either instruction or expression");

    // Is this a stylesheet parameter that has a value?
    if (var->mIsParam && mGlobalParams) {
        txIGlobalParameter* param = mGlobalParams->get(name);
        if (param) {
            rv = param->getValue(&aResult);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = mGlobalVariableValues.bindVariable(name, aResult);
            if (NS_FAILED(rv)) {
                NS_RELEASE(aResult);
                return rv;
            }

            return NS_OK;
        }
    }

    // Insert a placeholdervalue to protect against recursion
    rv = mGlobalVariableValues.bindVariable(name, mGlobalVarPlaceholderValue);
    NS_ENSURE_SUCCESS(rv, rv);

    // evaluate the global variable
    pushEvalContext(mInitialEvalContext);
    if (var->mExpr) {
        txVariableMap* oldVars = mLocalVariables;
        mLocalVariables = nullptr;
        rv = var->mExpr->evaluate(getEvalContext(), &aResult);
        mLocalVariables = oldVars;

        if (NS_FAILED(rv)) {
          popAndDeleteEvalContextUntil(mInitialEvalContext);
          return rv;
        }
    }
    else {
        nsAutoPtr<txRtfHandler> rtfHandler(new txRtfHandler);

        rv = pushResultHandler(rtfHandler);
        if (NS_FAILED(rv)) {
          popAndDeleteEvalContextUntil(mInitialEvalContext);
          return rv;
        }

        rtfHandler.forget();

        txInstruction* prevInstr = mNextInstruction;
        // set return to nullptr to stop execution
        mNextInstruction = nullptr;
        rv = runTemplate(var->mFirstInstruction);
        if (NS_FAILED(rv)) {
          popAndDeleteEvalContextUntil(mInitialEvalContext);
          return rv;
        }

        pushTemplateRule(nullptr, txExpandedName(), nullptr);
        rv = txXSLTProcessor::execute(*this);
        if (NS_FAILED(rv)) {
          popAndDeleteEvalContextUntil(mInitialEvalContext);
          return rv;
        }

        popTemplateRule();

        mNextInstruction = prevInstr;
        rtfHandler = (txRtfHandler*)popResultHandler();
        rv = rtfHandler->getAsRTF(&aResult);
        if (NS_FAILED(rv)) {
          popAndDeleteEvalContextUntil(mInitialEvalContext);
          return rv;
        }
    }
    popEvalContext();

    // Remove the placeholder and insert the calculated value
    mGlobalVariableValues.removeVariable(name);
    rv = mGlobalVariableValues.bindVariable(name, aResult);
    if (NS_FAILED(rv)) {
        NS_RELEASE(aResult);

        return rv;
    }

    return NS_OK;
}

nsresult
txExecutionState::isStripSpaceAllowed(const txXPathNode& aNode, bool& aAllowed)
{
    return mStylesheet->isStripSpaceAllowed(aNode, this, aAllowed);
}

void*
txExecutionState::getPrivateContext()
{
    return this;
}

txResultRecycler*
txExecutionState::recycler()
{
    return mRecycler;
}

void
txExecutionState::receiveError(const nsAString& aMsg, nsresult aRes)
{
    // XXX implement me
}

nsresult
txExecutionState::pushEvalContext(txIEvalContext* aContext)
{
    nsresult rv = mEvalContextStack.push(mEvalContext);
    NS_ENSURE_SUCCESS(rv, rv);

    mEvalContext = aContext;

    return NS_OK;
}

txIEvalContext*
txExecutionState::popEvalContext()
{
    txIEvalContext* prev = mEvalContext;
    mEvalContext = (txIEvalContext*)mEvalContextStack.pop();

    return prev;
}

nsresult
txExecutionState::pushBool(bool aBool)
{
    return mBoolStack.AppendElement(aBool) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

bool
txExecutionState::popBool()
{
    NS_ASSERTION(mBoolStack.Length(), "popping from empty stack");
    uint32_t last = mBoolStack.Length() - 1;
    NS_ENSURE_TRUE(last != (uint32_t)-1, false);

    bool res = mBoolStack.ElementAt(last);
    mBoolStack.RemoveElementAt(last);

    return res;
}

nsresult
txExecutionState::pushResultHandler(txAXMLEventHandler* aHandler)
{
    nsresult rv = mResultHandlerStack.push(mResultHandler);
    NS_ENSURE_SUCCESS(rv, rv);

    mResultHandler = aHandler;

    return NS_OK;
}

txAXMLEventHandler*
txExecutionState::popResultHandler()
{
    txAXMLEventHandler* oldHandler = mResultHandler;
    mResultHandler = (txAXMLEventHandler*)mResultHandlerStack.pop();

    return oldHandler;
}

void
txExecutionState::pushTemplateRule(txStylesheet::ImportFrame* aFrame,
                                   const txExpandedName& aMode,
                                   txParameterMap* aParams)
{
    TemplateRule* rule = mTemplateRules.AppendElement();
    rule->mFrame = aFrame;
    rule->mModeNsId = aMode.mNamespaceID;
    rule->mModeLocalName = aMode.mLocalName;
    rule->mParams = aParams;
}

void
txExecutionState::popTemplateRule()
{
    NS_PRECONDITION(!mTemplateRules.IsEmpty(), "No rules to pop");
    mTemplateRules.RemoveElementAt(mTemplateRules.Length() - 1);
}

txIEvalContext*
txExecutionState::getEvalContext()
{
    return mEvalContext;
}

const txXPathNode*
txExecutionState::retrieveDocument(const nsAString& aUri)
{
    NS_ASSERTION(!aUri.Contains(char16_t('#')),
                 "Remove the fragment.");

    if (mDisableLoads) {
        return nullptr;
    }

    MOZ_LOG(txLog::xslt, LogLevel::Debug,
           ("Retrieve Document %s", NS_LossyConvertUTF16toASCII(aUri).get()));

    // try to get already loaded document
    txLoadedDocumentEntry *entry = mLoadedDocuments.PutEntry(aUri);
    if (!entry) {
        return nullptr;
    }

    if (!entry->mDocument && !entry->LoadingFailed()) {
        // open URI
        nsAutoString errMsg;
        // XXX we should get the loader from the actual node
        // triggering the load, but this will do for the time being
        entry->mLoadResult =
            txParseDocumentFromURI(aUri, *mLoadedDocuments.mSourceDocument,
                                   errMsg, getter_Transfers(entry->mDocument));

        if (entry->LoadingFailed()) {
            receiveError(NS_LITERAL_STRING("Couldn't load document '") +
                         aUri + NS_LITERAL_STRING("': ") + errMsg,
                         entry->mLoadResult);
        }
    }

    return entry->mDocument;
}

nsresult
txExecutionState::getKeyNodes(const txExpandedName& aKeyName,
                              const txXPathNode& aRoot,
                              const nsAString& aKeyValue,
                              bool aIndexIfNotFound,
                              txNodeSet** aResult)
{
    return mKeyHash.getKeyNodes(aKeyName, aRoot, aKeyValue,
                                aIndexIfNotFound, *this, aResult);
}

txExecutionState::TemplateRule*
txExecutionState::getCurrentTemplateRule()
{
    NS_PRECONDITION(!mTemplateRules.IsEmpty(), "No current rule!");
    return &mTemplateRules[mTemplateRules.Length() - 1];
}

txInstruction*
txExecutionState::getNextInstruction()
{
    txInstruction* instr = mNextInstruction;
    if (instr) {
        mNextInstruction = instr->mNext;
    }

    return instr;
}

nsresult
txExecutionState::runTemplate(txInstruction* aTemplate)
{
    NS_ENSURE_TRUE(++mRecursionDepth < kMaxRecursionDepth,
                   NS_ERROR_XSLT_BAD_RECURSION);

    nsresult rv = mLocalVarsStack.push(mLocalVariables);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mReturnStack.push(mNextInstruction);
    NS_ENSURE_SUCCESS(rv, rv);

    mLocalVariables = nullptr;
    mNextInstruction = aTemplate;

    return NS_OK;
}

void
txExecutionState::gotoInstruction(txInstruction* aNext)
{
    mNextInstruction = aNext;
}

void
txExecutionState::returnFromTemplate()
{
    --mRecursionDepth;
    NS_ASSERTION(!mReturnStack.isEmpty() && !mLocalVarsStack.isEmpty(),
                 "return or variable stack is empty");
    delete mLocalVariables;
    mNextInstruction = (txInstruction*)mReturnStack.pop();
    mLocalVariables = (txVariableMap*)mLocalVarsStack.pop();
}

nsresult
txExecutionState::bindVariable(const txExpandedName& aName,
                               txAExprResult* aValue)
{
    if (!mLocalVariables) {
        mLocalVariables = new txVariableMap;
    }
    return mLocalVariables->bindVariable(aName, aValue);
}

void
txExecutionState::removeVariable(const txExpandedName& aName)
{
    mLocalVariables->removeVariable(aName);
}

void
txExecutionState::pushParamMap(txParameterMap* aParams)
{
    mParamStack.AppendElement(mTemplateParams.forget());
    mTemplateParams = aParams;
}

already_AddRefed<txParameterMap>
txExecutionState::popParamMap()
{
    RefPtr<txParameterMap> oldParams = mTemplateParams.forget();
    mTemplateParams = mParamStack.LastElement();
    mParamStack.RemoveElementAt(mParamStack.Length() - 1);

    return oldParams.forget();
}
