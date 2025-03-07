/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txMozillaXSLTProcessor.h"
#include "nsError.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Document.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "XPathResult.h"
#include "txExecutionState.h"
#include "txMozillaTextOutput.h"
#include "txMozillaXMLOutput.h"
#include "txURIUtils.h"
#include "txXMLUtils.h"
#include "txUnknownHandler.h"
#include "txXSLTMsgsURL.h"
#include "txXSLTProcessor.h"
#include "nsIPrincipal.h"
#include "nsThreadUtils.h"
#include "jsapi.h"
#include "txExprParser.h"
#include "nsJSUtils.h"
#include "nsIXPConnect.h"
#include "nsNameSpaceManager.h"
#include "nsVariant.h"
#include "nsTextNode.h"
#include "mozilla/Components.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/XSLTProcessorBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

/**
 * Output Handler Factories
 */
class txToDocHandlerFactory : public txAOutputHandlerFactory {
 public:
  txToDocHandlerFactory(txExecutionState* aEs, Document* aSourceDocument,
                        nsITransformObserver* aObserver, bool aDocumentIsData)
      : mEs(aEs),
        mSourceDocument(aSourceDocument),
        mObserver(aObserver),
        mDocumentIsData(aDocumentIsData) {}

  TX_DECL_TXAOUTPUTHANDLERFACTORY

 private:
  txExecutionState* mEs;
  nsCOMPtr<Document> mSourceDocument;
  nsCOMPtr<nsITransformObserver> mObserver;
  bool mDocumentIsData;
};

class txToFragmentHandlerFactory : public txAOutputHandlerFactory {
 public:
  explicit txToFragmentHandlerFactory(DocumentFragment* aFragment)
      : mFragment(aFragment) {}

  TX_DECL_TXAOUTPUTHANDLERFACTORY

 private:
  RefPtr<DocumentFragment> mFragment;
};

nsresult txToDocHandlerFactory::createHandlerWith(
    txOutputFormat* aFormat, txAXMLEventHandler** aHandler) {
  *aHandler = nullptr;
  switch (aFormat->mMethod) {
    case eMethodNotSet:
    case eXMLOutput: {
      *aHandler = new txUnknownHandler(mEs);
      return NS_OK;
    }

    case eHTMLOutput: {
      UniquePtr<txMozillaXMLOutput> handler(
          new txMozillaXMLOutput(mSourceDocument, aFormat, mObserver));

      nsresult rv = handler->createResultDocument(
          u""_ns, kNameSpaceID_None, mSourceDocument, mDocumentIsData);
      if (NS_SUCCEEDED(rv)) {
        *aHandler = handler.release();
      }

      return rv;
    }

    case eTextOutput: {
      UniquePtr<txMozillaTextOutput> handler(
          new txMozillaTextOutput(mSourceDocument, mObserver));

      nsresult rv = handler->createResultDocument(mDocumentIsData);
      if (NS_SUCCEEDED(rv)) {
        *aHandler = handler.release();
      }

      return rv;
    }
  }

  MOZ_CRASH("Unknown output method");

  return NS_ERROR_FAILURE;
}

nsresult txToDocHandlerFactory::createHandlerWith(
    txOutputFormat* aFormat, const nsAString& aName, int32_t aNsID,
    txAXMLEventHandler** aHandler) {
  *aHandler = nullptr;
  switch (aFormat->mMethod) {
    case eMethodNotSet: {
      NS_ERROR("How can method not be known when root element is?");
      return NS_ERROR_UNEXPECTED;
    }

    case eXMLOutput:
    case eHTMLOutput: {
      UniquePtr<txMozillaXMLOutput> handler(
          new txMozillaXMLOutput(mSourceDocument, aFormat, mObserver));

      nsresult rv = handler->createResultDocument(aName, aNsID, mSourceDocument,
                                                  mDocumentIsData);
      if (NS_SUCCEEDED(rv)) {
        *aHandler = handler.release();
      }

      return rv;
    }

    case eTextOutput: {
      UniquePtr<txMozillaTextOutput> handler(
          new txMozillaTextOutput(mSourceDocument, mObserver));

      nsresult rv = handler->createResultDocument(mDocumentIsData);
      if (NS_SUCCEEDED(rv)) {
        *aHandler = handler.release();
      }

      return rv;
    }
  }

  MOZ_CRASH("Unknown output method");

  return NS_ERROR_FAILURE;
}

nsresult txToFragmentHandlerFactory::createHandlerWith(
    txOutputFormat* aFormat, txAXMLEventHandler** aHandler) {
  *aHandler = nullptr;
  switch (aFormat->mMethod) {
    case eMethodNotSet: {
      txOutputFormat format;
      format.merge(*aFormat);
      nsCOMPtr<Document> doc = mFragment->OwnerDoc();

      if (doc->IsHTMLDocument()) {
        format.mMethod = eHTMLOutput;
      } else {
        format.mMethod = eXMLOutput;
      }

      *aHandler = new txMozillaXMLOutput(&format, mFragment, false);
      break;
    }

    case eXMLOutput:
    case eHTMLOutput: {
      *aHandler = new txMozillaXMLOutput(aFormat, mFragment, false);
      break;
    }

    case eTextOutput: {
      *aHandler = new txMozillaTextOutput(mFragment);
      break;
    }
  }
  return NS_OK;
}

nsresult txToFragmentHandlerFactory::createHandlerWith(
    txOutputFormat* aFormat, const nsAString& aName, int32_t aNsID,
    txAXMLEventHandler** aHandler) {
  *aHandler = nullptr;
  NS_ASSERTION(aFormat->mMethod != eMethodNotSet,
               "How can method not be known when root element is?");
  NS_ENSURE_TRUE(aFormat->mMethod != eMethodNotSet, NS_ERROR_UNEXPECTED);
  return createHandlerWith(aFormat, aHandler);
}

class txVariable : public txIGlobalParameter {
  using XSLTParameterValue = txMozillaXSLTProcessor::XSLTParameterValue;
  using OwningXSLTParameterValue =
      txMozillaXSLTProcessor::OwningXSLTParameterValue;

 public:
  explicit txVariable(UniquePtr<OwningXSLTParameterValue>&& aValue)
      : mUnionValue(std::move(aValue)) {}
  nsresult getValue(txAExprResult** aValue) override {
    if (!mValue) {
      nsresult rv = convert(*mUnionValue, getter_AddRefs(mValue));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ADDREF(*aValue = mValue);

    return NS_OK;
  }
  OwningXSLTParameterValue getUnionValue() {
    return OwningXSLTParameterValue(*mUnionValue);
  }
  void setValue(UniquePtr<OwningXSLTParameterValue>&& aValue) {
    mValue = nullptr;
    mUnionValue = std::move(aValue);
  }

  static UniquePtr<OwningXSLTParameterValue> convertToOwning(
      const XSLTParameterValue& aValue, ErrorResult& aError);

  friend void ImplCycleCollectionTraverse(
      nsCycleCollectionTraversalCallback& aCallback, txVariable& aVariable,
      const char* aName, uint32_t aFlags);

 private:
  static nsresult convert(const OwningXSLTParameterValue& aUnionValue,
                          txAExprResult** aValue);

  UniquePtr<OwningXSLTParameterValue> mUnionValue;
  RefPtr<txAExprResult> mValue;
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, txVariable& aVariable,
    const char* aName, uint32_t aFlags) {
  ImplCycleCollectionTraverse(aCallback, *aVariable.mUnionValue, aName, aFlags);
}

inline void ImplCycleCollectionUnlink(
    txOwningExpandedNameMap<txIGlobalParameter>& aMap) {
  aMap.clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    txOwningExpandedNameMap<txIGlobalParameter>& aMap, const char* aName,
    uint32_t aFlags = 0) {
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  txOwningExpandedNameMap<txIGlobalParameter>::iterator iter(aMap);
  while (iter.next()) {
    ImplCycleCollectionTraverse(
        aCallback, *static_cast<txVariable*>(iter.value()), aName, aFlags);
  }
}

/**
 * txMozillaXSLTProcessor
 */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(txMozillaXSLTProcessor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(txMozillaXSLTProcessor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner, mSource)
  tmp->Reset();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(txMozillaXSLTProcessor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(
      mOwner, mStylesheetDocument, mEmbeddedStylesheetRoot, mSource, mVariables)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(txMozillaXSLTProcessor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(txMozillaXSLTProcessor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(txMozillaXSLTProcessor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDocumentTransformer)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocumentTransformer)
NS_INTERFACE_MAP_END

txMozillaXSLTProcessor::txMozillaXSLTProcessor()
    : mOwner(nullptr),
      mTransformResult(NS_OK),
      mCompileResult(NS_OK),
      mFlags(0) {}

txMozillaXSLTProcessor::txMozillaXSLTProcessor(nsISupports* aOwner)
    : mOwner(aOwner),
      mTransformResult(NS_OK),
      mCompileResult(NS_OK),
      mFlags(0) {}

txMozillaXSLTProcessor::~txMozillaXSLTProcessor() { Reset(); }

NS_IMETHODIMP
txMozillaXSLTProcessor::SetTransformObserver(nsITransformObserver* aObserver) {
  mObserver = aObserver;
  return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::SetSourceContentModel(nsINode* aSource) {
  mSource = aSource;

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
                                              const nsString& aNamespace) {
  RefPtr<nsAtom> pre = NS_Atomize(aPrefix);
  return mParamNamespaceMap.mapNamespace(pre, aNamespace);
}

class txXSLTParamContext : public txIParseContext, public txIEvalContext {
 public:
  txXSLTParamContext(txNamespaceMap* aResolver, const txXPathNode& aContext,
                     txResultRecycler* aRecycler)
      : mResolver(aResolver), mContext(aContext), mRecycler(aRecycler) {}

  // txIParseContext
  int32_t resolveNamespacePrefix(nsAtom* aPrefix) override {
    return mResolver->lookupNamespace(aPrefix);
  }
  nsresult resolveFunctionCall(nsAtom* aName, int32_t aID,
                               FunctionCall** aFunction) override {
    return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
  }
  bool caseInsensitiveNameTests() override { return false; }
  void SetErrorOffset(uint32_t aOffset) override {}

  // txIEvalContext
  nsresult getVariable(int32_t aNamespace, nsAtom* aLName,
                       txAExprResult*& aResult) override {
    aResult = nullptr;
    return NS_ERROR_INVALID_ARG;
  }
  nsresult isStripSpaceAllowed(const txXPathNode& aNode,
                               bool& aAllowed) override {
    aAllowed = false;

    return NS_OK;
  }
  void* getPrivateContext() override { return nullptr; }
  txResultRecycler* recycler() override { return mRecycler; }
  void receiveError(const nsAString& aMsg, nsresult aRes) override {}
  const txXPathNode& getContextNode() override { return mContext; }
  uint32_t size() override { return 1; }
  uint32_t position() override { return 1; }

 private:
  txNamespaceMap* mResolver;
  const txXPathNode& mContext;
  txResultRecycler* mRecycler;
};

NS_IMETHODIMP
txMozillaXSLTProcessor::AddXSLTParam(const nsString& aName,
                                     const nsString& aNamespace,
                                     const nsString& aSelect,
                                     const nsString& aValue,
                                     nsINode* aContext) {
  nsresult rv = NS_OK;

  if (aSelect.IsVoid() == aValue.IsVoid()) {
    // Ignore if neither or both are specified
    return NS_ERROR_FAILURE;
  }

  RefPtr<txAExprResult> value;
  uint16_t resultType;
  if (!aSelect.IsVoid()) {
    // Set up context
    UniquePtr<txXPathNode> contextNode(
        txXPathNativeNode::createXPathNode(aContext));
    NS_ENSURE_TRUE(contextNode, NS_ERROR_OUT_OF_MEMORY);

    if (!mRecycler) {
      mRecycler = new txResultRecycler;
    }

    txXSLTParamContext paramContext(&mParamNamespaceMap, *contextNode,
                                    mRecycler);

    // Parse
    UniquePtr<Expr> expr;
    rv = txExprParser::createExpr(aSelect, &paramContext,
                                  getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);

    // Evaluate
    rv = expr->evaluate(&paramContext, getter_AddRefs(value));
    NS_ENSURE_SUCCESS(rv, rv);

    switch (value->getResultType()) {
      case txAExprResult::NUMBER:
        resultType = XPathResult::NUMBER_TYPE;
        break;
      case txAExprResult::STRING:
        resultType = XPathResult::STRING_TYPE;
        break;
      case txAExprResult::BOOLEAN:
        resultType = XPathResult::BOOLEAN_TYPE;
        break;
      case txAExprResult::NODESET:
        resultType = XPathResult::UNORDERED_NODE_ITERATOR_TYPE;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE(
            "We shouldn't have a txAExprResult::RESULT_TREE_FRAGMENT here.");
        return NS_ERROR_FAILURE;
    }
  } else {
    value = new StringResult(aValue, nullptr);
    resultType = XPathResult::STRING_TYPE;
  }

  RefPtr<nsAtom> name = NS_Atomize(aName);
  int32_t nsId = kNameSpaceID_Unknown;
  rv = nsNameSpaceManager::GetInstance()->RegisterNameSpace(aNamespace, nsId);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<XPathResult> xpathResult = MakeRefPtr<XPathResult>(aContext);

  ErrorResult error;
  xpathResult->SetExprResult(value, resultType, aContext, error);
  if (error.Failed()) {
    return error.StealNSResult();
  }

  UniquePtr<OwningXSLTParameterValue> varValue =
      MakeUnique<OwningXSLTParameterValue>();
  varValue->SetAsXPathResult() = xpathResult.forget();

  txExpandedName varName(nsId, name);
  txVariable* var = static_cast<txVariable*>(mVariables.get(varName));
  if (var) {
    var->setValue(std::move(varValue));

    return NS_OK;
  }

  var = new txVariable(std::move(varValue));

  return mVariables.add(varName, var);
}

class nsTransformBlockerEvent : public mozilla::Runnable {
 public:
  RefPtr<txMozillaXSLTProcessor> mProcessor;

  explicit nsTransformBlockerEvent(txMozillaXSLTProcessor* processor)
      : mozilla::Runnable("nsTransformBlockerEvent"), mProcessor(processor) {}

  ~nsTransformBlockerEvent() {
    nsCOMPtr<Document> document =
        mProcessor->GetSourceContentModel()->OwnerDoc();
    document->UnblockOnload(true);
  }

  NS_IMETHOD Run() override {
    mProcessor->TransformToDoc(nullptr, false);
    return NS_OK;
  }
};

nsresult txMozillaXSLTProcessor::DoTransform() {
  NS_ENSURE_TRUE(mSource, NS_ERROR_UNEXPECTED);
  NS_ENSURE_TRUE(mStylesheet, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(mObserver, "no observer");
  NS_ASSERTION(NS_IsMainThread(), "should only be on main thread");

  nsCOMPtr<nsIRunnable> event = new nsTransformBlockerEvent(this);
  mSource->OwnerDoc()->BlockOnload();
  nsresult rv = NS_DispatchToCurrentThread(event);
  if (NS_FAILED(rv)) {
    // XXX Maybe we should just display the source document in this case?
    //     Also, set up context information, see bug 204655.
    reportError(rv, nullptr, nullptr);
  }

  return rv;
}

void txMozillaXSLTProcessor::ImportStylesheet(nsINode& aStyle,
                                              mozilla::ErrorResult& aRv) {
  // We don't support importing multiple stylesheets yet.
  if (NS_WARN_IF(mStylesheetDocument || mStylesheet)) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  MOZ_ASSERT(!mEmbeddedStylesheetRoot);

  mCompileResult = NS_OK;

  if (!nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller()->Subsumes(
          aStyle.NodePrincipal())) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (NS_WARN_IF(!aStyle.IsElement() && !aStyle.IsDocument())) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsresult rv =
      TX_CompileStylesheet(&aStyle, this, getter_AddRefs(mStylesheet));
  // XXX set up exception context, bug 204658
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  mStylesheetDocument = aStyle.OwnerDoc();
  if (aStyle.IsElement()) {
    mEmbeddedStylesheetRoot = aStyle.AsElement();
  }

  mStylesheetDocument->AddMutationObserver(this);
}

already_AddRefed<Document> txMozillaXSLTProcessor::TransformToDocument(
    nsINode& aSource, ErrorResult& aRv) {
  if (NS_WARN_IF(NS_FAILED(mCompileResult))) {
    aRv.Throw(mCompileResult);
    return nullptr;
  }

  if (!nsContentUtils::CanCallerAccess(&aSource)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsresult rv = ensureStylesheet();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  mSource = &aSource;

  nsCOMPtr<Document> doc;
  rv = TransformToDoc(getter_AddRefs(doc), true);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }
  return doc.forget();
}

class XSLTProcessRequest final : public nsIRequest {
 public:
  explicit XSLTProcessRequest(txExecutionState* aState) : mState(aState) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST

  void Done() { mState = nullptr; }

 private:
  ~XSLTProcessRequest() {}
  txExecutionState* mState;
};
NS_IMPL_ISUPPORTS(XSLTProcessRequest, nsIRequest)

NS_IMETHODIMP
XSLTProcessRequest::GetName(nsACString& aResult) {
  aResult.AssignLiteral("about:xslt-load-blocker");
  return NS_OK;
}

NS_IMETHODIMP
XSLTProcessRequest::IsPending(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
XSLTProcessRequest::GetStatus(nsresult* status) {
  *status = NS_OK;
  return NS_OK;
}

NS_IMETHODIMP XSLTProcessRequest::SetCanceledReason(const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP XSLTProcessRequest::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP XSLTProcessRequest::CancelWithReason(nsresult aStatus,
                                                   const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

NS_IMETHODIMP
XSLTProcessRequest::Cancel(nsresult status) {
  mState->stopProcessing();
  return NS_OK;
}

NS_IMETHODIMP
XSLTProcessRequest::Suspend(void) { return NS_OK; }

NS_IMETHODIMP
XSLTProcessRequest::Resume(void) { return NS_OK; }

NS_IMETHODIMP
XSLTProcessRequest::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  *aLoadGroup = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
XSLTProcessRequest::SetLoadGroup(nsILoadGroup* aLoadGroup) { return NS_OK; }

NS_IMETHODIMP
XSLTProcessRequest::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = nsIRequest::LOAD_NORMAL;
  return NS_OK;
}

NS_IMETHODIMP
XSLTProcessRequest::SetLoadFlags(nsLoadFlags aLoadFlags) { return NS_OK; }

NS_IMETHODIMP
XSLTProcessRequest::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
XSLTProcessRequest::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

nsresult txMozillaXSLTProcessor::TransformToDoc(Document** aResult,
                                                bool aCreateDataDocument) {
  UniquePtr<txXPathNode> sourceNode(
      txXPathNativeNode::createXPathNode(mSource));
  if (!sourceNode) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  txExecutionState es(mStylesheet, IsLoadDisabled());

  Document* sourceDoc = mSource->OwnerDoc();
  nsCOMPtr<nsILoadGroup> loadGroup = sourceDoc->GetDocumentLoadGroup();
  if (!loadGroup) {
    nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(mOwner);
    if (win && win->IsCurrentInnerWindow()) {
      Document* doc = win->GetDoc();
      if (doc) {
        loadGroup = doc->GetDocumentLoadGroup();
      }
    }

    if (!loadGroup) {
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<XSLTProcessRequest> xsltProcessRequest = new XSLTProcessRequest(&es);
  loadGroup->AddRequest(xsltProcessRequest, nullptr);

  // XXX Need to add error observers

  // If aResult is non-null, we're a data document
  txToDocHandlerFactory handlerFactory(&es, sourceDoc, mObserver,
                                       aCreateDataDocument);
  es.mOutputHandlerFactory = &handlerFactory;

  nsresult rv = es.init(*sourceNode, &mVariables);

  // Process root of XML source document
  if (NS_SUCCEEDED(rv)) {
    rv = txXSLTProcessor::execute(es);
  }

  xsltProcessRequest->Done();
  loadGroup->RemoveRequest(xsltProcessRequest, nullptr, NS_OK);

  nsresult endRv = es.end(rv);
  if (NS_SUCCEEDED(rv)) {
    rv = endRv;
  }

  if (NS_SUCCEEDED(rv)) {
    if (aResult) {
      txAOutputXMLEventHandler* handler =
          static_cast<txAOutputXMLEventHandler*>(es.mOutputHandler);
      nsCOMPtr<Document> doc;
      handler->getOutputDocument(getter_AddRefs(doc));
      MOZ_ASSERT(doc->GetReadyStateEnum() == Document::READYSTATE_INTERACTIVE,
                 "Bad readyState");
      doc->SetReadyStateInternal(Document::READYSTATE_COMPLETE);
      doc.forget(aResult);
    }
  } else if (mObserver) {
    // XXX set up context information, bug 204655
    reportError(rv, nullptr, nullptr);
  }

  return rv;
}

already_AddRefed<DocumentFragment> txMozillaXSLTProcessor::TransformToFragment(
    nsINode& aSource, Document& aOutput, ErrorResult& aRv) {
  if (NS_WARN_IF(NS_FAILED(mCompileResult))) {
    aRv.Throw(mCompileResult);
    return nullptr;
  }

  nsIPrincipal* subject =
      nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller();
  if (!subject->Subsumes(aSource.NodePrincipal()) ||
      !subject->Subsumes(aOutput.NodePrincipal())) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsresult rv = ensureStylesheet();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  UniquePtr<txXPathNode> sourceNode(
      txXPathNativeNode::createXPathNode(&aSource));
  if (!sourceNode) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  txExecutionState es(mStylesheet, IsLoadDisabled());

  // XXX Need to add error observers

  RefPtr<DocumentFragment> frag = aOutput.CreateDocumentFragment();
  txToFragmentHandlerFactory handlerFactory(frag);
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

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return frag.forget();
}

void txMozillaXSLTProcessor::SetParameter(const nsAString& aNamespaceURI,
                                          const nsAString& aLocalName,
                                          const XSLTParameterValue& aValue,
                                          ErrorResult& aError) {
  if (aValue.IsNode()) {
    if (!nsContentUtils::CanCallerAccess(&aValue.GetAsNode())) {
      aError.ThrowSecurityError("Caller is not allowed to access node.");
      return;
    }
  } else if (aValue.IsNodeSequence()) {
    const Sequence<OwningNonNull<nsINode>>& values = aValue.GetAsNodeSequence();
    for (const auto& node : values) {
      if (!nsContentUtils::CanCallerAccess(node.get())) {
        aError.ThrowSecurityError(
            "Caller is not allowed to access node in sequence.");
        return;
      }
    }
  } else if (aValue.IsXPathResult()) {
    XPathResult& xpathResult = aValue.GetAsXPathResult();
    RefPtr<txAExprResult> result;
    aError = xpathResult.GetExprResult(getter_AddRefs(result));
    if (aError.Failed()) {
      return;
    }

    if (result->getResultType() == txAExprResult::NODESET) {
      txNodeSet* nodeSet =
          static_cast<txNodeSet*>(static_cast<txAExprResult*>(result));

      int32_t i, count = nodeSet->size();
      for (i = 0; i < count; ++i) {
        nsINode* node = txXPathNativeNode::getNode(nodeSet->get(i));
        if (!nsContentUtils::CanCallerAccess(node)) {
          aError.ThrowSecurityError(
              "Caller is not allowed to access node in node-set.");
          return;
        }
      }
    }
  }

  int32_t nsId = kNameSpaceID_Unknown;
  aError =
      nsNameSpaceManager::GetInstance()->RegisterNameSpace(aNamespaceURI, nsId);
  if (aError.Failed()) {
    return;
  }

  RefPtr<nsAtom> localName = NS_Atomize(aLocalName);
  txExpandedName varName(nsId, localName);

  UniquePtr<OwningXSLTParameterValue> value =
      txVariable::convertToOwning(aValue, aError);
  if (aError.Failed()) {
    return;
  }

  txVariable* var = static_cast<txVariable*>(mVariables.get(varName));
  if (var) {
    var->setValue(std::move(value));
    return;
  }

  UniquePtr<txVariable> newVar = MakeUnique<txVariable>(std::move(value));
  mVariables.add(varName, newVar.release());
}

void txMozillaXSLTProcessor::GetParameter(
    const nsAString& aNamespaceURI, const nsAString& aLocalName,
    Nullable<OwningXSLTParameterValue>& aValue, ErrorResult& aRv) {
  int32_t nsId = kNameSpaceID_Unknown;
  nsresult rv =
      nsNameSpaceManager::GetInstance()->RegisterNameSpace(aNamespaceURI, nsId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  RefPtr<nsAtom> localName = NS_Atomize(aLocalName);
  txExpandedName varName(nsId, localName);

  txVariable* var = static_cast<txVariable*>(mVariables.get(varName));
  if (!var) {
    return;
  }

  aValue.SetValue(var->getUnionValue());
}

void txMozillaXSLTProcessor::RemoveParameter(const nsAString& aNamespaceURI,
                                             const nsAString& aLocalName,
                                             ErrorResult& aRv) {
  int32_t nsId = kNameSpaceID_Unknown;
  nsresult rv =
      nsNameSpaceManager::GetInstance()->RegisterNameSpace(aNamespaceURI, nsId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  RefPtr<nsAtom> localName = NS_Atomize(aLocalName);
  txExpandedName varName(nsId, localName);

  mVariables.remove(varName);
}

void txMozillaXSLTProcessor::ClearParameters() { mVariables.clear(); }

void txMozillaXSLTProcessor::Reset() {
  if (mStylesheetDocument) {
    mStylesheetDocument->RemoveMutationObserver(this);
  }
  mStylesheet = nullptr;
  mStylesheetDocument = nullptr;
  mEmbeddedStylesheetRoot = nullptr;
  mCompileResult = NS_OK;
  mVariables.clear();
}

void txMozillaXSLTProcessor::SetFlags(uint32_t aFlags, SystemCallerGuarantee) {
  mFlags = aFlags;
}

uint32_t txMozillaXSLTProcessor::Flags(SystemCallerGuarantee) { return mFlags; }

NS_IMETHODIMP
txMozillaXSLTProcessor::LoadStyleSheet(nsIURI* aUri,
                                       Document* aLoaderDocument) {
  mozilla::dom::ReferrerPolicy refpol = mozilla::dom::ReferrerPolicy::_empty;
  if (mStylesheetDocument) {
    refpol = mStylesheetDocument->GetReferrerPolicy();
  }

  nsresult rv = TX_LoadSheet(aUri, this, aLoaderDocument, refpol);
  if (NS_FAILED(rv) && mObserver) {
    // This is most likely a network or security error, just
    // use the uri as context.
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CopyUTF8toUTF16(spec, mSourceText);
    nsresult status = NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_XSLT
                          ? rv
                          : NS_ERROR_XSLT_NETWORK_ERROR;
    reportError(status, nullptr, nullptr);
  }
  return rv;
}

nsresult txMozillaXSLTProcessor::setStylesheet(txStylesheet* aStylesheet) {
  mStylesheet = aStylesheet;
  if (mSource) {
    return DoTransform();
  }
  return NS_OK;
}

void txMozillaXSLTProcessor::reportError(nsresult aResult,
                                         const char16_t* aErrorText,
                                         const char16_t* aSourceText) {
  if (!mObserver) {
    return;
  }

  mTransformResult = aResult;

  if (aErrorText) {
    mErrorText.Assign(aErrorText);
  } else {
    nsCOMPtr<nsIStringBundleService> sbs =
        mozilla::components::StringBundle::Service();
    if (sbs) {
      nsString errorText;
      sbs->FormatStatusMessage(aResult, u"", errorText);

      nsAutoString errorMessage;
      nsCOMPtr<nsIStringBundle> bundle;
      sbs->CreateBundle(XSLT_MSGS_URL, getter_AddRefs(bundle));

      if (bundle) {
        AutoTArray<nsString, 1> error = {errorText};
        if (mStylesheet) {
          bundle->FormatStringFromName("TransformError", error, errorMessage);
        } else {
          bundle->FormatStringFromName("LoadingError", error, errorMessage);
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

void txMozillaXSLTProcessor::notifyError() {
  nsCOMPtr<Document> document;
  {
    nsresult rv = NS_NewXMLDocument(getter_AddRefs(document), nullptr, nullptr);
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  URIUtils::ResetWithSource(document, mSource);

  MOZ_ASSERT(
      document->GetReadyStateEnum() == Document::READYSTATE_UNINITIALIZED,
      "Bad readyState.");
  document->SetReadyStateInternal(Document::READYSTATE_LOADING);

  constexpr auto ns =
      u"http://www.mozilla.org/newlayout/xml/parsererror.xml"_ns;

  IgnoredErrorResult rv;
  ElementCreationOptionsOrString options;
  Unused << options.SetAsString();

  nsCOMPtr<Element> element =
      document->CreateElementNS(ns, u"parsererror"_ns, options, rv);
  if (rv.Failed()) {
    return;
  }

  document->AppendChild(*element, rv);
  if (rv.Failed()) {
    return;
  }

  RefPtr<nsTextNode> text = document->CreateTextNode(mErrorText);

  element->AppendChild(*text, rv);
  if (rv.Failed()) {
    return;
  }

  if (!mSourceText.IsEmpty()) {
    ElementCreationOptionsOrString options;
    Unused << options.SetAsString();

    nsCOMPtr<Element> sourceElement =
        document->CreateElementNS(ns, u"sourcetext"_ns, options, rv);
    if (rv.Failed()) {
      return;
    }

    element->AppendChild(*sourceElement, rv);
    if (rv.Failed()) {
      return;
    }

    text = document->CreateTextNode(mSourceText);

    sourceElement->AppendChild(*text, rv);
    if (rv.Failed()) {
      return;
    }
  }

  MOZ_ASSERT(document->GetReadyStateEnum() == Document::READYSTATE_LOADING,
             "Bad readyState.");
  document->SetReadyStateInternal(Document::READYSTATE_INTERACTIVE);
  mObserver->OnTransformDone(mSource->OwnerDoc(), mTransformResult, document);
}

nsresult txMozillaXSLTProcessor::ensureStylesheet() {
  if (mStylesheet) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(mStylesheetDocument, NS_ERROR_NOT_INITIALIZED);

  nsINode* style = mEmbeddedStylesheetRoot;
  if (!style) {
    style = mStylesheetDocument;
  }

  return TX_CompileStylesheet(style, this, getter_AddRefs(mStylesheet));
}

void txMozillaXSLTProcessor::NodeWillBeDestroyed(nsINode* aNode) {
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  if (NS_FAILED(mCompileResult)) {
    return;
  }

  mCompileResult = ensureStylesheet();
  mStylesheetDocument = nullptr;
  mEmbeddedStylesheetRoot = nullptr;
}

void txMozillaXSLTProcessor::CharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo&) {
  mStylesheet = nullptr;
}

void txMozillaXSLTProcessor::AttributeChanged(Element* aElement,
                                              int32_t aNameSpaceID,
                                              nsAtom* aAttribute,
                                              int32_t aModType,
                                              const nsAttrValue* aOldValue) {
  mStylesheet = nullptr;
}

void txMozillaXSLTProcessor::ContentAppended(nsIContent* aFirstNewContent) {
  mStylesheet = nullptr;
}

void txMozillaXSLTProcessor::ContentInserted(nsIContent* aChild) {
  mStylesheet = nullptr;
}

void txMozillaXSLTProcessor::ContentRemoved(nsIContent* aChild,
                                            nsIContent* aPreviousSibling) {
  mStylesheet = nullptr;
}

/* virtual */
JSObject* txMozillaXSLTProcessor::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return XSLTProcessor_Binding::Wrap(aCx, this, aGivenProto);
}

DocGroup* txMozillaXSLTProcessor::GetDocGroup() const {
  return mStylesheetDocument ? mStylesheetDocument->GetDocGroup() : nullptr;
}

/* static */
already_AddRefed<txMozillaXSLTProcessor> txMozillaXSLTProcessor::Constructor(
    const GlobalObject& aGlobal) {
  RefPtr<txMozillaXSLTProcessor> processor =
      new txMozillaXSLTProcessor(aGlobal.GetAsSupports());
  return processor.forget();
}

/* static*/
nsresult txMozillaXSLTProcessor::Startup() {
  if (!txXSLTProcessor::init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* static*/
void txMozillaXSLTProcessor::Shutdown() { txXSLTProcessor::shutdown(); }

/* static */
UniquePtr<txVariable::OwningXSLTParameterValue> txVariable::convertToOwning(
    const XSLTParameterValue& aValue, ErrorResult& aError) {
  UniquePtr<OwningXSLTParameterValue> value =
      MakeUnique<OwningXSLTParameterValue>();
  if (aValue.IsUnrestrictedDouble()) {
    value->SetAsUnrestrictedDouble() = aValue.GetAsUnrestrictedDouble();
  } else if (aValue.IsBoolean()) {
    value->SetAsBoolean() = aValue.GetAsBoolean();
  } else if (aValue.IsString()) {
    value->SetAsString() = aValue.GetAsString();
  } else if (aValue.IsNode()) {
    value->SetAsNode() = aValue.GetAsNode();
  } else if (aValue.IsNodeSequence()) {
    value->SetAsNodeSequence() = aValue.GetAsNodeSequence();
  } else if (aValue.IsXPathResult()) {
    // Clone the XPathResult so that mutations don't affect this variable.
    RefPtr<XPathResult> clone = aValue.GetAsXPathResult().Clone(aError);
    if (aError.Failed()) {
      return nullptr;
    }
    value->SetAsXPathResult() = *clone;
  } else {
    MOZ_ASSERT(false, "Unknown type?");
  }
  return value;
}

/* static */
nsresult txVariable::convert(const OwningXSLTParameterValue& aUnionValue,
                             txAExprResult** aValue) {
  if (aUnionValue.IsUnrestrictedDouble()) {
    NS_ADDREF(*aValue = new NumberResult(aUnionValue.GetAsUnrestrictedDouble(),
                                         nullptr));
    return NS_OK;
  }

  if (aUnionValue.IsBoolean()) {
    NS_ADDREF(*aValue = new BooleanResult(aUnionValue.GetAsBoolean()));
    return NS_OK;
  }

  if (aUnionValue.IsString()) {
    NS_ADDREF(*aValue = new StringResult(aUnionValue.GetAsString(), nullptr));
    return NS_OK;
  }

  if (aUnionValue.IsNode()) {
    nsINode& node = aUnionValue.GetAsNode();
    UniquePtr<txXPathNode> xpathNode(txXPathNativeNode::createXPathNode(&node));
    if (!xpathNode) {
      return NS_ERROR_FAILURE;
    }

    NS_ADDREF(*aValue = new txNodeSet(*xpathNode, nullptr));
    return NS_OK;
  }

  if (aUnionValue.IsNodeSequence()) {
    RefPtr<txNodeSet> nodeSet(new txNodeSet(nullptr));
    const Sequence<OwningNonNull<nsINode>>& values =
        aUnionValue.GetAsNodeSequence();
    for (const auto& node : values) {
      UniquePtr<txXPathNode> xpathNode(
          txXPathNativeNode::createXPathNode(node.get()));
      if (!xpathNode) {
        return NS_ERROR_FAILURE;
      }

      nodeSet->append(*xpathNode);
    }
    nodeSet.forget(aValue);
    return NS_OK;
  }

  MOZ_ASSERT(aUnionValue.IsXPathResult());

  XPathResult& xpathResult = aUnionValue.GetAsXPathResult();
  if (xpathResult.ResultType() == XPathResult::NUMBER_TYPE) {
    IgnoredErrorResult rv;
    NS_ADDREF(*aValue =
                  new NumberResult(xpathResult.GetNumberValue(rv), nullptr));
    MOZ_ASSERT(!rv.Failed());
    return NS_OK;
  }

  if (xpathResult.ResultType() == XPathResult::BOOLEAN_TYPE) {
    IgnoredErrorResult rv;
    NS_ADDREF(*aValue = new BooleanResult(xpathResult.GetBooleanValue(rv)));
    MOZ_ASSERT(!rv.Failed());
    return NS_OK;
  }

  if (xpathResult.ResultType() == XPathResult::STRING_TYPE) {
    IgnoredErrorResult rv;
    nsString value;
    xpathResult.GetStringValue(value, rv);
    NS_ADDREF(*aValue = new StringResult(value, nullptr));
    MOZ_ASSERT(!rv.Failed());
    return NS_OK;
  }

  // If the XPathResult holds a nodeset, then it will keep the nodes alive and
  // we'll hold the XPathResult alive.
  return xpathResult.GetExprResult(aValue);
}
