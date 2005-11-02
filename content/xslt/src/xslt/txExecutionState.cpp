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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * Jonas Sicking. All Rights Reserved.
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

#include "txExecutionState.h"
#include "txSingleNodeContext.h"
#include "txInstructions.h"
#include "txStylesheet.h"
#include "txVariableMap.h"
#include "txRtfHandler.h"
#include "txXSLTProcessor.h"
#include "TxLog.h"
#include "txURIUtils.h"
#include "txXMLParser.h"

#ifndef TX_EXE
#include "nsIDOMDocument.h"
#endif

const PRInt32 txExecutionState::kMaxRecursionDepth = 20000;

DHASH_WRAPPER(txLoadedDocumentsBase, txLoadedDocumentEntry, nsAString&)

nsresult txLoadedDocumentsHash::init(Document* aSourceDocument)
{
    nsresult rv = Init(8);
    NS_ENSURE_SUCCESS(rv, rv);

    mSourceDocument = aSourceDocument;
    Add(mSourceDocument);

    return NS_OK;
}

txLoadedDocumentsHash::~txLoadedDocumentsHash()
{
    if (!mHashTable.ops) {
        return;
    }

    nsAutoString baseURI;
    mSourceDocument->getBaseURI(baseURI);
    txLoadedDocumentEntry* entry = GetEntry(baseURI);
    if (entry) {
        entry->mDocument = nsnull;
    }
}

void txLoadedDocumentsHash::Add(Document* aDocument)
{
    nsAutoString baseURI;
    aDocument->getBaseURI(baseURI);
    txLoadedDocumentEntry* entry = AddEntry(baseURI);
    if (entry) {
        entry->mDocument = aDocument;
    }
}

Document* txLoadedDocumentsHash::Get(const nsAString& aURI)
{
    txLoadedDocumentEntry* entry = GetEntry(aURI);
    if (entry) {
        return entry->mDocument;
    }

    return nsnull;
}

txExecutionState::txExecutionState(txStylesheet* aStylesheet)
    : mStylesheet(aStylesheet),
      mNextInstruction(nsnull),
      mLocalVariables(nsnull),
      mRecursionDepth(0),
      mTemplateRules(nsnull),
      mTemplateRulesBufferSize(0),
      mTemplateRuleCount(0),
      mEvalContext(nsnull),
      mInitialEvalContext(nsnull),
      mRTFDocument(nsnull),
      mGlobalParams(nsnull),
      mKeyHash(aStylesheet->getKeyMap())
{
}

txExecutionState::~txExecutionState()
{
    delete mResultHandler;
    delete mLocalVariables;
    delete mEvalContext;
    delete mRTFDocument;

    PRInt32 i;
    for (i = 0; i < mTemplateRuleCount; ++i) {
        NS_IF_RELEASE(mTemplateRules[i].mModeLocalName);
    }
    delete [] mTemplateRules;
    
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

    txStackIterator paramIter(&mParamStack);
    while (paramIter.hasNext()) {
        delete (txExpandedNameMap*)paramIter.next();
    }
}

nsresult
txExecutionState::init(Node* aNode,
                       txExpandedNameMap* aGlobalParams)
{
    nsresult rv = NS_OK;

    mGlobalParams = aGlobalParams;

    // Set up initial context
    mEvalContext = new txSingleNodeContext(aNode, this);
    NS_ENSURE_TRUE(mEvalContext, NS_ERROR_OUT_OF_MEMORY);

    mInitialEvalContext = mEvalContext;

    // Set up output and result-handler
    txAXMLEventHandler* handler = 0;
    rv = mOutputHandlerFactory->
        createHandlerWith(mStylesheet->getOutputFormat(), &handler);
    NS_ENSURE_SUCCESS(rv, rv);

    mOutputHandler = handler;
    mResultHandler = handler;
    mOutputHandler->startDocument();

    // Initiate first instruction
    txStylesheet::ImportFrame* frame = 0;
    txExpandedName nullName;
    txInstruction* templ = mStylesheet->findTemplate(aNode, nullName,
                                                     this, nsnull, &frame);
    pushTemplateRule(frame, nullName, nsnull);
    rv = runTemplate(templ);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set up loaded-documents-hash
    Document* sourceDoc;
    if (aNode->getNodeType() == Node::DOCUMENT_NODE) {
        sourceDoc = (Document*)aNode;
    }
    else {
        sourceDoc = aNode->getOwnerDocument();
    }
    rv = mLoadedDocuments.init(sourceDoc);
    NS_ENSURE_SUCCESS(rv, rv);

    // Init members    
    rv = mKeyHash.init();
    NS_ENSURE_SUCCESS(rv, rv);
    
    mRecycler = new txResultRecycler;
    NS_ENSURE_TRUE(mRecycler, NS_ERROR_OUT_OF_MEMORY);
    
    rv = mRecycler->init();
    NS_ENSURE_SUCCESS(rv, rv);
    
    // The actual value here doesn't really matter since noone should use this
    // value. But lets put something errorlike in just in case
    mGlobalVarPlaceholderValue = new StringResult(NS_LITERAL_STRING("Error"), nsnull);
    NS_ENSURE_TRUE(mGlobalVarPlaceholderValue, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

nsresult
txExecutionState::end()
{
    popTemplateRule();
    mOutputHandler->endDocument();
    
    return NS_OK;
}



nsresult
txExecutionState::getVariable(PRInt32 aNamespace, nsIAtom* aLName,
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
    
    NS_ASSERTION(var->mExpr && !var->mFirstInstruction ||
                 !var->mExpr && var->mFirstInstruction,
                 "global variable should have either instruction or expression");

    // Is this a stylesheet parameter that has a value?
    if (var->mIsParam && mGlobalParams) {
        txIGlobalParameter* param =
            (txIGlobalParameter*)mGlobalParams->get(name);
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
        mLocalVariables = nsnull;
        rv = var->mExpr->evaluate(getEvalContext(), &aResult);
        NS_ENSURE_SUCCESS(rv, rv);

        mLocalVariables = oldVars;
    }
    else {
        nsAutoPtr<txRtfHandler> rtfHandler(new txRtfHandler);
        NS_ENSURE_TRUE(rtfHandler, NS_ERROR_OUT_OF_MEMORY);

        rv = pushResultHandler(rtfHandler);
        NS_ENSURE_SUCCESS(rv, rv);
        
        rtfHandler.forget();

        txInstruction* prevInstr = mNextInstruction;
        // set return to nsnull to stop execution
        mNextInstruction = nsnull;
        rv = runTemplate(var->mFirstInstruction);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = pushTemplateRule(nsnull, txExpandedName(), nsnull);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = txXSLTProcessor::execute(*this);
        NS_ENSURE_SUCCESS(rv, rv);

        popTemplateRule();

        mNextInstruction = prevInstr;
        rtfHandler = (txRtfHandler*)popResultHandler();
        rv = rtfHandler->getAsRTF(&aResult);
        NS_ENSURE_SUCCESS(rv, rv);
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

PRBool
txExecutionState::isStripSpaceAllowed(Node* aNode)
{
    return mStylesheet->isStripSpaceAllowed(aNode, this);
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
txExecutionState::pushString(const nsAString& aStr)
{
    if (!mStringStack.AppendString(aStr)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    return NS_OK;
}

void
txExecutionState::popString(nsAString& aStr)
{
    PRInt32 count = mStringStack.Count() - 1;
    NS_ASSERTION(count >= 0, "stack is empty");
    mStringStack.StringAt(count, aStr);
    mStringStack.RemoveStringAt(count);
}

nsresult
txExecutionState::pushInt(PRInt32 aInt)
{
    return mIntStack.push(NS_INT32_TO_PTR(aInt));
}

PRInt32
txExecutionState::popInt()
{
    return NS_PTR_TO_INT32(mIntStack.pop());
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

nsresult
txExecutionState::pushTemplateRule(txStylesheet::ImportFrame* aFrame,
                                   const txExpandedName& aMode,
                                   txVariableMap* aParams)
{
    if (mTemplateRuleCount == mTemplateRulesBufferSize) {
        PRInt32 newSize =
            mTemplateRulesBufferSize ? mTemplateRulesBufferSize * 2 : 10;
        TemplateRule* newRules = new TemplateRule[newSize];
        NS_ENSURE_TRUE(newRules, NS_ERROR_OUT_OF_MEMORY);
        
        memcpy(newRules, mTemplateRules,
               mTemplateRuleCount * sizeof(TemplateRule));
        delete [] mTemplateRules;
        mTemplateRules = newRules;
        mTemplateRulesBufferSize = newSize;
    }

    mTemplateRules[mTemplateRuleCount].mFrame = aFrame;
    mTemplateRules[mTemplateRuleCount].mModeNsId = aMode.mNamespaceID;
    mTemplateRules[mTemplateRuleCount].mModeLocalName = aMode.mLocalName;
    mTemplateRules[mTemplateRuleCount].mParams = aParams;
    NS_IF_ADDREF(mTemplateRules[mTemplateRuleCount].mModeLocalName);
    ++mTemplateRuleCount;
    
    return NS_OK;
}

void
txExecutionState::popTemplateRule()
{
    // decrement outside of RELEASE, that would decrement twice
    --mTemplateRuleCount;
    NS_IF_RELEASE(mTemplateRules[mTemplateRuleCount].mModeLocalName);
}

txIEvalContext*
txExecutionState::getEvalContext()
{
    return mEvalContext;
}

Node*
txExecutionState::retrieveDocument(const nsAString& uri,
                                   const nsAString& baseUri)
{
    nsAutoString absUrl;
    URIUtils::resolveHref(uri, baseUri, absUrl);

    PRInt32 hash = absUrl.RFindChar(PRUnichar('#'));
    PRUint32 urlEnd, fragStart, fragEnd;
    if (hash == kNotFound) {
        urlEnd = absUrl.Length();
        fragStart = 0;
        fragEnd = 0;
    }
    else {
        urlEnd = hash;
        fragStart = hash + 1;
        fragEnd = absUrl.Length();
    }

    nsDependentSubstring docUrl(absUrl, 0, urlEnd);
    nsDependentSubstring frag(absUrl, fragStart, fragEnd);

    PR_LOG(txLog::xslt, PR_LOG_DEBUG,
           ("Retrieve Document %s, uri %s, baseUri %s, fragment %s\n", 
            NS_LossyConvertUCS2toASCII(docUrl).get(),
            NS_LossyConvertUCS2toASCII(uri).get(),
            NS_LossyConvertUCS2toASCII(baseUri).get(),
            NS_LossyConvertUCS2toASCII(frag).get()));

    // try to get already loaded document
    Document* xmlDoc = mLoadedDocuments.Get(docUrl);

    if (!xmlDoc) {
        // open URI
        nsAutoString errMsg, refUri;
        // XXX we should get the referrer from the actual node
        // triggering the load, but this will do for the time being
        mLoadedDocuments.mSourceDocument->getBaseURI(refUri);
        nsresult rv;
        rv = txParseDocumentFromURI(docUrl, refUri,
                                    mLoadedDocuments.mSourceDocument, errMsg,
                                    &xmlDoc);

        if (NS_FAILED(rv) || !xmlDoc) {
            receiveError(NS_LITERAL_STRING("Couldn't load document '") +
                         docUrl + NS_LITERAL_STRING("': ") + errMsg, rv);
            return NULL;
        }
        // add to list of documents
        mLoadedDocuments.Add(xmlDoc);
    }

    // return element with supplied id if supplied
    if (!frag.IsEmpty())
        return xmlDoc->getElementById(frag);

    return xmlDoc;
}

nsresult
txExecutionState::getKeyNodes(const txExpandedName& aKeyName,
                              Document* aDocument,
                              const nsAString& aKeyValue,
                              PRBool aIndexIfNotFound,
                              NodeSet** aResult)
{
    return mKeyHash.getKeyNodes(aKeyName, aDocument, aKeyValue,
                                aIndexIfNotFound, *this, aResult);
}

txExecutionState::TemplateRule*
txExecutionState::getCurrentTemplateRule()
{
    return mTemplateRules + mTemplateRuleCount - 1;
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
    
    mLocalVariables = nsnull;
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
        NS_ENSURE_TRUE(mLocalVariables, NS_ERROR_OUT_OF_MEMORY);
    }
    return mLocalVariables->bindVariable(aName, aValue);
}

void
txExecutionState::removeVariable(const txExpandedName& aName)
{
    mLocalVariables->removeVariable(aName);
}

nsresult
txExecutionState::pushParamMap(txVariableMap* aParams)
{
    nsresult rv = mParamStack.push(mTemplateParams);
    NS_ENSURE_SUCCESS(rv, rv);

    mTemplateParams.forget();
    mTemplateParams = aParams;
    
    return NS_OK;
}

txVariableMap*
txExecutionState::popParamMap()
{
    txVariableMap* oldParams = mTemplateParams.forget();
    mTemplateParams = (txVariableMap*)mParamStack.pop();

    return oldParams;
}
