/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txMozillaXSLTProcessor.h"
#include "nsContentCID.h"
#include "nsError.h"
#include "nsIChannel.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDocument.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNodeList.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsXPathResult.h"
#include "txExecutionState.h"
#include "txMozillaTextOutput.h"
#include "txMozillaXMLOutput.h"
#include "txURIUtils.h"
#include "txXMLUtils.h"
#include "txUnknownHandler.h"
#include "txXSLTProcessor.h"
#include "nsIPrincipal.h"
#include "nsThreadUtils.h"
#include "jsapi.h"
#include "txExprParser.h"
#include "nsIErrorService.h"
#include "nsIScriptSecurityManager.h"
#include "nsJSUtils.h"

using namespace mozilla::dom;

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

/**
 * Output Handler Factories
 */
class txToDocHandlerFactory : public txAOutputHandlerFactory
{
public:
    txToDocHandlerFactory(txExecutionState* aEs,
                          nsIDOMDocument* aSourceDocument,
                          nsITransformObserver* aObserver,
                          bool aDocumentIsData)
        : mEs(aEs), mSourceDocument(aSourceDocument), mObserver(aObserver),
          mDocumentIsData(aDocumentIsData)
    {
    }

    TX_DECL_TXAOUTPUTHANDLERFACTORY

private:
    txExecutionState* mEs;
    nsCOMPtr<nsIDOMDocument> mSourceDocument;
    nsCOMPtr<nsITransformObserver> mObserver;
    bool mDocumentIsData;
};

class txToFragmentHandlerFactory : public txAOutputHandlerFactory
{
public:
    txToFragmentHandlerFactory(nsIDOMDocumentFragment* aFragment)
        : mFragment(aFragment)
    {
    }

    TX_DECL_TXAOUTPUTHANDLERFACTORY

private:
    nsCOMPtr<nsIDOMDocumentFragment> mFragment;
};

nsresult
txToDocHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                         txAXMLEventHandler** aHandler)
{
    *aHandler = nullptr;
    switch (aFormat->mMethod) {
        case eMethodNotSet:
        case eXMLOutput:
        {
            *aHandler = new txUnknownHandler(mEs);
            return NS_OK;
        }

        case eHTMLOutput:
        {
            nsAutoPtr<txMozillaXMLOutput> handler(
                new txMozillaXMLOutput(aFormat, mObserver));

            nsresult rv = handler->createResultDocument(EmptyString(),
                                                        kNameSpaceID_None,
                                                        mSourceDocument,
                                                        mDocumentIsData);
            if (NS_SUCCEEDED(rv)) {
                *aHandler = handler.forget();
            }

            return rv;
        }

        case eTextOutput:
        {
            nsAutoPtr<txMozillaTextOutput> handler(
                new txMozillaTextOutput(mObserver));

            nsresult rv = handler->createResultDocument(mSourceDocument,
                                                        mDocumentIsData);
            if (NS_SUCCEEDED(rv)) {
                *aHandler = handler.forget();
            }

            return rv;
        }
    }

    NS_RUNTIMEABORT("Unknown output method");

    return NS_ERROR_FAILURE;
}

nsresult
txToDocHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                         const nsSubstring& aName,
                                         int32_t aNsID,
                                         txAXMLEventHandler** aHandler)
{
    *aHandler = nullptr;
    switch (aFormat->mMethod) {
        case eMethodNotSet:
        {
            NS_ERROR("How can method not be known when root element is?");
            return NS_ERROR_UNEXPECTED;
        }

        case eXMLOutput:
        case eHTMLOutput:
        {
            nsAutoPtr<txMozillaXMLOutput> handler(
                new txMozillaXMLOutput(aFormat, mObserver));

            nsresult rv = handler->createResultDocument(aName, aNsID,
                                                        mSourceDocument,
                                                        mDocumentIsData);
            if (NS_SUCCEEDED(rv)) {
                *aHandler = handler.forget();
            }

            return rv;
        }

        case eTextOutput:
        {
            nsAutoPtr<txMozillaTextOutput> handler(
                new txMozillaTextOutput(mObserver));

            nsresult rv = handler->createResultDocument(mSourceDocument,
                                                        mDocumentIsData);
            if (NS_SUCCEEDED(rv)) {
                *aHandler = handler.forget();
            }

            return rv;
        }
    }

    NS_RUNTIMEABORT("Unknown output method");

    return NS_ERROR_FAILURE;
}

nsresult
txToFragmentHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              txAXMLEventHandler** aHandler)
{
    *aHandler = nullptr;
    switch (aFormat->mMethod) {
        case eMethodNotSet:
        {
            txOutputFormat format;
            format.merge(*aFormat);
            nsCOMPtr<nsIDOMDocument> domdoc;
            mFragment->GetOwnerDocument(getter_AddRefs(domdoc));
            NS_ASSERTION(domdoc, "unable to get ownerdocument");
            nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

            if (doc && doc->IsHTML()) {
                format.mMethod = eHTMLOutput;
            } else {
                format.mMethod = eXMLOutput;
            }

            *aHandler = new txMozillaXMLOutput(&format, mFragment, false);
            break;
        }

        case eXMLOutput:
        case eHTMLOutput:
        {
            *aHandler = new txMozillaXMLOutput(aFormat, mFragment, false);
            break;
        }

        case eTextOutput:
        {
            *aHandler = new txMozillaTextOutput(mFragment);
            break;
        }
    }
    NS_ENSURE_TRUE(*aHandler, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
}

nsresult
txToFragmentHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              const nsSubstring& aName,
                                              int32_t aNsID,
                                              txAXMLEventHandler** aHandler)
{
    *aHandler = nullptr;
    NS_ASSERTION(aFormat->mMethod != eMethodNotSet,
                 "How can method not be known when root element is?");
    NS_ENSURE_TRUE(aFormat->mMethod != eMethodNotSet, NS_ERROR_UNEXPECTED);
    return createHandlerWith(aFormat, aHandler);
}

class txVariable : public txIGlobalParameter
{
public:
    txVariable(nsIVariant *aValue) : mValue(aValue)
    {
        NS_ASSERTION(aValue, "missing value");
    }
    txVariable(txAExprResult* aValue) : mTxValue(aValue)
    {
        NS_ASSERTION(aValue, "missing value");
    }
    nsresult getValue(txAExprResult** aValue)
    {
        NS_ASSERTION(mValue || mTxValue, "variablevalue is null");

        if (!mTxValue) {
            nsresult rv = Convert(mValue, getter_AddRefs(mTxValue));
            NS_ENSURE_SUCCESS(rv, rv);
        }

        *aValue = mTxValue;
        NS_ADDREF(*aValue);

        return NS_OK;
    }
    nsresult getValue(nsIVariant** aValue)
    {
        *aValue = mValue;
        NS_ADDREF(*aValue);
        return NS_OK;
    }
    nsIVariant* getValue()
    {
        return mValue;
    }
    void setValue(nsIVariant* aValue)
    {
        NS_ASSERTION(aValue, "setting variablevalue to null");
        mValue = aValue;
        mTxValue = nullptr;
    }
    void setValue(txAExprResult* aValue)
    {
        NS_ASSERTION(aValue, "setting variablevalue to null");
        mValue = nullptr;
        mTxValue = aValue;
    }

private:
    static nsresult Convert(nsIVariant *aValue, txAExprResult** aResult);

    nsCOMPtr<nsIVariant> mValue;
    nsRefPtr<txAExprResult> mTxValue;
};

/**
 * txMozillaXSLTProcessor
 */

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(txMozillaXSLTProcessor)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mEmbeddedStylesheetRoot)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
    tmp->mVariables.clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(txMozillaXSLTProcessor)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEmbeddedStylesheetRoot)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
    txOwningExpandedNameMap<txIGlobalParameter>::iterator iter(tmp->mVariables);
    while (iter.next()) {
        cb.NoteXPCOMChild(static_cast<txVariable*>(iter.value())->getValue());
    }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(txMozillaXSLTProcessor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(txMozillaXSLTProcessor)

DOMCI_DATA(XSLTProcessor, txMozillaXSLTProcessor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(txMozillaXSLTProcessor)
    NS_INTERFACE_MAP_ENTRY(nsIXSLTProcessor)
    NS_INTERFACE_MAP_ENTRY(nsIXSLTProcessorPrivate)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentTransformer)
    NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
    NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXSLTProcessor)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XSLTProcessor)
NS_INTERFACE_MAP_END

txMozillaXSLTProcessor::txMozillaXSLTProcessor() : mStylesheetDocument(nullptr),
                                                   mTransformResult(NS_OK),
                                                   mCompileResult(NS_OK),
                                                   mFlags(0)
{
}

txMozillaXSLTProcessor::~txMozillaXSLTProcessor()
{
    if (mStylesheetDocument) {
        mStylesheetDocument->RemoveMutationObserver(this);
    }
}

NS_IMETHODIMP
txMozillaXSLTProcessor::SetTransformObserver(nsITransformObserver* aObserver)
{
    mObserver = aObserver;
    return NS_OK;
}

nsresult
txMozillaXSLTProcessor::SetSourceContentModel(nsIDOMNode* aSourceDOM)
{
    mSource = aSourceDOM;

    if (NS_FAILED(mTransformResult)) {
        notifyError();
        return NS_OK;
    }

    if (mStylesheet) {
        return DoTransform();
    }

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::AddXSLTParamNamespace(const nsString& aPrefix,
                                              const nsString& aNamespace)
{
    nsCOMPtr<nsIAtom> pre = do_GetAtom(aPrefix);
    return mParamNamespaceMap.mapNamespace(pre, aNamespace);
}


class txXSLTParamContext : public txIParseContext,
                           public txIEvalContext
{
public:
    txXSLTParamContext(txNamespaceMap *aResolver, const txXPathNode& aContext,
                       txResultRecycler* aRecycler)
        : mResolver(aResolver),
          mContext(aContext),
          mRecycler(aRecycler)
    {
    }

    // txIParseContext
    nsresult resolveNamespacePrefix(nsIAtom* aPrefix, int32_t& aID)
    {
        aID = mResolver->lookupNamespace(aPrefix);
        return aID == kNameSpaceID_Unknown ? NS_ERROR_DOM_NAMESPACE_ERR :
                                             NS_OK;
    }
    nsresult resolveFunctionCall(nsIAtom* aName, int32_t aID,
                                 FunctionCall** aFunction)
    {
        return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
    }
    bool caseInsensitiveNameTests()
    {
        return false;
    }
    void SetErrorOffset(uint32_t aOffset)
    {
    }

    // txIEvalContext
    nsresult getVariable(int32_t aNamespace, nsIAtom* aLName,
                         txAExprResult*& aResult)
    {
        aResult = nullptr;
        return NS_ERROR_INVALID_ARG;
    }
    bool isStripSpaceAllowed(const txXPathNode& aNode)
    {
        return false;
    }
    void* getPrivateContext()
    {
        return nullptr;
    }
    txResultRecycler* recycler()
    {
        return mRecycler;
    }
    void receiveError(const nsAString& aMsg, nsresult aRes)
    {
    }
    const txXPathNode& getContextNode()
    {
      return mContext;
    }
    uint32_t size()
    {
      return 1;
    }
    uint32_t position()
    {
      return 1;
    }

private:
    txNamespaceMap *mResolver;
    const txXPathNode& mContext;
    txResultRecycler* mRecycler;
};


NS_IMETHODIMP
txMozillaXSLTProcessor::AddXSLTParam(const nsString& aName,
                                     const nsString& aNamespace,
                                     const nsString& aSelect,
                                     const nsString& aValue,
                                     nsIDOMNode* aContext)
{
    nsresult rv = NS_OK;

    if (aSelect.IsVoid() == aValue.IsVoid()) {
        // Ignore if neither or both are specified
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<txAExprResult> value;
    if (!aSelect.IsVoid()) {

        // Set up context
        nsAutoPtr<txXPathNode> contextNode(
          txXPathNativeNode::createXPathNode(aContext));
        NS_ENSURE_TRUE(contextNode, NS_ERROR_OUT_OF_MEMORY);

        if (!mRecycler) {
            mRecycler = new txResultRecycler;
            NS_ENSURE_TRUE(mRecycler, NS_ERROR_OUT_OF_MEMORY);

            rv = mRecycler->init();
            NS_ENSURE_SUCCESS(rv, rv);
        }

        txXSLTParamContext paramContext(&mParamNamespaceMap, *contextNode,
                                        mRecycler);

        // Parse
        nsAutoPtr<Expr> expr;
        rv = txExprParser::createExpr(aSelect, &paramContext,
                                      getter_Transfers(expr));
        NS_ENSURE_SUCCESS(rv, rv);

        // Evaluate
        rv = expr->evaluate(&paramContext, getter_AddRefs(value));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        value = new StringResult(aValue, nullptr);
        NS_ENSURE_TRUE(value, NS_ERROR_OUT_OF_MEMORY);
    }

    nsCOMPtr<nsIAtom> name = do_GetAtom(aName);
    int32_t nsId = kNameSpaceID_Unknown;
    rv = nsContentUtils::NameSpaceManager()->
        RegisterNameSpace(aNamespace, nsId);
    NS_ENSURE_SUCCESS(rv, rv);

    txExpandedName varName(nsId, name);
    txVariable* var = static_cast<txVariable*>(mVariables.get(varName));
    if (var) {
        var->setValue(value);
        
        return NS_OK;
    }

    var = new txVariable(value);
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    return mVariables.add(varName, var);
}

class nsTransformBlockerEvent : public nsRunnable {
public:
  nsRefPtr<txMozillaXSLTProcessor> mProcessor;

  nsTransformBlockerEvent(txMozillaXSLTProcessor *processor)
    : mProcessor(processor)
  {}

  ~nsTransformBlockerEvent()
  {
    nsCOMPtr<nsIDocument> document =
        do_QueryInterface(mProcessor->GetSourceContentModel());
    document->UnblockOnload(true);
  }

  NS_IMETHOD Run()
  {
    mProcessor->TransformToDoc(nullptr, false);
    return NS_OK;
  }
};

nsresult
txMozillaXSLTProcessor::DoTransform()
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(mStylesheet, NS_ERROR_UNEXPECTED);
    NS_ASSERTION(mObserver, "no observer");
    NS_ASSERTION(NS_IsMainThread(), "should only be on main thread");

    nsresult rv;
    nsCOMPtr<nsIDocument> document = do_QueryInterface(mSource, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRunnable> event = new nsTransformBlockerEvent(this);
    if (!event) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    document->BlockOnload();

    rv = NS_DispatchToCurrentThread(event);
    if (NS_FAILED(rv)) {
        // XXX Maybe we should just display the source document in this case?
        //     Also, set up context information, see bug 204655.
        reportError(rv, nullptr, nullptr);
    }

    return rv;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::ImportStylesheet(nsIDOMNode *aStyle)
{
    NS_ENSURE_TRUE(aStyle, NS_ERROR_NULL_POINTER);
    
    // We don't support importing multiple stylesheets yet.
    NS_ENSURE_TRUE(!mStylesheetDocument && !mStylesheet,
                   NS_ERROR_NOT_IMPLEMENTED);

    if (!nsContentUtils::CanCallerAccess(aStyle)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }
    
    nsCOMPtr<nsINode> styleNode = do_QueryInterface(aStyle);
    NS_ENSURE_TRUE(styleNode &&
                   (styleNode->IsElement() ||
                    styleNode->IsNodeOfType(nsINode::eDOCUMENT)),
                   NS_ERROR_INVALID_ARG);

    nsresult rv = TX_CompileStylesheet(styleNode, this, mPrincipal,
                                       getter_AddRefs(mStylesheet));
    // XXX set up exception context, bug 204658
    NS_ENSURE_SUCCESS(rv, rv);

    if (styleNode->IsElement()) {
        mStylesheetDocument = styleNode->OwnerDoc();
        NS_ENSURE_TRUE(mStylesheetDocument, NS_ERROR_UNEXPECTED);

        mEmbeddedStylesheetRoot = static_cast<nsIContent*>(styleNode.get());
    }
    else {
        mStylesheetDocument = static_cast<nsIDocument*>(styleNode.get());
    }

    mStylesheetDocument->AddMutationObserver(this);

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::TransformToDocument(nsIDOMNode *aSource,
                                            nsIDOMDocument **aResult)
{
    NS_ENSURE_ARG(aSource);
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_SUCCESS(mCompileResult, mCompileResult);

    if (!nsContentUtils::CanCallerAccess(aSource)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsresult rv = ensureStylesheet();
    NS_ENSURE_SUCCESS(rv, rv);

    mSource = aSource;

    return TransformToDoc(aResult, true);
}

nsresult
txMozillaXSLTProcessor::TransformToDoc(nsIDOMDocument **aResult,
                                       bool aCreateDataDocument)
{
    nsAutoPtr<txXPathNode> sourceNode(txXPathNativeNode::createXPathNode(mSource));
    if (!sourceNode) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIDOMDocument> sourceDOMDocument;
    mSource->GetOwnerDocument(getter_AddRefs(sourceDOMDocument));
    if (!sourceDOMDocument) {
        sourceDOMDocument = do_QueryInterface(mSource);
    }

    txExecutionState es(mStylesheet, IsLoadDisabled());

    // XXX Need to add error observers

    // If aResult is non-null, we're a data document
    txToDocHandlerFactory handlerFactory(&es, sourceDOMDocument, mObserver,
                                         aCreateDataDocument);
    es.mOutputHandlerFactory = &handlerFactory;

    nsresult rv = es.init(*sourceNode, &mVariables);

    // Process root of XML source document
    if (NS_SUCCEEDED(rv)) {
        rv = txXSLTProcessor::execute(es);
    }
    
    nsresult endRv = es.end(rv);
    if (NS_SUCCEEDED(rv)) {
      rv = endRv;
    }
    
    if (NS_SUCCEEDED(rv)) {
        if (aResult) {
            txAOutputXMLEventHandler* handler =
                static_cast<txAOutputXMLEventHandler*>(es.mOutputHandler);
            handler->getOutputDocument(aResult);
            nsCOMPtr<nsIDocument> doc = do_QueryInterface(*aResult);
            MOZ_ASSERT(doc->GetReadyStateEnum() ==
                       nsIDocument::READYSTATE_INTERACTIVE, "Bad readyState");
            doc->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);
        }
    }
    else if (mObserver) {
        // XXX set up context information, bug 204655
        reportError(rv, nullptr, nullptr);
    }

    return rv;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::TransformToFragment(nsIDOMNode *aSource,
                                            nsIDOMDocument *aOutput,
                                            nsIDOMDocumentFragment **aResult)
{
    NS_ENSURE_ARG(aSource);
    NS_ENSURE_ARG(aOutput);
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_SUCCESS(mCompileResult, mCompileResult);

    if (!nsContentUtils::CanCallerAccess(aSource) ||
        !nsContentUtils::CanCallerAccess(aOutput)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsresult rv = ensureStylesheet();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txXPathNode> sourceNode(txXPathNativeNode::createXPathNode(aSource));
    if (!sourceNode) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    txExecutionState es(mStylesheet, IsLoadDisabled());

    // XXX Need to add error observers

    rv = aOutput->CreateDocumentFragment(aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    txToFragmentHandlerFactory handlerFactory(*aResult);
    es.mOutputHandlerFactory = &handlerFactory;

    rv = es.init(*sourceNode, &mVariables);

    // Process root of XML source document
    if (NS_SUCCEEDED(rv)) {
        rv = txXSLTProcessor::execute(es);
    }
    // XXX setup exception context, bug 204658
    nsresult endRv = es.end(rv);
    if (NS_SUCCEEDED(rv)) {
      rv = endRv;
    }

    return rv;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::SetParameter(const nsAString & aNamespaceURI,
                                     const nsAString & aLocalName,
                                     nsIVariant *aValue)
{
    NS_ENSURE_ARG(aValue);

    nsCOMPtr<nsIVariant> value = aValue;

    uint16_t dataType;
    value->GetDataType(&dataType);
    switch (dataType) {
        // Number
        case nsIDataType::VTYPE_INT8:
        case nsIDataType::VTYPE_INT16:
        case nsIDataType::VTYPE_INT32:
        case nsIDataType::VTYPE_INT64:
        case nsIDataType::VTYPE_UINT8:
        case nsIDataType::VTYPE_UINT16:
        case nsIDataType::VTYPE_UINT32:
        case nsIDataType::VTYPE_UINT64:
        case nsIDataType::VTYPE_FLOAT:
        case nsIDataType::VTYPE_DOUBLE:

        // Boolean
        case nsIDataType::VTYPE_BOOL:

        // String
        case nsIDataType::VTYPE_CHAR:
        case nsIDataType::VTYPE_WCHAR:
        case nsIDataType::VTYPE_DOMSTRING:
        case nsIDataType::VTYPE_CHAR_STR:
        case nsIDataType::VTYPE_WCHAR_STR:
        case nsIDataType::VTYPE_STRING_SIZE_IS:
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        case nsIDataType::VTYPE_UTF8STRING:
        case nsIDataType::VTYPE_CSTRING:
        case nsIDataType::VTYPE_ASTRING:
        {
            break;
        }

        // Nodeset
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS:
        {
            nsCOMPtr<nsISupports> supports;
            nsresult rv = value->GetAsISupports(getter_AddRefs(supports));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIDOMNode> node = do_QueryInterface(supports);
            if (node) {
                if (!nsContentUtils::CanCallerAccess(node)) {
                    return NS_ERROR_DOM_SECURITY_ERR;
                }

                break;
            }

            nsCOMPtr<nsIXPathResult> xpathResult = do_QueryInterface(supports);
            if (xpathResult) {
                nsRefPtr<txAExprResult> result;
                nsresult rv = xpathResult->GetExprResult(getter_AddRefs(result));
                NS_ENSURE_SUCCESS(rv, rv);

                if (result->getResultType() == txAExprResult::NODESET) {
                    txNodeSet *nodeSet =
                        static_cast<txNodeSet*>
                                   (static_cast<txAExprResult*>(result));

                    nsCOMPtr<nsIDOMNode> node;
                    int32_t i, count = nodeSet->size();
                    for (i = 0; i < count; ++i) {
                        rv = txXPathNativeNode::getNode(nodeSet->get(i),
                                                        getter_AddRefs(node));
                        NS_ENSURE_SUCCESS(rv, rv);

                        if (!nsContentUtils::CanCallerAccess(node)) {
                            return NS_ERROR_DOM_SECURITY_ERR;
                        }
                    }
                }

                // Clone the nsXPathResult so that mutations don't affect this
                // variable.
                nsCOMPtr<nsIXPathResult> clone;
                rv = xpathResult->Clone(getter_AddRefs(clone));
                NS_ENSURE_SUCCESS(rv, rv);

                nsCOMPtr<nsIWritableVariant> variant =
                    do_CreateInstance("@mozilla.org/variant;1", &rv);
                NS_ENSURE_SUCCESS(rv, rv);

                rv = variant->SetAsISupports(clone);
                NS_ENSURE_SUCCESS(rv, rv);

                value = variant;

                break;
            }

            nsCOMPtr<nsIDOMNodeList> nodeList = do_QueryInterface(supports);
            if (nodeList) {
                uint32_t length;
                nodeList->GetLength(&length);

                nsCOMPtr<nsIDOMNode> node;
                uint32_t i;
                for (i = 0; i < length; ++i) {
                    nodeList->Item(i, getter_AddRefs(node));

                    if (!nsContentUtils::CanCallerAccess(node)) {
                        return NS_ERROR_DOM_SECURITY_ERR;
                    }
                }

                break;
            }

            // Random JS Objects will be converted to a string.
            nsCOMPtr<nsIXPConnectJSObjectHolder> holder =
                do_QueryInterface(supports);
            if (holder) {
                break;
            }

            // We don't know how to handle this type of param.
            return NS_ERROR_ILLEGAL_VALUE;
        }

        case nsIDataType::VTYPE_ARRAY:
        {
            uint16_t type;
            nsIID iid;
            uint32_t count;
            void* array;
            nsresult rv = value->GetAsArray(&type, &iid, &count, &array);
            NS_ENSURE_SUCCESS(rv, rv);

            if (type != nsIDataType::VTYPE_INTERFACE &&
                type != nsIDataType::VTYPE_INTERFACE_IS) {
                nsMemory::Free(array);

                // We only support arrays of DOM nodes.
                return NS_ERROR_ILLEGAL_VALUE;
            }

            nsISupports** values = static_cast<nsISupports**>(array);

            uint32_t i;
            for (i = 0; i < count; ++i) {
                nsISupports *supports = values[i];
                nsCOMPtr<nsIDOMNode> node = do_QueryInterface(supports);

                if (node) {
                    rv = nsContentUtils::CanCallerAccess(node) ? NS_OK :
                         NS_ERROR_DOM_SECURITY_ERR;
                }
                else {
                    // We only support arrays of DOM nodes.
                    rv = NS_ERROR_ILLEGAL_VALUE;
                }

                if (NS_FAILED(rv)) {
                    while (i < count) {
                        NS_IF_RELEASE(values[i]);
                        ++i;
                    }
                    nsMemory::Free(array);

                    return rv;
                }

                NS_RELEASE(supports);
            }

            nsMemory::Free(array);

            break;
        }

        default:
        {
            return NS_ERROR_FAILURE;
        }        
    }

    int32_t nsId = kNameSpaceID_Unknown;
    nsresult rv = nsContentUtils::NameSpaceManager()->
        RegisterNameSpace(aNamespaceURI, nsId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(aLocalName);
    txExpandedName varName(nsId, localName);

    txVariable* var = static_cast<txVariable*>(mVariables.get(varName));
    if (var) {
        var->setValue(value);
        return NS_OK;
    }

    var = new txVariable(value);
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    return mVariables.add(varName, var);
}

NS_IMETHODIMP
txMozillaXSLTProcessor::GetParameter(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIVariant **aResult)
{
    int32_t nsId = kNameSpaceID_Unknown;
    nsresult rv = nsContentUtils::NameSpaceManager()->
        RegisterNameSpace(aNamespaceURI, nsId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(aLocalName);
    txExpandedName varName(nsId, localName);

    txVariable* var = static_cast<txVariable*>(mVariables.get(varName));
    if (var) {
        return var->getValue(aResult);
    }
    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::RemoveParameter(const nsAString& aNamespaceURI,
                                        const nsAString& aLocalName)
{
    int32_t nsId = kNameSpaceID_Unknown;
    nsresult rv = nsContentUtils::NameSpaceManager()->
        RegisterNameSpace(aNamespaceURI, nsId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(aLocalName);
    txExpandedName varName(nsId, localName);

    mVariables.remove(varName);
    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::ClearParameters()
{
    mVariables.clear();

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::Reset()
{
    if (mStylesheetDocument) {
        mStylesheetDocument->RemoveMutationObserver(this);
    }
    mStylesheet = nullptr;
    mStylesheetDocument = nullptr;
    mEmbeddedStylesheetRoot = nullptr;
    mCompileResult = NS_OK;
    mVariables.clear();

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::SetFlags(uint32_t aFlags)
{
    NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(),
                   NS_ERROR_DOM_SECURITY_ERR);

    mFlags = aFlags;

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::GetFlags(uint32_t* aFlags)
{
    NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(),
                   NS_ERROR_DOM_SECURITY_ERR);

    *aFlags = mFlags;

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::LoadStyleSheet(nsIURI* aUri, nsILoadGroup* aLoadGroup)
{
    nsresult rv = TX_LoadSheet(aUri, this, aLoadGroup, mPrincipal);
    if (NS_FAILED(rv) && mObserver) {
        // This is most likely a network or security error, just
        // use the uri as context.
        nsAutoCString spec;
        aUri->GetSpec(spec);
        CopyUTF8toUTF16(spec, mSourceText);
        nsresult status = NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_XSLT ? rv :
                          NS_ERROR_XSLT_NETWORK_ERROR;
        reportError(status, nullptr, nullptr);
    }
    return rv;
}

nsresult
txMozillaXSLTProcessor::setStylesheet(txStylesheet* aStylesheet)
{
    mStylesheet = aStylesheet;
    if (mSource) {
        return DoTransform();
    }
    return NS_OK;
}

void
txMozillaXSLTProcessor::reportError(nsresult aResult,
                                    const PRUnichar *aErrorText,
                                    const PRUnichar *aSourceText)
{
    if (!mObserver) {
        return;
    }

    mTransformResult = aResult;

    if (aErrorText) {
        mErrorText.Assign(aErrorText);
    }
    else {
        nsCOMPtr<nsIStringBundleService> sbs =
            mozilla::services::GetStringBundleService();
        if (sbs) {
            nsXPIDLString errorText;
            sbs->FormatStatusMessage(aResult, EmptyString().get(),
                                     getter_Copies(errorText));

            nsXPIDLString errorMessage;
            nsCOMPtr<nsIStringBundle> bundle;
            sbs->CreateBundle(XSLT_MSGS_URL, getter_AddRefs(bundle));

            if (bundle) {
                const PRUnichar* error[] = { errorText.get() };
                if (mStylesheet) {
                    bundle->FormatStringFromName(NS_LITERAL_STRING("TransformError").get(),
                                                 error, 1,
                                                 getter_Copies(errorMessage));
                }
                else {
                    bundle->FormatStringFromName(NS_LITERAL_STRING("LoadingError").get(),
                                                 error, 1,
                                                 getter_Copies(errorMessage));
                }
            }
            mErrorText.Assign(errorMessage);
        }
    }

    if (aSourceText) {
        mSourceText.Assign(aSourceText);
    }

    if (mSource) {
        notifyError();
    }
}

void
txMozillaXSLTProcessor::notifyError()
{
    nsresult rv;
    nsCOMPtr<nsIDOMDocument> errorDocument = do_CreateInstance(kXMLDocumentCID,
                                                               &rv);
    if (NS_FAILED(rv)) {
        return;
    }

    // Set up the document
    nsCOMPtr<nsIDocument> document = do_QueryInterface(errorDocument);
    if (!document) {
        return;
    }
    URIUtils::ResetWithSource(document, mSource);

    MOZ_ASSERT(document->GetReadyStateEnum() ==
                 nsIDocument::READYSTATE_UNINITIALIZED,
               "Bad readyState.");
    document->SetReadyStateInternal(nsIDocument::READYSTATE_LOADING);

    NS_NAMED_LITERAL_STRING(ns, "http://www.mozilla.org/newlayout/xml/parsererror.xml");

    nsCOMPtr<nsIDOMElement> element;
    rv = errorDocument->CreateElementNS(ns, NS_LITERAL_STRING("parsererror"),
                                        getter_AddRefs(element));
    if (NS_FAILED(rv)) {
        return;
    }

    nsCOMPtr<nsIDOMNode> resultNode;
    rv = errorDocument->AppendChild(element, getter_AddRefs(resultNode));
    if (NS_FAILED(rv)) {
        return;
    }

    nsCOMPtr<nsIDOMText> text;
    rv = errorDocument->CreateTextNode(mErrorText, getter_AddRefs(text));
    if (NS_FAILED(rv)) {
        return;
    }

    rv = element->AppendChild(text, getter_AddRefs(resultNode));
    if (NS_FAILED(rv)) {
        return;
    }

    if (!mSourceText.IsEmpty()) {
        nsCOMPtr<nsIDOMElement> sourceElement;
        rv = errorDocument->CreateElementNS(ns,
                                            NS_LITERAL_STRING("sourcetext"),
                                            getter_AddRefs(sourceElement));
        if (NS_FAILED(rv)) {
            return;
        }
    
        rv = element->AppendChild(sourceElement, getter_AddRefs(resultNode));
        if (NS_FAILED(rv)) {
            return;
        }

        rv = errorDocument->CreateTextNode(mSourceText, getter_AddRefs(text));
        if (NS_FAILED(rv)) {
            return;
        }
    
        rv = sourceElement->AppendChild(text, getter_AddRefs(resultNode));
        if (NS_FAILED(rv)) {
            return;
        }
    }

    MOZ_ASSERT(document->GetReadyStateEnum() ==
                 nsIDocument::READYSTATE_LOADING,
               "Bad readyState.");
    document->SetReadyStateInternal(nsIDocument::READYSTATE_INTERACTIVE);

    mObserver->OnTransformDone(mTransformResult, document);
}

nsresult
txMozillaXSLTProcessor::ensureStylesheet()
{
    if (mStylesheet) {
        return NS_OK;
    }

    NS_ENSURE_TRUE(mStylesheetDocument, NS_ERROR_NOT_INITIALIZED);

    nsINode* style = mEmbeddedStylesheetRoot;
    if (!style) {
        style = mStylesheetDocument;
    }

    return TX_CompileStylesheet(style, this, mPrincipal,
                                getter_AddRefs(mStylesheet));
}

void
txMozillaXSLTProcessor::NodeWillBeDestroyed(const nsINode* aNode)
{
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
    if (NS_FAILED(mCompileResult)) {
        return;
    }

    mCompileResult = ensureStylesheet();
    mStylesheetDocument = nullptr;
    mEmbeddedStylesheetRoot = nullptr;
}

void
txMozillaXSLTProcessor::CharacterDataChanged(nsIDocument* aDocument,
                                             nsIContent *aContent,
                                             CharacterDataChangeInfo* aInfo)
{
    mStylesheet = nullptr;
}

void
txMozillaXSLTProcessor::AttributeChanged(nsIDocument* aDocument,
                                         Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         int32_t aModType)
{
    mStylesheet = nullptr;
}

void
txMozillaXSLTProcessor::ContentAppended(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aFirstNewContent,
                                        int32_t /* unused */)
{
    mStylesheet = nullptr;
}

void
txMozillaXSLTProcessor::ContentInserted(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aChild,
                                        int32_t /* unused */)
{
    mStylesheet = nullptr;
}

void
txMozillaXSLTProcessor::ContentRemoved(nsIDocument* aDocument,
                                       nsIContent* aContainer,
                                       nsIContent* aChild,
                                       int32_t aIndexInContainer,
                                       nsIContent* aPreviousSibling)
{
    mStylesheet = nullptr;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::Initialize(nsISupports* aOwner, JSContext* cx,
                                   JSObject* obj, const JS::CallArgs& args)
{
    nsCOMPtr<nsIPrincipal> prin;
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ENSURE_TRUE(secMan, NS_ERROR_UNEXPECTED);

    nsresult rv = secMan->GetSubjectPrincipal(getter_AddRefs(prin));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(prin, NS_ERROR_UNEXPECTED);

    return Init(prin);
}

NS_IMETHODIMP
txMozillaXSLTProcessor::Init(nsIPrincipal* aPrincipal)
{
    NS_ENSURE_ARG_POINTER(aPrincipal);
    mPrincipal = aPrincipal;

    return NS_OK;
}

/* static*/
nsresult
txMozillaXSLTProcessor::Startup()
{
    if (!txXSLTProcessor::init()) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIErrorService> errorService =
        do_GetService(NS_ERRORSERVICE_CONTRACTID);
    if (errorService) {
        errorService->RegisterErrorStringBundle(NS_ERROR_MODULE_XSLT,
                                                XSLT_MSGS_URL);
    }

    return NS_OK;
}

/* static*/
void
txMozillaXSLTProcessor::Shutdown()
{
    txXSLTProcessor::shutdown();

    nsCOMPtr<nsIErrorService> errorService =
        do_GetService(NS_ERRORSERVICE_CONTRACTID);
    if (errorService) {
        errorService->UnregisterErrorStringBundle(NS_ERROR_MODULE_XSLT);
    }
}

/* static*/
nsresult
txVariable::Convert(nsIVariant *aValue, txAExprResult** aResult)
{
    *aResult = nullptr;

    uint16_t dataType;
    aValue->GetDataType(&dataType);
    switch (dataType) {
        // Number
        case nsIDataType::VTYPE_INT8:
        case nsIDataType::VTYPE_INT16:
        case nsIDataType::VTYPE_INT32:
        case nsIDataType::VTYPE_INT64:
        case nsIDataType::VTYPE_UINT8:
        case nsIDataType::VTYPE_UINT16:
        case nsIDataType::VTYPE_UINT32:
        case nsIDataType::VTYPE_UINT64:
        case nsIDataType::VTYPE_FLOAT:
        case nsIDataType::VTYPE_DOUBLE:
        {
            double value;
            nsresult rv = aValue->GetAsDouble(&value);
            NS_ENSURE_SUCCESS(rv, rv);

            *aResult = new NumberResult(value, nullptr);
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

            NS_ADDREF(*aResult);

            return NS_OK;
        }

        // Boolean
        case nsIDataType::VTYPE_BOOL:
        {
            bool value;
            nsresult rv = aValue->GetAsBool(&value);
            NS_ENSURE_SUCCESS(rv, rv);

            *aResult = new BooleanResult(value);
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

            NS_ADDREF(*aResult);

            return NS_OK;
        }

        // String
        case nsIDataType::VTYPE_CHAR:
        case nsIDataType::VTYPE_WCHAR:
        case nsIDataType::VTYPE_DOMSTRING:
        case nsIDataType::VTYPE_CHAR_STR:
        case nsIDataType::VTYPE_WCHAR_STR:
        case nsIDataType::VTYPE_STRING_SIZE_IS:
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        case nsIDataType::VTYPE_UTF8STRING:
        case nsIDataType::VTYPE_CSTRING:
        case nsIDataType::VTYPE_ASTRING:
        {
            nsAutoString value;
            nsresult rv = aValue->GetAsAString(value);
            NS_ENSURE_SUCCESS(rv, rv);

            *aResult = new StringResult(value, nullptr);
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

            NS_ADDREF(*aResult);

            return NS_OK;
        }

        // Nodeset
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS:
        {
            nsCOMPtr<nsISupports> supports;
            nsresult rv = aValue->GetAsISupports(getter_AddRefs(supports));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIDOMNode> node = do_QueryInterface(supports);
            if (node) {
                nsAutoPtr<txXPathNode> xpathNode(txXPathNativeNode::createXPathNode(node));
                if (!xpathNode) {
                    return NS_ERROR_FAILURE;
                }

                *aResult = new txNodeSet(*xpathNode, nullptr);
                if (!*aResult) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }

                NS_ADDREF(*aResult);

                return NS_OK;
            }

            nsCOMPtr<nsIXPathResult> xpathResult = do_QueryInterface(supports);
            if (xpathResult) {
                return xpathResult->GetExprResult(aResult);
            }

            nsCOMPtr<nsIDOMNodeList> nodeList = do_QueryInterface(supports);
            if (nodeList) {
                nsRefPtr<txNodeSet> nodeSet = new txNodeSet(nullptr);
                if (!nodeSet) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }

                uint32_t length;
                nodeList->GetLength(&length);

                nsCOMPtr<nsIDOMNode> node;
                uint32_t i;
                for (i = 0; i < length; ++i) {
                    nodeList->Item(i, getter_AddRefs(node));

                    nsAutoPtr<txXPathNode> xpathNode(
                        txXPathNativeNode::createXPathNode(node));
                    if (!xpathNode) {
                        return NS_ERROR_FAILURE;
                    }

                    nodeSet->add(*xpathNode);
                }

                NS_ADDREF(*aResult = nodeSet);

                return NS_OK;
            }

            // Convert random JS Objects to a string.
            nsCOMPtr<nsIXPConnectJSObjectHolder> holder =
                do_QueryInterface(supports);
            if (holder) {
                JSContext* cx = nsContentUtils::GetCurrentJSContext();
                NS_ENSURE_TRUE(cx, NS_ERROR_NOT_AVAILABLE);

                JSObject *jsobj;
                rv = holder->GetJSObject(&jsobj);
                NS_ENSURE_SUCCESS(rv, rv);

                JSString *str = JS_ValueToString(cx, OBJECT_TO_JSVAL(jsobj));
                NS_ENSURE_TRUE(str, NS_ERROR_FAILURE);

                nsDependentJSString value;
                NS_ENSURE_TRUE(value.init(cx, str), NS_ERROR_FAILURE);

                *aResult = new StringResult(value, nullptr);
                if (!*aResult) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }

                NS_ADDREF(*aResult);

                return NS_OK;
            }

            break;
        }

        case nsIDataType::VTYPE_ARRAY:
        {
            uint16_t type;
            nsIID iid;
            uint32_t count;
            void* array;
            nsresult rv = aValue->GetAsArray(&type, &iid, &count, &array);
            NS_ENSURE_SUCCESS(rv, rv);

            NS_ASSERTION(type == nsIDataType::VTYPE_INTERFACE ||
                         type == nsIDataType::VTYPE_INTERFACE_IS,
                         "Huh, we checked this in SetParameter?");

            nsISupports** values = static_cast<nsISupports**>(array);

            nsRefPtr<txNodeSet> nodeSet = new txNodeSet(nullptr);
            if (!nodeSet) {
                NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, values);

                return NS_ERROR_OUT_OF_MEMORY;
            }

            uint32_t i;
            for (i = 0; i < count; ++i) {
                nsISupports *supports = values[i];
                nsCOMPtr<nsIDOMNode> node = do_QueryInterface(supports);
                NS_ASSERTION(node, "Huh, we checked this in SetParameter?");

                nsAutoPtr<txXPathNode> xpathNode(
                    txXPathNativeNode::createXPathNode(node));
                if (!xpathNode) {
                    while (i < count) {
                        NS_RELEASE(values[i]);
                        ++i;
                    }
                    nsMemory::Free(array);

                    return NS_ERROR_FAILURE;
                }

                nodeSet->add(*xpathNode);

                NS_RELEASE(supports);
            }

            nsMemory::Free(array);

            NS_ADDREF(*aResult = nodeSet);

            return NS_OK;
        }
    }

    return NS_ERROR_ILLEGAL_VALUE;
}
