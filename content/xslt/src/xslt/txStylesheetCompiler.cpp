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

#include "txStylesheetCompiler.h"
#include "txStylesheetCompileHandlers.h"
#include "txAtoms.h"
#include "txURIUtils.h"
#include "txTokenizer.h"
#include "txStylesheet.h"
#include "txInstructions.h"
#include "txToplevelItems.h"
#include "ExprParser.h"
#include "TxLog.h"
#include "txPatternParser.h"
#include "txStringUtils.h"
#include "XSLTFunctions.h"

txStylesheetCompiler::txStylesheetCompiler(const nsAString& aBaseURI,
                                           txACompileObserver* aObserver)
    : txStylesheetCompilerState(aObserver)
{
    mStatus = init(aBaseURI, nsnull, nsnull);
}

txStylesheetCompiler::txStylesheetCompiler(const nsAString& aBaseURI,
                                           txStylesheet* aStylesheet,
                                           txListIterator* aInsertPosition,
                                           txACompileObserver* aObserver)
    : txStylesheetCompilerState(aObserver)
{
    mStatus = init(aBaseURI, aStylesheet, aInsertPosition);
}

nsrefcnt
txStylesheetCompiler::AddRef()
{
    return ++mRefCnt;
}

nsrefcnt
txStylesheetCompiler::Release()
{
    if (--mRefCnt == 0) {
        mRefCnt = 1; //stabilize
        delete this;
        return 0;
    }
    return mRefCnt;
}

nsresult
txStylesheetCompiler::startElement(PRInt32 aNamespaceID, nsIAtom* aLocalName,
                                   nsIAtom* aPrefix,
                                   txStylesheetAttr* aAttributes,
                                   PRInt32 aAttrCount)
{
    if (NS_FAILED(mStatus)) {
        // ignore content after failure
        // XXX reevaluate once expat stops on failure
        return NS_OK;
    }

    nsresult rv = flushCharacters();
    NS_ENSURE_SUCCESS(rv, rv);

    // look for new namespace mappings
    PRBool hasOwnNamespaceMap = PR_FALSE;
    PRInt32 i;
    for (i = 0; i < aAttrCount; ++i) {
        txStylesheetAttr* attr = aAttributes + i;
        if (attr->mNamespaceID == kNameSpaceID_XMLNS) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (!hasOwnNamespaceMap) {
                mElementContext->mMappings =
                    new txNamespaceMap(*mElementContext->mMappings);
                NS_ENSURE_TRUE(mElementContext->mMappings,
                               NS_ERROR_OUT_OF_MEMORY);
                hasOwnNamespaceMap = PR_TRUE;
            }

            if (attr->mLocalName == txXMLAtoms::xmlns) {
                mElementContext->mMappings->addNamespace(nsnull, attr->mValue);
            }
            else {
                mElementContext->mMappings->
                    addNamespace(attr->mLocalName, attr->mValue);
            }
        }
    }

    return startElementInternal(aNamespaceID, aLocalName, aPrefix,
                                aAttributes, aAttrCount);
}

nsresult
txStylesheetCompiler::startElement(const PRUnichar *aName,
                                   const PRUnichar **aAttrs,
                                   PRInt32 aAttrCount, PRInt32 aIDOffset)
{
    if (NS_FAILED(mStatus)) {
        // ignore content after failure
        // XXX reevaluate once expat stops on failure
        return NS_OK;
    }

    nsresult rv = flushCharacters();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoArrayPtr<txStylesheetAttr> atts;
    if (aAttrCount > 0) {
        atts = new txStylesheetAttr[aAttrCount];
        NS_ENSURE_TRUE(atts, NS_ERROR_OUT_OF_MEMORY);
    }

    PRBool hasOwnNamespaceMap = PR_FALSE;
    PRInt32 i;
    for (i = 0; i < aAttrCount; ++i) {
        rv = XMLUtils::splitXMLName(nsDependentString(aAttrs[i * 2]),
                                    getter_AddRefs(atts[i].mPrefix),
                                    getter_AddRefs(atts[i].mLocalName));
        NS_ENSURE_SUCCESS(rv, rv);
        atts[i].mValue.Append(aAttrs[i * 2 + 1]);

        nsCOMPtr<nsIAtom> prefixToBind;
        if (atts[i].mPrefix == txXMLAtoms::xmlns) {
            prefixToBind = atts[i].mLocalName;
        }
        else if (!atts[i].mPrefix && atts[i].mLocalName == txXMLAtoms::xmlns) {
            prefixToBind = txXMLAtoms::_empty;
        }

        if (prefixToBind) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (!hasOwnNamespaceMap) {
                mElementContext->mMappings =
                    new txNamespaceMap(*mElementContext->mMappings);
                NS_ENSURE_TRUE(mElementContext->mMappings,
                               NS_ERROR_OUT_OF_MEMORY);
                hasOwnNamespaceMap = PR_TRUE;
            }

            rv = mElementContext->mMappings->
                addNamespace(prefixToBind, atts[i].mValue);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    for (i = 0; i < aAttrCount; ++i) {
        if (atts[i].mPrefix && atts[i].mPrefix != txXMLAtoms::xmlns) {
            atts[i].mNamespaceID =
                mElementContext->mMappings->lookupNamespace(atts[i].mPrefix);
            NS_ENSURE_TRUE(atts[i].mNamespaceID != kNameSpaceID_Unknown,
                           NS_ERROR_FAILURE);
        }
        else if (atts[i].mPrefix == txXMLAtoms::xmlns || 
                 (!atts[i].mPrefix && 
                  atts[i].mLocalName == txXMLAtoms::xmlns)) {
            atts[i].mNamespaceID = kNameSpaceID_XMLNS;
        }
        else {
            atts[i].mNamespaceID = kNameSpaceID_None;
        }
    }

    nsCOMPtr<nsIAtom> prefix, localname;
    rv = XMLUtils::splitXMLName(nsDependentString(aName),
                                getter_AddRefs(prefix),
                                getter_AddRefs(localname));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 namespaceID = mElementContext->mMappings->lookupNamespace(prefix);

    NS_ENSURE_TRUE(namespaceID != kNameSpaceID_Unknown, NS_ERROR_FAILURE);

    PRInt32 idOffset = aIDOffset;
    if (idOffset > 0) {
        idOffset /= 2;
    }
    return startElementInternal(namespaceID, localname, prefix, atts,
                                aAttrCount, idOffset);
}

nsresult
txStylesheetCompiler::startElementInternal(PRInt32 aNamespaceID,
                                           nsIAtom* aLocalName,
                                           nsIAtom* aPrefix,
                                           txStylesheetAttr* aAttributes,
                                           PRInt32 aAttrCount,
                                           PRInt32 aIDOffset)
{
    nsresult rv = NS_OK;
    PRInt32 i;
    for (i = mInScopeVariables.Count() - 1; i >= 0; --i) {
        ++(NS_STATIC_CAST(txInScopeVariable*, mInScopeVariables[i]))->mLevel;
    }

    // Update the elementcontext if we have special attributes
    for (i = 0; i < aAttrCount; ++i) {
        txStylesheetAttr* attr = aAttributes + i;

        // xml:space
        if (attr->mNamespaceID == kNameSpaceID_XML &&
            attr->mLocalName == txXMLAtoms::space) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (TX_StringEqualsAtom(attr->mValue, txXMLAtoms::preserve)) {
                mElementContext->mPreserveWhitespace = MB_TRUE;
            }
            else if (TX_StringEqualsAtom(attr->mValue, txXMLAtoms::_default)) {
                mElementContext->mPreserveWhitespace = MB_FALSE;
            }
            else {
                return NS_ERROR_XSLT_PARSE_FAILURE;
            }
        }

        // xml:base
        if (attr->mNamespaceID == kNameSpaceID_XML &&
            attr->mLocalName == txXMLAtoms::base &&
            !attr->mValue.IsEmpty()) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);
            
            nsAutoString uri;
            URIUtils::resolveHref(attr->mValue, mElementContext->mBaseURI, uri);
            mElementContext->mBaseURI = uri;
        }

        // extension-element-prefixes
        if ((attr->mNamespaceID == kNameSpaceID_XSLT &&
             attr->mLocalName == txXSLTAtoms::extensionElementPrefixes &&
             aNamespaceID != kNameSpaceID_XSLT) ||
            (attr->mNamespaceID == kNameSpaceID_None &&
             attr->mLocalName == txXSLTAtoms::extensionElementPrefixes &&
             aNamespaceID == kNameSpaceID_XSLT &&
             (aLocalName == txXSLTAtoms::stylesheet ||
              aLocalName == txXSLTAtoms::transform))) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            txTokenizer tok(attr->mValue);
            while (tok.hasMoreTokens()) {
                PRInt32 namespaceID = mElementContext->mMappings->
                    lookupNamespaceWithDefault(tok.nextToken());
                
                if (namespaceID == kNameSpaceID_Unknown)
                    return NS_ERROR_XSLT_PARSE_FAILURE;

                if (!mElementContext->mInstructionNamespaces.
                        AppendElement(NS_INT32_TO_PTR(namespaceID))) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }

        // version
        if ((attr->mNamespaceID == kNameSpaceID_XSLT &&
             attr->mLocalName == txXSLTAtoms::version &&
             aNamespaceID != kNameSpaceID_XSLT) ||
            (attr->mNamespaceID == kNameSpaceID_None &&
             attr->mLocalName == txXSLTAtoms::version &&
             aNamespaceID == kNameSpaceID_XSLT &&
             (aLocalName == txXSLTAtoms::stylesheet ||
              aLocalName == txXSLTAtoms::transform))) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (attr->mValue.EqualsLiteral("1.0")) {
                mElementContext->mForwardsCompatibleParsing = MB_FALSE;
            }
            else {
                mElementContext->mForwardsCompatibleParsing = MB_TRUE;
            }
        }
    }

    // Find the right elementhandler and execute it
    MBool isInstruction = MB_FALSE;
    PRInt32 count = mElementContext->mInstructionNamespaces.Count();
    for (i = 0; i < count; ++i) {
        if (NS_PTR_TO_INT32(mElementContext->mInstructionNamespaces[i]) ==
            aNamespaceID) {
            isInstruction = MB_TRUE;
            break;
        }
    }

    if (mEmbedStatus == eNeedEmbed) {
        // handle embedded stylesheets
        if (aIDOffset >= 0 && aAttributes[aIDOffset].mValue.Equals(mTarget)) {
            // We found the right ID, signal to compile the 
            // embedded stylesheet.
            mEmbedStatus = eInEmbed;
        }
    }
    const txElementHandler* handler;
    do {
        handler = isInstruction ?
                  mHandlerTable->find(aNamespaceID, aLocalName) :
                  mHandlerTable->mLREHandler;

        rv = (handler->mStartFunction)(aNamespaceID, aLocalName, aPrefix,
                                       aAttributes, aAttrCount, *this);
    } while (rv == NS_XSLT_GET_NEW_HANDLER);

    NS_ENSURE_SUCCESS(rv, rv);

    rv = pushPtr(NS_CONST_CAST(txElementHandler*, handler));
    NS_ENSURE_SUCCESS(rv, rv);

    mElementContext->mDepth++;

    return NS_OK;
}

nsresult
txStylesheetCompiler::endElement()
{
    if (NS_FAILED(mStatus)) {
        // ignore content after failure
        // XXX reevaluate once expat stops on failure
        return NS_OK;
    }

    nsresult rv = flushCharacters();
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 i;
    for (i = mInScopeVariables.Count() - 1; i >= 0; --i) {
        txInScopeVariable* var =
            NS_STATIC_CAST(txInScopeVariable*, mInScopeVariables[i]);
        if (!--(var->mLevel)) {
            nsAutoPtr<txInstruction> instr(new txRemoveVariable(var->mName));
            NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

            rv = addInstruction(instr);
            NS_ENSURE_SUCCESS(rv, rv);
            
            mInScopeVariables.RemoveElementAt(i);
            delete var;
        }
    }

    const txElementHandler* handler =
        NS_CONST_CAST(const txElementHandler*,
                      NS_STATIC_CAST(txElementHandler*, popPtr()));
    rv = (handler->mEndFunction)(*this);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!--mElementContext->mDepth) {
        // this will delete the old object
        mElementContext = NS_STATIC_CAST(txElementContext*, popObject());
    }

    return NS_OK;
}

nsresult
txStylesheetCompiler::characters(const nsAString& aStr)
{
    if (NS_FAILED(mStatus)) {
        // ignore content after failure
        // XXX reevaluate once expat stops on failure
        return NS_OK;
    }

    mCharacters.Append(aStr);

    return NS_OK;
}

nsresult
txStylesheetCompiler::doneLoading()
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("Compiler::doneLoading: %s\n",
            NS_LossyConvertUCS2toASCII(mURI).get()));
    if (NS_FAILED(mStatus)) {
        return mStatus;
    }

    mDoneWithThisStylesheet = PR_TRUE;

    return maybeDoneCompiling();
}

void
txStylesheetCompiler::cancel(nsresult aError, const PRUnichar *aErrorText,
                             const PRUnichar *aParam)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("Compiler::cancel: %s, module: %d, code %d\n",
            NS_LossyConvertUCS2toASCII(mURI).get(),
            NS_ERROR_GET_MODULE(aError),
            NS_ERROR_GET_CODE(aError)));
    if (NS_SUCCEEDED(mStatus)) {
        mStatus = aError;
    }

    if (mObserver) {
        mObserver->onDoneCompiling(this, mStatus, aErrorText, aParam);
        // This will ensure that we don't call onDoneCompiling twice. Also
        // ensures that we don't keep the observer alive longer then necessary.
        mObserver = nsnull;
    }
}

txStylesheet*
txStylesheetCompiler::getStylesheet()
{
    return mStylesheet;
}

nsresult
txStylesheetCompiler::loadURI(const nsAString& aUri,
                              txStylesheetCompiler* aCompiler)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("Compiler::loadURI forwards %s thru %s\n",
            NS_LossyConvertUCS2toASCII(aUri).get(),
            NS_LossyConvertUCS2toASCII(mURI).get()));
    if (mURI.Equals(aUri)) {
        return NS_ERROR_XSLT_LOAD_RECURSION;
    }
    return mObserver ? mObserver->loadURI(aUri, aCompiler) : NS_ERROR_FAILURE;
}

void
txStylesheetCompiler::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                      nsresult aResult,
                                      const PRUnichar *aErrorText,
                                      const PRUnichar *aParam)
{
    if (NS_FAILED(aResult)) {
        cancel(aResult, aErrorText, aParam);
        return;
    }

    mChildCompilerList.RemoveElement(aCompiler);

    maybeDoneCompiling();
}

nsresult
txStylesheetCompiler::flushCharacters()
{
    // Bail if we don't have any characters. The handler will detect
    // ignoreable whitespace
    if (mCharacters.IsEmpty()) {
        return NS_OK;
    }

    nsresult rv = NS_OK;

    do {
        rv = (mHandlerTable->mTextHandler)(mCharacters, *this);
    } while (rv == NS_XSLT_GET_NEW_HANDLER);

    NS_ENSURE_SUCCESS(rv, rv);

    mCharacters.Truncate();

    return NS_OK;
}

nsresult
txStylesheetCompiler::ensureNewElementContext()
{
    // Do we already have a new context?
    if (!mElementContext->mDepth) {
        return NS_OK;
    }
    
    nsAutoPtr<txElementContext>
        context(new txElementContext(*mElementContext));
    NS_ENSURE_TRUE(context, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = pushObject(mElementContext);
    NS_ENSURE_SUCCESS(rv, rv);

    mElementContext.forget();
    mElementContext = context;

    return NS_OK;
}

nsresult
txStylesheetCompiler::maybeDoneCompiling()
{
    if (!mDoneWithThisStylesheet || mChildCompilerList.Count()) {
        return NS_OK;
    }
    
    if (mIsTopCompiler) {
        nsresult rv = mStylesheet->doneCompiling();
        if (NS_FAILED(rv)) {
            cancel(rv);
            return rv;
        }
    }
    
    if (mObserver) {
        mObserver->onDoneCompiling(this, mStatus);
        // This will ensure that we don't call onDoneCompiling twice. Also
        // ensures that we don't keep the observer alive longer then necessary.
        mObserver = nsnull;
    }

    return NS_OK;
}

/**
 * txStylesheetCompilerState
 */


txStylesheetCompilerState::txStylesheetCompilerState(txACompileObserver* aObserver)
    : mHandlerTable(nsnull),
      mSorter(nsnull),
      mDOE(PR_FALSE),
      mSearchingForFallback(PR_FALSE),
      mObserver(aObserver),
      mEmbedStatus(eNoEmbed),
      mDoneWithThisStylesheet(PR_FALSE),
      mNextInstrPtr(nsnull),
      mToplevelIterator(nsnull)
{
    // Embedded stylesheets have another handler, which is set in
    // txStylesheetCompiler::init if the baseURI has a fragment identifier.
    mHandlerTable = gTxRootHandler;

}

nsresult
txStylesheetCompilerState::init(const nsAString& aBaseURI,
                                txStylesheet* aStylesheet,
                                txListIterator* aInsertPosition)
{
    NS_ASSERTION(!aStylesheet || aInsertPosition,
                 "must provide insertposition if loading subsheet");
    mURI = aBaseURI;
    // Check for fragment identifier of an embedded stylesheet.
    PRInt32 fragment = aBaseURI.FindChar('#') + 1;
    if (fragment > 0) {
        PRInt32 fragmentLength = aBaseURI.Length() - fragment;
        if (fragmentLength > 0) {
            // This is really an embedded stylesheet, not just a
            // "url#". We may want to unescape the fragment.
            mTarget = Substring(aBaseURI, (PRUint32)fragment,
                                fragmentLength);
            mEmbedStatus = eNeedEmbed;
            mHandlerTable = gTxEmbedHandler;
        }
    }
    nsresult rv = NS_OK;
    if (aStylesheet) {
        mStylesheet = aStylesheet;
        mToplevelIterator = *aInsertPosition;
        mIsTopCompiler = PR_FALSE;
    }
    else {
        mStylesheet = new txStylesheet;
        NS_ENSURE_TRUE(mStylesheet, NS_ERROR_OUT_OF_MEMORY);
        
        rv = mStylesheet->init();
        NS_ENSURE_SUCCESS(rv, rv);
        
        mToplevelIterator =
            txListIterator(&mStylesheet->mRootFrame->mToplevelItems);
        mToplevelIterator.next(); // go to the end of the list
        mIsTopCompiler = PR_TRUE;
    }
   
    mElementContext = new txElementContext(aBaseURI);
    NS_ENSURE_TRUE(mElementContext && mElementContext->mMappings,
                   NS_ERROR_OUT_OF_MEMORY);

    // Push the "old" txElementContext
    rv = pushObject(0);
    NS_ENSURE_SUCCESS(rv, rv);
    
    return NS_OK;
}


txStylesheetCompilerState::~txStylesheetCompilerState()
{
    while (!mObjectStack.isEmpty()) {
        delete popObject();
    }
    
    PRInt32 i;
    for (i = mInScopeVariables.Count() - 1; i >= 0; --i) {
        delete NS_STATIC_CAST(txInScopeVariable*, mInScopeVariables[i]);
    }
}

nsresult
txStylesheetCompilerState::pushHandlerTable(txHandlerTable* aTable)
{
    nsresult rv = pushPtr(mHandlerTable);
    NS_ENSURE_SUCCESS(rv, rv);

    mHandlerTable = aTable;

    return NS_OK;
}

void
txStylesheetCompilerState::popHandlerTable()
{
    mHandlerTable = NS_STATIC_CAST(txHandlerTable*, popPtr());
}

nsresult
txStylesheetCompilerState::pushSorter(txPushNewContext* aSorter)
{
    nsresult rv = pushPtr(mSorter);
    NS_ENSURE_SUCCESS(rv, rv);

    mSorter = aSorter;

    return NS_OK;
}

void
txStylesheetCompilerState::popSorter()
{
    mSorter = NS_STATIC_CAST(txPushNewContext*, popPtr());
}

nsresult
txStylesheetCompilerState::pushChooseGotoList()
{
    nsresult rv = pushObject(mChooseGotoList);
    NS_ENSURE_SUCCESS(rv, rv);

    mChooseGotoList.forget();
    mChooseGotoList = new txList;
    NS_ENSURE_TRUE(mChooseGotoList, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

void
txStylesheetCompilerState::popChooseGotoList()
{
    // this will delete the old value
    mChooseGotoList = NS_STATIC_CAST(txList*, popObject());
}

nsresult
txStylesheetCompilerState::pushObject(TxObject* aObject)
{
    return mObjectStack.push(aObject);
}

TxObject*
txStylesheetCompilerState::popObject()
{
    return NS_STATIC_CAST(TxObject*, mObjectStack.pop());
}

nsresult
txStylesheetCompilerState::pushPtr(void* aPtr)
{
#ifdef TX_DEBUG_STACK
    PR_LOG(txLog::xslt, PR_LOG_DEBUG, ("pushPtr: %d\n", aPtr));
#endif
    return mOtherStack.push(aPtr);
}

void*
txStylesheetCompilerState::popPtr()
{
    void* value = mOtherStack.pop();
#ifdef TX_DEBUG_STACK
    PR_LOG(txLog::xslt, PR_LOG_DEBUG, ("popPtr: %d\n", value));
#endif
    return value;
}

MBool
txStylesheetCompilerState::fcp()
{
    return mElementContext->mForwardsCompatibleParsing;
}

nsresult
txStylesheetCompilerState::addToplevelItem(txToplevelItem* aItem)
{
    return mToplevelIterator.addBefore(aItem);
}

nsresult
txStylesheetCompilerState::openInstructionContainer(txInstructionContainer* aContainer)
{
    NS_PRECONDITION(!mNextInstrPtr, "can't nest instruction-containers");

    mNextInstrPtr = aContainer->mFirstInstruction.StartAssignment();
    return NS_OK;
}

void
txStylesheetCompilerState::closeInstructionContainer()
{
    NS_ASSERTION(mGotoTargetPointers.Count() == 0,
                 "GotoTargets still exists, did you forget to add txReturn?");
    mNextInstrPtr = 0;
}

nsresult
txStylesheetCompilerState::addInstruction(nsAutoPtr<txInstruction> aInstruction)
{
    NS_PRECONDITION(mNextInstrPtr, "adding instruction outside container");

    txInstruction* newInstr = aInstruction;

    *mNextInstrPtr = aInstruction.forget();
    mNextInstrPtr = &newInstr->mNext;
    
    PRInt32 i, count = mGotoTargetPointers.Count();
    for (i = 0; i < count; ++i) {
        *NS_STATIC_CAST(txInstruction**, mGotoTargetPointers[i]) = newInstr;
    }
    mGotoTargetPointers.Clear();

    return NS_OK;
}

nsresult
txStylesheetCompilerState::loadIncludedStylesheet(const nsAString& aURI)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("CompilerState::loadIncludedStylesheet: %s\n",
            NS_LossyConvertUCS2toASCII(aURI).get()));
    if (mURI.Equals(aURI)) {
        return NS_ERROR_XSLT_LOAD_RECURSION;
    }
    NS_ENSURE_TRUE(mObserver, NS_ERROR_NOT_IMPLEMENTED);

    nsAutoPtr<txToplevelItem> item(new txDummyItem);
    NS_ENSURE_TRUE(item, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = mToplevelIterator.addBefore(item);
    NS_ENSURE_SUCCESS(rv, rv);
    
    item.forget();

    // step back to the dummy-item
    mToplevelIterator.previous();
    
    txACompileObserver* observer = NS_STATIC_CAST(txStylesheetCompiler*, this);

    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(aURI, mStylesheet, &mToplevelIterator,
                                 observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    // step forward before calling the observer in case of syncronous loading
    mToplevelIterator.next();

    if (!mChildCompilerList.AppendElement(compiler)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = mObserver->loadURI(aURI, compiler);
    if (NS_FAILED(rv)) {
        mChildCompilerList.RemoveElement(compiler);
        return rv;
    }

    return NS_OK;
}

nsresult
txStylesheetCompilerState::loadImportedStylesheet(const nsAString& aURI,
                                                  txStylesheet::ImportFrame* aFrame)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("CompilerState::loadImportedStylesheet: %s\n",
            NS_LossyConvertUCS2toASCII(aURI).get()));
    if (mURI.Equals(aURI)) {
        return NS_ERROR_XSLT_LOAD_RECURSION;
    }
    NS_ENSURE_TRUE(mObserver, NS_ERROR_NOT_IMPLEMENTED);

    txListIterator iter(&aFrame->mToplevelItems);
    iter.next(); // go to the end of the list

    txACompileObserver* observer = NS_STATIC_CAST(txStylesheetCompiler*, this);

    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(aURI, mStylesheet, &iter, observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    if (!mChildCompilerList.AppendElement(compiler)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = mObserver->loadURI(aURI, compiler);
    if (NS_FAILED(rv)) {
        mChildCompilerList.RemoveElement(compiler);
        return rv;
    }

    return NS_OK;  
}

nsresult
txStylesheetCompilerState::addGotoTarget(txInstruction** aTargetPointer)
{
    if (!mGotoTargetPointers.AppendElement(aTargetPointer)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    return NS_OK;
}

nsresult
txStylesheetCompilerState::addVariable(const txExpandedName& aName)
{
    txInScopeVariable* var = new txInScopeVariable(aName);
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    if (!mInScopeVariables.AppendElement(var)) {
        delete var;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

nsresult
txStylesheetCompilerState::resolveNamespacePrefix(nsIAtom* aPrefix,
                                                  PRInt32& aID)
{
    NS_ASSERTION(aPrefix && aPrefix != txXMLAtoms::_empty,
                 "caller should handle default namespace ''");
    aID = mElementContext->mMappings->lookupNamespace(aPrefix);
    return (aID != kNameSpaceID_Unknown) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
txStylesheetCompilerState::resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                               FunctionCall*& aFunction)
{
   aFunction = nsnull;

   if (aID != kNameSpaceID_None) {
       return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
   }
   if (aName == txXSLTAtoms::document) {
       aFunction = new DocumentFunctionCall(mElementContext->mBaseURI);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }
   if (aName == txXSLTAtoms::key) {
       aFunction = new txKeyFunctionCall(mElementContext->mMappings);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }
   if (aName == txXSLTAtoms::formatNumber) {
       aFunction = new txFormatNumberFunctionCall(mStylesheet,
                                                  mElementContext->mMappings);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }
   if (aName == txXSLTAtoms::current) {
       aFunction = new CurrentFunctionCall();
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }
   if (aName == txXSLTAtoms::unparsedEntityUri) {

       return NS_ERROR_NOT_IMPLEMENTED;
   }
   if (aName == txXSLTAtoms::generateId) {
       aFunction = new GenerateIdFunctionCall();
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }
   if (aName == txXSLTAtoms::systemProperty) {
       aFunction = new SystemPropertyFunctionCall(mElementContext->mMappings);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }
   if (aName == txXSLTAtoms::elementAvailable) {
       aFunction =
          new ElementAvailableFunctionCall(mElementContext->mMappings);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }
   if (aName == txXSLTAtoms::functionAvailable) {
       aFunction =
          new FunctionAvailableFunctionCall(mElementContext->mMappings);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);

       return NS_OK;
   }

   return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
}

PRBool
txStylesheetCompilerState::caseInsensitiveNameTests()
{
    return PR_FALSE;
}

void
txStylesheetCompilerState::SetErrorOffset(PRUint32 aOffset)
{
    // XXX implement me
}

txElementContext::txElementContext(const nsAString& aBaseURI)
    : mPreserveWhitespace(PR_FALSE),
      mForwardsCompatibleParsing(PR_TRUE),
      mBaseURI(aBaseURI),
      mMappings(new txNamespaceMap),
      mDepth(0)
{
    mInstructionNamespaces.AppendElement(NS_INT32_TO_PTR(kNameSpaceID_XSLT));
}

txElementContext::txElementContext(const txElementContext& aOther)
    : mPreserveWhitespace(aOther.mPreserveWhitespace),
      mForwardsCompatibleParsing(aOther.mForwardsCompatibleParsing),
      mBaseURI(aOther.mBaseURI),
      mMappings(aOther.mMappings),
      mDepth(0)
{
      mInstructionNamespaces = aOther.mInstructionNamespaces;
}
