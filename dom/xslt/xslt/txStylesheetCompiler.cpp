/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"

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
using mozilla::net::ReferrerPolicy;

txStylesheetCompiler::txStylesheetCompiler(const nsAString& aStylesheetURI,
                                           ReferrerPolicy aReferrerPolicy,
                                           txACompileObserver* aObserver)
    : txStylesheetCompilerState(aObserver)
{
    mStatus = init(aStylesheetURI, aReferrerPolicy, nullptr, nullptr);
}

txStylesheetCompiler::txStylesheetCompiler(const nsAString& aStylesheetURI,
                                           txStylesheet* aStylesheet,
                                           txListIterator* aInsertPosition,
                                           ReferrerPolicy aReferrerPolicy,
                                           txACompileObserver* aObserver)
    : txStylesheetCompilerState(aObserver)
{
    mStatus = init(aStylesheetURI, aReferrerPolicy, aStylesheet,
                   aInsertPosition);
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
txStylesheetCompiler::startElement(int32_t aNamespaceID, nsAtom* aLocalName,
                                   nsAtom* aPrefix,
                                   txStylesheetAttr* aAttributes,
                                   int32_t aAttrCount)
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
    int32_t i;
    for (i = 0; i < aAttrCount; ++i) {
        txStylesheetAttr* attr = aAttributes + i;
        if (attr->mNamespaceID == kNameSpaceID_XMLNS) {
            rv = ensureNewElementContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (!hasOwnNamespaceMap) {
                mElementContext->mMappings =
                    new txNamespaceMap(*mElementContext->mMappings);
                hasOwnNamespaceMap = true;
            }

            if (attr->mLocalName == nsGkAtoms::xmlns) {
                mElementContext->mMappings->mapNamespace(nullptr, attr->mValue);
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
txStylesheetCompiler::startElement(const char16_t *aName,
                                   const char16_t **aAttrs,
                                   int32_t aAttrCount)
{
    if (NS_FAILED(mStatus)) {
        // ignore content after failure
        // XXX reevaluate once expat stops on failure
        return NS_OK;
    }

    nsresult rv = flushCharacters();
    NS_ENSURE_SUCCESS(rv, rv);

    UniquePtr<txStylesheetAttr[]> atts;
    if (aAttrCount > 0) {
        atts = MakeUnique<txStylesheetAttr[]>(aAttrCount);
    }

    bool hasOwnNamespaceMap = false;
    int32_t i;
    for (i = 0; i < aAttrCount; ++i) {
        rv = XMLUtils::splitExpatName(aAttrs[i * 2],
                                      getter_AddRefs(atts[i].mPrefix),
                                      getter_AddRefs(atts[i].mLocalName),
                                      &atts[i].mNamespaceID);
        NS_ENSURE_SUCCESS(rv, rv);
        atts[i].mValue.Append(aAttrs[i * 2 + 1]);

        RefPtr<nsAtom> prefixToBind;
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
                hasOwnNamespaceMap = true;
            }

            rv = mElementContext->mMappings->
                mapNamespace(prefixToBind, atts[i].mValue);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    RefPtr<nsAtom> prefix, localname;
    int32_t namespaceID;
    rv = XMLUtils::splitExpatName(aName, getter_AddRefs(prefix),
                                  getter_AddRefs(localname), &namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    return startElementInternal(namespaceID, localname, prefix, atts.get(),
                                aAttrCount);
}

nsresult
txStylesheetCompiler::startElementInternal(int32_t aNamespaceID,
                                           nsAtom* aLocalName,
                                           nsAtom* aPrefix,
                                           txStylesheetAttr* aAttributes,
                                           int32_t aAttrCount)
{
    nsresult rv = NS_OK;
    int32_t i;
    for (i = mInScopeVariables.Length() - 1; i >= 0; --i) {
        ++mInScopeVariables[i]->mLevel;
    }

    // Update the elementcontext if we have special attributes
    for (i = 0; i < aAttrCount; ++i) {
        txStylesheetAttr* attr = aAttributes + i;

        // id
        if (mEmbedStatus == eNeedEmbed &&
            attr->mLocalName == nsGkAtoms::id &&
            attr->mNamespaceID == kNameSpaceID_None &&
            attr->mValue.Equals(mTarget)) {
            // We found the right ID, signal to compile the
            // embedded stylesheet.
            mEmbedStatus = eInEmbed;
        }

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
                int32_t namespaceID = mElementContext->mMappings->
                    lookupNamespaceWithDefault(tok.nextToken());

                if (namespaceID == kNameSpaceID_Unknown)
                    return NS_ERROR_XSLT_PARSE_FAILURE;

                if (!mElementContext->mInstructionNamespaces.
                        AppendElement(namespaceID)) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }

            attr->mLocalName = nullptr;
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
    int32_t count = mElementContext->mInstructionNamespaces.Length();
    for (i = 0; i < count; ++i) {
        if (mElementContext->mInstructionNamespaces[i] == aNamespaceID) {
            isInstruction = true;
            break;
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

    rv = pushPtr(const_cast<txElementHandler*>(handler), eElementHandler);
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

    int32_t i;
    for (i = mInScopeVariables.Length() - 1; i >= 0; --i) {
        txInScopeVariable* var = mInScopeVariables[i];
        if (!--(var->mLevel)) {
            nsAutoPtr<txInstruction> instr(new txRemoveVariable(var->mName));
            rv = addInstruction(std::move(instr));
            NS_ENSURE_SUCCESS(rv, rv);

            mInScopeVariables.RemoveElementAt(i);
            delete var;
        }
    }

    const txElementHandler* handler =
        const_cast<const txElementHandler*>
                  (static_cast<txElementHandler*>(popPtr(eElementHandler)));
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
    MOZ_LOG(txLog::xslt, LogLevel::Info,
           ("Compiler::doneLoading: %s\n",
            NS_LossyConvertUTF16toASCII(mStylesheetURI).get()));
    if (NS_FAILED(mStatus)) {
        return mStatus;
    }

    mDoneWithThisStylesheet = true;

    return maybeDoneCompiling();
}

void
txStylesheetCompiler::cancel(nsresult aError, const char16_t *aErrorText,
                             const char16_t *aParam)
{
    MOZ_LOG(txLog::xslt, LogLevel::Info,
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
        mObserver = nullptr;
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
                              ReferrerPolicy aReferrerPolicy,
                              txStylesheetCompiler* aCompiler)
{
    MOZ_LOG(txLog::xslt, LogLevel::Info,
           ("Compiler::loadURI forwards %s thru %s\n",
            NS_LossyConvertUTF16toASCII(aUri).get(),
            NS_LossyConvertUTF16toASCII(mStylesheetURI).get()));
    if (mStylesheetURI.Equals(aUri)) {
        return NS_ERROR_XSLT_LOAD_RECURSION;
    }
    return mObserver ?
        mObserver->loadURI(aUri, aReferrerUri, aReferrerPolicy, aCompiler) :
        NS_ERROR_FAILURE;
}

void
txStylesheetCompiler::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                      nsresult aResult,
                                      const char16_t *aErrorText,
                                      const char16_t *aParam)
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
    nsresult rv = pushObject(mElementContext);
    NS_ENSURE_SUCCESS(rv, rv);

    mElementContext.forget();
    mElementContext = std::move(context);

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
        mObserver = nullptr;
    }

    return NS_OK;
}

/**
 * txStylesheetCompilerState
 */


txStylesheetCompilerState::txStylesheetCompilerState(txACompileObserver* aObserver)
    : mHandlerTable(nullptr),
      mSorter(nullptr),
      mDOE(false),
      mSearchingForFallback(false),
      mDisAllowed(0),
      mObserver(aObserver),
      mEmbedStatus(eNoEmbed),
      mIsTopCompiler(false),
      mDoneWithThisStylesheet(false),
      mNextInstrPtr(nullptr),
      mToplevelIterator(nullptr),
      mReferrerPolicy(mozilla::net::RP_Unset)
{
    // Embedded stylesheets have another handler, which is set in
    // txStylesheetCompiler::init if the baseURI has a fragment identifier.
    mHandlerTable = gTxRootHandler;

}

nsresult
txStylesheetCompilerState::init(const nsAString& aStylesheetURI,
                                ReferrerPolicy aReferrerPolicy,
                                txStylesheet* aStylesheet,
                                txListIterator* aInsertPosition)
{
    NS_ASSERTION(!aStylesheet || aInsertPosition,
                 "must provide insertposition if loading subsheet");
    mStylesheetURI = aStylesheetURI;
    mReferrerPolicy = aReferrerPolicy;
    // Check for fragment identifier of an embedded stylesheet.
    int32_t fragment = aStylesheetURI.FindChar('#') + 1;
    if (fragment > 0) {
        int32_t fragmentLength = aStylesheetURI.Length() - fragment;
        if (fragmentLength > 0) {
            // This is really an embedded stylesheet, not just a
            // "url#". We may want to unescape the fragment.
            mTarget = Substring(aStylesheetURI, (uint32_t)fragment,
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
        rv = mStylesheet->init();
        NS_ENSURE_SUCCESS(rv, rv);

        mToplevelIterator =
            txListIterator(&mStylesheet->mRootFrame->mToplevelItems);
        mToplevelIterator.next(); // go to the end of the list
        mIsTopCompiler = true;
    }

    mElementContext = new txElementContext(aStylesheetURI);
    NS_ENSURE_TRUE(mElementContext->mMappings, NS_ERROR_OUT_OF_MEMORY);

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

    int32_t i;
    for (i = mInScopeVariables.Length() - 1; i >= 0; --i) {
        delete mInScopeVariables[i];
    }
}

nsresult
txStylesheetCompilerState::pushHandlerTable(txHandlerTable* aTable)
{
    nsresult rv = pushPtr(mHandlerTable, eHandlerTable);
    NS_ENSURE_SUCCESS(rv, rv);

    mHandlerTable = aTable;

    return NS_OK;
}

void
txStylesheetCompilerState::popHandlerTable()
{
    mHandlerTable = static_cast<txHandlerTable*>(popPtr(eHandlerTable));
}

nsresult
txStylesheetCompilerState::pushSorter(txPushNewContext* aSorter)
{
    nsresult rv = pushPtr(mSorter, ePushNewContext);
    NS_ENSURE_SUCCESS(rv, rv);

    mSorter = aSorter;

    return NS_OK;
}

void
txStylesheetCompilerState::popSorter()
{
    mSorter = static_cast<txPushNewContext*>(popPtr(ePushNewContext));
}

nsresult
txStylesheetCompilerState::pushChooseGotoList()
{
    nsresult rv = pushObject(mChooseGotoList);
    NS_ENSURE_SUCCESS(rv, rv);

    mChooseGotoList.forget();
    mChooseGotoList = new txList;

    return NS_OK;
}

void
txStylesheetCompilerState::popChooseGotoList()
{
    // this will delete the old value
    mChooseGotoList = static_cast<txList*>(popObject());
}

nsresult
txStylesheetCompilerState::pushObject(txObject* aObject)
{
    return mObjectStack.push(aObject);
}

txObject*
txStylesheetCompilerState::popObject()
{
    return static_cast<txObject*>(mObjectStack.pop());
}

nsresult
txStylesheetCompilerState::pushPtr(void* aPtr, enumStackType aType)
{
#ifdef TX_DEBUG_STACK
    MOZ_LOG(txLog::xslt, LogLevel::Debug, ("pushPtr: 0x%x type %u\n", aPtr, aType));
#endif
    mTypeStack.AppendElement(aType);
    return mOtherStack.push(aPtr);
}

void*
txStylesheetCompilerState::popPtr(enumStackType aType)
{
    uint32_t stacklen = mTypeStack.Length();
    if (stacklen == 0) {
        MOZ_CRASH("Attempt to pop when type stack is empty");
    }

    enumStackType type = mTypeStack.ElementAt(stacklen - 1);
    mTypeStack.RemoveElementAt(stacklen - 1);
    void* value = mOtherStack.pop();

#ifdef TX_DEBUG_STACK
    MOZ_LOG(txLog::xslt, LogLevel::Debug, ("popPtr: 0x%x type %u requested %u\n", value, type, aType));
#endif

    if (type != aType) {
        MOZ_CRASH("Expected type does not match top element type");
    }

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
    MOZ_ASSERT(!mNextInstrPtr, "can't nest instruction-containers");

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
txStylesheetCompilerState::addInstruction(nsAutoPtr<txInstruction>&& aInstruction)
{
    MOZ_ASSERT(mNextInstrPtr, "adding instruction outside container");

    txInstruction* newInstr = aInstruction;

    *mNextInstrPtr = aInstruction.forget();
    mNextInstrPtr = newInstr->mNext.StartAssignment();

    uint32_t i, count = mGotoTargetPointers.Length();
    for (i = 0; i < count; ++i) {
        *mGotoTargetPointers[i] = newInstr;
    }
    mGotoTargetPointers.Clear();

    return NS_OK;
}

nsresult
txStylesheetCompilerState::loadIncludedStylesheet(const nsAString& aURI)
{
    MOZ_LOG(txLog::xslt, LogLevel::Info,
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

    RefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(aURI, mStylesheet, &mToplevelIterator,
                                 mReferrerPolicy, observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    // step forward before calling the observer in case of syncronous loading
    mToplevelIterator.next();

    if (mChildCompilerList.AppendElement(compiler) == nullptr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = mObserver->loadURI(aURI, mStylesheetURI, mReferrerPolicy, compiler);
    if (NS_FAILED(rv)) {
        mChildCompilerList.RemoveElement(compiler);
    }

    return rv;
}

nsresult
txStylesheetCompilerState::loadImportedStylesheet(const nsAString& aURI,
                                                  txStylesheet::ImportFrame* aFrame)
{
    MOZ_LOG(txLog::xslt, LogLevel::Info,
           ("CompilerState::loadImportedStylesheet: %s\n",
            NS_LossyConvertUTF16toASCII(aURI).get()));
    if (mStylesheetURI.Equals(aURI)) {
        return NS_ERROR_XSLT_LOAD_RECURSION;
    }
    NS_ENSURE_TRUE(mObserver, NS_ERROR_NOT_IMPLEMENTED);

    txListIterator iter(&aFrame->mToplevelItems);
    iter.next(); // go to the end of the list

    txACompileObserver* observer = static_cast<txStylesheetCompiler*>(this);

    RefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(aURI, mStylesheet, &iter, mReferrerPolicy,
                                 observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    if (mChildCompilerList.AppendElement(compiler) == nullptr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = mObserver->loadURI(aURI, mStylesheetURI, mReferrerPolicy,
                                     compiler);
    if (NS_FAILED(rv)) {
        mChildCompilerList.RemoveElement(compiler);
    }

    return rv;
}

nsresult
txStylesheetCompilerState::addGotoTarget(txInstruction** aTargetPointer)
{
    if (mGotoTargetPointers.AppendElement(aTargetPointer) == nullptr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

nsresult
txStylesheetCompilerState::addVariable(const txExpandedName& aName)
{
    txInScopeVariable* var = new txInScopeVariable(aName);
    if (!mInScopeVariables.AppendElement(var)) {
        delete var;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

nsresult
txStylesheetCompilerState::resolveNamespacePrefix(nsAtom* aPrefix,
                                                  int32_t& aID)
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
    explicit txErrorFunctionCall(nsAtom* aName)
      : mName(aName)
    {
    }

    TX_DECL_FUNCTION

private:
    RefPtr<nsAtom> mName;
};

nsresult
txErrorFunctionCall::evaluate(txIEvalContext* aContext,
                              txAExprResult** aResult)
{
    *aResult = nullptr;

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
void
txErrorFunctionCall::appendName(nsAString& aDest)
{
    aDest.Append(mName->GetUTF16String());
}
#endif

static nsresult
TX_ConstructXSLTFunction(nsAtom* aName, txStylesheetCompilerState* aState,
                         FunctionCall** aFunction)
{
    if (aName == nsGkAtoms::document) {
        *aFunction =
            new DocumentFunctionCall(aState->mElementContext->mBaseURI);
    }
    else if (aName == nsGkAtoms::key) {
        if (!aState->allowed(txIParseContext::KEY_FUNCTION)) {
            return NS_ERROR_XSLT_CALL_TO_KEY_NOT_ALLOWED;
        }
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

    MOZ_ASSERT(*aFunction);
    return NS_OK;
}

extern nsresult
TX_ConstructEXSLTFunction(nsAtom *aName,
                          int32_t aNamespaceID,
                          txStylesheetCompilerState* aState,
                          FunctionCall **aResult);

static nsresult
findFunction(nsAtom* aName, int32_t aNamespaceID,
             txStylesheetCompilerState* aState, FunctionCall** aResult)
{
    if (aNamespaceID == kNameSpaceID_None) {
      return TX_ConstructXSLTFunction(aName, aState, aResult);
    }

    return TX_ConstructEXSLTFunction(aName, aNamespaceID, aState, aResult);
}

extern bool
TX_XSLTFunctionAvailable(nsAtom* aName, int32_t aNameSpaceID)
{
    RefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(EmptyString(),
                                 mozilla::net::RP_Unset, nullptr);
    NS_ENSURE_TRUE(compiler, false);

    nsAutoPtr<FunctionCall> fnCall;

    return NS_SUCCEEDED(findFunction(aName, aNameSpaceID, compiler,
                                     getter_Transfers(fnCall)));
}

nsresult
txStylesheetCompilerState::resolveFunctionCall(nsAtom* aName, int32_t aID,
                                               FunctionCall **aFunction)
{
    *aFunction = nullptr;

    nsresult rv = findFunction(aName, aID, this, aFunction);
    if (rv == NS_ERROR_XPATH_UNKNOWN_FUNCTION &&
        (aID != kNameSpaceID_None || fcp())) {
        *aFunction = new txErrorFunctionCall(aName);
        rv = NS_OK;
    }

    return rv;
}

bool
txStylesheetCompilerState::caseInsensitiveNameTests()
{
    return false;
}

void
txStylesheetCompilerState::SetErrorOffset(uint32_t aOffset)
{
    // XXX implement me
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
