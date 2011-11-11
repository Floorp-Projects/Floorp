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

#include "mozilla/Util.h"

#include "txStylesheetCompiler.h"
#include "txStylesheetCompileHandlers.h"
#include "nsGkAtoms.h"
#include "txURIUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "txStylesheet.h"
#include "txInstructions.h"
#include "txToplevelItems.h"
#include "txExprParser.h"
#include "txLog.h"
#include "txPatternParser.h"
#include "txStringUtils.h"
#include "txXSLTFunctions.h"
#include "nsICategoryManager.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

using namespace mozilla;

txStylesheetCompiler::txStylesheetCompiler(const nsAString& aStylesheetURI,
                                           txACompileObserver* aObserver)
    : txStylesheetCompilerState(aObserver)
{
    mStatus = init(aStylesheetURI, nsnull, nsnull);
}

txStylesheetCompiler::txStylesheetCompiler(const nsAString& aStylesheetURI,
                                           txStylesheet* aStylesheet,
                                           txListIterator* aInsertPosition,
                                           txACompileObserver* aObserver)
    : txStylesheetCompilerState(aObserver)
{
    mStatus = init(aStylesheetURI, aStylesheet, aInsertPosition);
}

void
txStylesheetCompiler::setBaseURI(const nsString& aBaseURI)
{
    NS_ASSERTION(mObjectStack.size() == 1 && !mObjectStack.peek(),
                 "Execution already started");

    if (NS_FAILED(mStatus)) {
        return;
    }

    mElementContext->mBaseURI = aBaseURI;
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
    bool hasOwnNamespaceMap = false;
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
                hasOwnNamespaceMap = true;
            }

            if (attr->mLocalName == nsGkAtoms::xmlns) {
                mElementContext->mMappings->mapNamespace(nsnull, attr->mValue);
            }
            else {
                mElementContext->mMappings->
                    mapNamespace(attr->mLocalName, attr->mValue);
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

    bool hasOwnNamespaceMap = false;
    PRInt32 i;
    for (i = 0; i < aAttrCount; ++i) {
        rv = XMLUtils::splitExpatName(aAttrs[i * 2],
                                      getter_AddRefs(atts[i].mPrefix),
                                      getter_AddRefs(atts[i].mLocalName),
                                      &atts[i].mNamespaceID);
        NS_ENSURE_SUCCESS(rv, rv);
        atts[i].mValue.Append(aAttrs[i * 2 + 1]);

        nsCOMPtr<nsIAtom> prefixToBind;
        if (atts[i].mPrefix == nsGkAtoms::xmlns) {
            prefixToBind = atts[i].mLocalName;
        }
        else if (atts[i].mNamespaceID == kNameSpaceID_XMLNS) {
            prefixToBind = nsGkAtoms::_empty;
        }

        if (prefixToBind) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (!hasOwnNamespaceMap) {
                mElementContext->mMappings =
                    new txNamespaceMap(*mElementContext->mMappings);
                NS_ENSURE_TRUE(mElementContext->mMappings,
                               NS_ERROR_OUT_OF_MEMORY);
                hasOwnNamespaceMap = true;
            }

            rv = mElementContext->mMappings->
                mapNamespace(prefixToBind, atts[i].mValue);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    nsCOMPtr<nsIAtom> prefix, localname;
    PRInt32 namespaceID;
    rv = XMLUtils::splitExpatName(aName, getter_AddRefs(prefix),
                                  getter_AddRefs(localname), &namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

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
    for (i = mInScopeVariables.Length() - 1; i >= 0; --i) {
        ++mInScopeVariables[i]->mLevel;
    }

    // Update the elementcontext if we have special attributes
    for (i = 0; i < aAttrCount; ++i) {
        txStylesheetAttr* attr = aAttributes + i;

        // xml:space
        if (attr->mNamespaceID == kNameSpaceID_XML &&
            attr->mLocalName == nsGkAtoms::space) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (TX_StringEqualsAtom(attr->mValue, nsGkAtoms::preserve)) {
                mElementContext->mPreserveWhitespace = true;
            }
            else if (TX_StringEqualsAtom(attr->mValue, nsGkAtoms::_default)) {
                mElementContext->mPreserveWhitespace = false;
            }
            else {
                return NS_ERROR_XSLT_PARSE_FAILURE;
            }
        }

        // xml:base
        if (attr->mNamespaceID == kNameSpaceID_XML &&
            attr->mLocalName == nsGkAtoms::base &&
            !attr->mValue.IsEmpty()) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);
            
            nsAutoString uri;
            URIUtils::resolveHref(attr->mValue, mElementContext->mBaseURI, uri);
            mElementContext->mBaseURI = uri;
        }

        // extension-element-prefixes
        if ((attr->mNamespaceID == kNameSpaceID_XSLT &&
             attr->mLocalName == nsGkAtoms::extensionElementPrefixes &&
             aNamespaceID != kNameSpaceID_XSLT) ||
            (attr->mNamespaceID == kNameSpaceID_None &&
             attr->mLocalName == nsGkAtoms::extensionElementPrefixes &&
             aNamespaceID == kNameSpaceID_XSLT &&
             (aLocalName == nsGkAtoms::stylesheet ||
              aLocalName == nsGkAtoms::transform))) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            nsWhitespaceTokenizer tok(attr->mValue);
            while (tok.hasMoreTokens()) {
                PRInt32 namespaceID = mElementContext->mMappings->
                    lookupNamespaceWithDefault(tok.nextToken());
                
                if (namespaceID == kNameSpaceID_Unknown)
                    return NS_ERROR_XSLT_PARSE_FAILURE;

                if (!mElementContext->mInstructionNamespaces.
                        AppendElement(namespaceID)) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }

            attr->mLocalName = nsnull;
        }

        // version
        if ((attr->mNamespaceID == kNameSpaceID_XSLT &&
             attr->mLocalName == nsGkAtoms::version &&
             aNamespaceID != kNameSpaceID_XSLT) ||
            (attr->mNamespaceID == kNameSpaceID_None &&
             attr->mLocalName == nsGkAtoms::version &&
             aNamespaceID == kNameSpaceID_XSLT &&
             (aLocalName == nsGkAtoms::stylesheet ||
              aLocalName == nsGkAtoms::transform))) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (attr->mValue.EqualsLiteral("1.0")) {
                mElementContext->mForwardsCompatibleParsing = false;
            }
            else {
                mElementContext->mForwardsCompatibleParsing = true;
            }
        }
    }

    // Find the right elementhandler and execute it
    bool isInstruction = false;
    PRInt32 count = mElementContext->mInstructionNamespaces.Length();
    for (i = 0; i < count; ++i) {
        if (mElementContext->mInstructionNamespaces[i] == aNamespaceID) {
            isInstruction = true;
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

    if (!fcp()) {
        for (i = 0; i < aAttrCount; ++i) {
            txStylesheetAttr& attr = aAttributes[i];
            if (attr.mLocalName &&
                (attr.mNamespaceID == kNameSpaceID_XSLT ||
                 (aNamespaceID == kNameSpaceID_XSLT &&
                  attr.mNamespaceID == kNameSpaceID_None))) {
                // XXX ErrorReport: unknown attribute
                return NS_ERROR_XSLT_PARSE_FAILURE;
            }
        }
    }

    rv = pushPtr(const_cast<txElementHandler*>(handler));
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
    for (i = mInScopeVariables.Length() - 1; i >= 0; --i) {
        txInScopeVariable* var = mInScopeVariables[i];
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
        const_cast<const txElementHandler*>
                  (static_cast<txElementHandler*>(popPtr()));
    rv = (handler->mEndFunction)(*this);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!--mElementContext->mDepth) {
        // this will delete the old object
        mElementContext = static_cast<txElementContext*>(popObject());
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
            NS_LossyConvertUTF16toASCII(mStylesheetURI).get()));
    if (NS_FAILED(mStatus)) {
        return mStatus;
    }

    mDoneWithThisStylesheet = true;

    return maybeDoneCompiling();
}

void
txStylesheetCompiler::cancel(nsresult aError, const PRUnichar *aErrorText,
                             const PRUnichar *aParam)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("Compiler::cancel: %s, module: %d, code %d\n",
            NS_LossyConvertUTF16toASCII(mStylesheetURI).get(),
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
                              const nsAString& aReferrerUri,
                              txStylesheetCompiler* aCompiler)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("Compiler::loadURI forwards %s thru %s\n",
            NS_LossyConvertUTF16toASCII(aUri).get(),
            NS_LossyConvertUTF16toASCII(mStylesheetURI).get()));
    if (mStylesheetURI.Equals(aUri)) {
        return NS_ERROR_XSLT_LOAD_RECURSION;
    }
    return mObserver ? mObserver->loadURI(aUri, aReferrerUri, aCompiler) :
                       NS_ERROR_FAILURE;
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
    if (!mDoneWithThisStylesheet || !mChildCompilerList.IsEmpty()) {
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
      mDOE(false),
      mSearchingForFallback(false),
      mObserver(aObserver),
      mEmbedStatus(eNoEmbed),
      mDoneWithThisStylesheet(false),
      mNextInstrPtr(nsnull),
      mToplevelIterator(nsnull)
{
    // Embedded stylesheets have another handler, which is set in
    // txStylesheetCompiler::init if the baseURI has a fragment identifier.
    mHandlerTable = gTxRootHandler;

}

nsresult
txStylesheetCompilerState::init(const nsAString& aStylesheetURI,
                                txStylesheet* aStylesheet,
                                txListIterator* aInsertPosition)
{
    NS_ASSERTION(!aStylesheet || aInsertPosition,
                 "must provide insertposition if loading subsheet");
    mStylesheetURI = aStylesheetURI;
    // Check for fragment identifier of an embedded stylesheet.
    PRInt32 fragment = aStylesheetURI.FindChar('#') + 1;
    if (fragment > 0) {
        PRInt32 fragmentLength = aStylesheetURI.Length() - fragment;
        if (fragmentLength > 0) {
            // This is really an embedded stylesheet, not just a
            // "url#". We may want to unescape the fragment.
            mTarget = Substring(aStylesheetURI, (PRUint32)fragment,
                                fragmentLength);
            mEmbedStatus = eNeedEmbed;
            mHandlerTable = gTxEmbedHandler;
        }
    }
    nsresult rv = NS_OK;
    if (aStylesheet) {
        mStylesheet = aStylesheet;
        mToplevelIterator = *aInsertPosition;
        mIsTopCompiler = false;
    }
    else {
        mStylesheet = new txStylesheet;
        NS_ENSURE_TRUE(mStylesheet, NS_ERROR_OUT_OF_MEMORY);
        
        rv = mStylesheet->init();
        NS_ENSURE_SUCCESS(rv, rv);
        
        mToplevelIterator =
            txListIterator(&mStylesheet->mRootFrame->mToplevelItems);
        mToplevelIterator.next(); // go to the end of the list
        mIsTopCompiler = true;
    }
   
    mElementContext = new txElementContext(aStylesheetURI);
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
    for (i = mInScopeVariables.Length() - 1; i >= 0; --i) {
        delete mInScopeVariables[i];
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
    mHandlerTable = static_cast<txHandlerTable*>(popPtr());
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
    mSorter = static_cast<txPushNewContext*>(popPtr());
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
    mChooseGotoList = static_cast<txList*>(popObject());
}

nsresult
txStylesheetCompilerState::pushObject(TxObject* aObject)
{
    return mObjectStack.push(aObject);
}

TxObject*
txStylesheetCompilerState::popObject()
{
    return static_cast<TxObject*>(mObjectStack.pop());
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
    NS_ASSERTION(mGotoTargetPointers.IsEmpty(),
                 "GotoTargets still exists, did you forget to add txReturn?");
    mNextInstrPtr = 0;
}

nsresult
txStylesheetCompilerState::addInstruction(nsAutoPtr<txInstruction> aInstruction)
{
    NS_PRECONDITION(mNextInstrPtr, "adding instruction outside container");

    txInstruction* newInstr = aInstruction;

    *mNextInstrPtr = aInstruction.forget();
    mNextInstrPtr = newInstr->mNext.StartAssignment();
    
    PRUint32 i, count = mGotoTargetPointers.Length();
    for (i = 0; i < count; ++i) {
        *mGotoTargetPointers[i] = newInstr;
    }
    mGotoTargetPointers.Clear();

    return NS_OK;
}

nsresult
txStylesheetCompilerState::loadIncludedStylesheet(const nsAString& aURI)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("CompilerState::loadIncludedStylesheet: %s\n",
            NS_LossyConvertUTF16toASCII(aURI).get()));
    if (mStylesheetURI.Equals(aURI)) {
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
    
    txACompileObserver* observer = static_cast<txStylesheetCompiler*>(this);

    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(aURI, mStylesheet, &mToplevelIterator,
                                 observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    // step forward before calling the observer in case of syncronous loading
    mToplevelIterator.next();

    if (mChildCompilerList.AppendElement(compiler) == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = mObserver->loadURI(aURI, mStylesheetURI, compiler);
    if (NS_FAILED(rv)) {
        mChildCompilerList.RemoveElement(compiler);
    }

    return rv;
}

nsresult
txStylesheetCompilerState::loadImportedStylesheet(const nsAString& aURI,
                                                  txStylesheet::ImportFrame* aFrame)
{
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("CompilerState::loadImportedStylesheet: %s\n",
            NS_LossyConvertUTF16toASCII(aURI).get()));
    if (mStylesheetURI.Equals(aURI)) {
        return NS_ERROR_XSLT_LOAD_RECURSION;
    }
    NS_ENSURE_TRUE(mObserver, NS_ERROR_NOT_IMPLEMENTED);

    txListIterator iter(&aFrame->mToplevelItems);
    iter.next(); // go to the end of the list

    txACompileObserver* observer = static_cast<txStylesheetCompiler*>(this);

    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(aURI, mStylesheet, &iter, observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    if (mChildCompilerList.AppendElement(compiler) == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = mObserver->loadURI(aURI, mStylesheetURI, compiler);
    if (NS_FAILED(rv)) {
        mChildCompilerList.RemoveElement(compiler);
    }

    return rv;  
}

nsresult
txStylesheetCompilerState::addGotoTarget(txInstruction** aTargetPointer)
{
    if (mGotoTargetPointers.AppendElement(aTargetPointer) == nsnull) {
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
    NS_ASSERTION(aPrefix && aPrefix != nsGkAtoms::_empty,
                 "caller should handle default namespace ''");
    aID = mElementContext->mMappings->lookupNamespace(aPrefix);
    return (aID != kNameSpaceID_Unknown) ? NS_OK : NS_ERROR_FAILURE;
}

/**
 * Error Function to be used for unknown extension functions.
 *
 */
class txErrorFunctionCall : public FunctionCall
{
public:
    txErrorFunctionCall(nsIAtom* aName, const PRInt32 aID)
        : mName(aName),
          mID(aID)
    {
    }

    TX_DECL_FUNCTION

private:
    nsCOMPtr<nsIAtom> mName;
    PRInt32 mID;
};

nsresult
txErrorFunctionCall::evaluate(txIEvalContext* aContext,
                              txAExprResult** aResult)
{
    *aResult = nsnull;

    return NS_ERROR_XPATH_BAD_EXTENSION_FUNCTION;
}

Expr::ResultType
txErrorFunctionCall::getReturnType()
{
    // It doesn't really matter what we return here, but it might
    // be a good idea to try to keep this as unoptimizable as possible
    return ANY_RESULT;
}

bool
txErrorFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    // It doesn't really matter what we return here, but it might
    // be a good idea to try to keep this as unoptimizable as possible
    return true;
}

#ifdef TX_TO_STRING
nsresult
txErrorFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    NS_IF_ADDREF(*aAtom = mName);

    return NS_OK;
}
#endif

static nsresult
TX_ConstructXSLTFunction(nsIAtom* aName, PRInt32 aNamespaceID,
                         txStylesheetCompilerState* aState,
                         FunctionCall** aFunction)
{
    if (aName == nsGkAtoms::document) {
        *aFunction =
            new DocumentFunctionCall(aState->mElementContext->mBaseURI);
    }
    else if (aName == nsGkAtoms::key) {
        *aFunction =
            new txKeyFunctionCall(aState->mElementContext->mMappings);
    }
    else if (aName == nsGkAtoms::formatNumber) {
        *aFunction =
            new txFormatNumberFunctionCall(aState->mStylesheet,
                                           aState->mElementContext->mMappings);
    }
    else if (aName == nsGkAtoms::current) {
        *aFunction = new CurrentFunctionCall();
    }
    else if (aName == nsGkAtoms::unparsedEntityUri) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    else if (aName == nsGkAtoms::generateId) {
        *aFunction = new GenerateIdFunctionCall();
    }
    else if (aName == nsGkAtoms::systemProperty) {
        *aFunction = new txXSLTEnvironmentFunctionCall(
            txXSLTEnvironmentFunctionCall::SYSTEM_PROPERTY,
            aState->mElementContext->mMappings);
    }
    else if (aName == nsGkAtoms::elementAvailable) {
        *aFunction = new txXSLTEnvironmentFunctionCall(
            txXSLTEnvironmentFunctionCall::ELEMENT_AVAILABLE,
            aState->mElementContext->mMappings);
    }
    else if (aName == nsGkAtoms::functionAvailable) {
        *aFunction = new txXSLTEnvironmentFunctionCall(
            txXSLTEnvironmentFunctionCall::FUNCTION_AVAILABLE,
            aState->mElementContext->mMappings);
    }
    else {
        return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
    }

    return *aFunction ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

typedef nsresult (*txFunctionFactory)(nsIAtom* aName,
                                      PRInt32 aNamespaceID,
                                      txStylesheetCompilerState* aState,
                                      FunctionCall** aResult);
struct txFunctionFactoryMapping
{
    const char* const mNamespaceURI;
    PRInt32 mNamespaceID;
    txFunctionFactory mFactory;
};

extern nsresult
TX_ConstructEXSLTFunction(nsIAtom *aName,
                          PRInt32 aNamespaceID,
                          txStylesheetCompilerState* aState,
                          FunctionCall **aResult);

static txFunctionFactoryMapping kExtensionFunctions[] = {
    { "", kNameSpaceID_Unknown, TX_ConstructXSLTFunction },
    { "http://exslt.org/common", kNameSpaceID_Unknown,
      TX_ConstructEXSLTFunction },
    { "http://exslt.org/sets", kNameSpaceID_Unknown,
      TX_ConstructEXSLTFunction },
    { "http://exslt.org/strings", kNameSpaceID_Unknown,
      TX_ConstructEXSLTFunction },
    { "http://exslt.org/math", kNameSpaceID_Unknown,
      TX_ConstructEXSLTFunction },
    { "http://exslt.org/dates-and-times", kNameSpaceID_Unknown,
      TX_ConstructEXSLTFunction }
};

extern nsresult
TX_ResolveFunctionCallXPCOM(const nsCString &aContractID, PRInt32 aNamespaceID,
                            nsIAtom *aName, nsISupports *aState,
                            FunctionCall **aFunction);

struct txXPCOMFunctionMapping
{
    PRInt32 mNamespaceID;
    nsCString mContractID;
};

static nsTArray<txXPCOMFunctionMapping> *sXPCOMFunctionMappings = nsnull;

static nsresult
findFunction(nsIAtom* aName, PRInt32 aNamespaceID,
             txStylesheetCompilerState* aState, FunctionCall** aResult)
{
    if (kExtensionFunctions[0].mNamespaceID == kNameSpaceID_Unknown) {
        PRUint32 i;
        for (i = 0; i < ArrayLength(kExtensionFunctions); ++i) {
            txFunctionFactoryMapping& mapping = kExtensionFunctions[i];
            NS_ConvertASCIItoUTF16 namespaceURI(mapping.mNamespaceURI);
            mapping.mNamespaceID =
                txNamespaceManager::getNamespaceID(namespaceURI);
        }
    }

    PRUint32 i;
    for (i = 0; i < ArrayLength(kExtensionFunctions); ++i) {
        const txFunctionFactoryMapping& mapping = kExtensionFunctions[i];
        if (mapping.mNamespaceID == aNamespaceID) {
            return mapping.mFactory(aName, aNamespaceID, aState, aResult);
        }
    }

    if (!sXPCOMFunctionMappings) {
        sXPCOMFunctionMappings = new nsTArray<txXPCOMFunctionMapping>;
        if (!sXPCOMFunctionMappings) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    txXPCOMFunctionMapping *map = nsnull;
    PRUint32 count = sXPCOMFunctionMappings->Length();
    for (i = 0; i < count; ++i) {
        map = &sXPCOMFunctionMappings->ElementAt(i);
        if (map->mNamespaceID == aNamespaceID) {
            break;
        }
    }

    if (i == count) {
        nsresult rv;
        nsCOMPtr<nsICategoryManager> catman =
            do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString namespaceURI;
        rv = txNamespaceManager::getNamespaceURI(aNamespaceID, namespaceURI);
        NS_ENSURE_SUCCESS(rv, rv);

        nsXPIDLCString contractID;
        rv = catman->GetCategoryEntry("XSLT-extension-functions",
                                      NS_ConvertUTF16toUTF8(namespaceURI).get(),
                                      getter_Copies(contractID));
        if (rv == NS_ERROR_NOT_AVAILABLE) {
            return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
        }
        NS_ENSURE_SUCCESS(rv, rv);

        map = sXPCOMFunctionMappings->AppendElement();
        if (!map) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        map->mNamespaceID = aNamespaceID;
        map->mContractID = contractID;
    }

    return TX_ResolveFunctionCallXPCOM(map->mContractID, aNamespaceID, aName,
                                       nsnull, aResult);
}

extern bool
TX_XSLTFunctionAvailable(nsIAtom* aName, PRInt32 aNameSpaceID)
{
    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(EmptyString(), nsnull);
    NS_ENSURE_TRUE(compiler, false);

    nsAutoPtr<FunctionCall> fnCall;

    return NS_SUCCEEDED(findFunction(aName, aNameSpaceID, compiler,
                                     getter_Transfers(fnCall)));
}

nsresult
txStylesheetCompilerState::resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                               FunctionCall **aFunction)
{
    *aFunction = nsnull;

    nsresult rv = findFunction(aName, aID, this, aFunction);
    if (rv == NS_ERROR_XPATH_UNKNOWN_FUNCTION &&
        (aID != kNameSpaceID_None || fcp())) {
        *aFunction = new txErrorFunctionCall(aName, aID);
        rv = *aFunction ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}

bool
txStylesheetCompilerState::caseInsensitiveNameTests()
{
    return false;
}

void
txStylesheetCompilerState::SetErrorOffset(PRUint32 aOffset)
{
    // XXX implement me
}

/* static */
void
txStylesheetCompilerState::shutdown()
{
    delete sXPCOMFunctionMappings;
    sXPCOMFunctionMappings = nsnull;
}

txElementContext::txElementContext(const nsAString& aBaseURI)
    : mPreserveWhitespace(false),
      mForwardsCompatibleParsing(true),
      mBaseURI(aBaseURI),
      mMappings(new txNamespaceMap),
      mDepth(0)
{
    mInstructionNamespaces.AppendElement(kNameSpaceID_XSLT);
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
