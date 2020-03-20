/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "mozilla/dom/PrototypeDocumentContentSink.h"
#include "nsIParser.h"
#include "mozilla/dom/Document.h"
#include "nsIContent.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsHTMLParts.h"
#include "nsCRT.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsDocElementCreatedNotificationRunner.h"
#include "nsIScriptContext.h"
#include "nsNameSpaceManager.h"
#include "nsIScriptError.h"
#include "prtime.h"
#include "mozilla/Logging.h"
#include "nsRect.h"
#include "nsIScriptElement.h"
#include "nsStyleLinkElement.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIChannel.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsError.h"
#include "nsIScriptGlobalObject.h"
#include "mozAutoDocUpdate.h"
#include "nsMimeTypes.h"
#include "nsHtml5SVGLoadDispatcher.h"
#include "nsTextNode.h"
#include "mozilla/dom/CDATASection.h"
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/PresShell.h"

#include "nsXULPrototypeCache.h"
#include "nsXULElement.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "js/CompilationAndEvaluation.h"

using namespace mozilla;
using namespace mozilla::dom;

LazyLogModule PrototypeDocumentContentSink::gLog("PrototypeDocument");

nsresult NS_NewPrototypeDocumentContentSink(nsIContentSink** aResult,
                                            Document* aDoc, nsIURI* aURI,
                                            nsISupports* aContainer,
                                            nsIChannel* aChannel) {
  MOZ_ASSERT(nullptr != aResult, "null ptr");
  if (nullptr == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<PrototypeDocumentContentSink> it = new PrototypeDocumentContentSink();

  nsresult rv = it->Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  it.forget(aResult);
  return NS_OK;
}

namespace mozilla::dom {

PrototypeDocumentContentSink::PrototypeDocumentContentSink()
    : mNextSrcLoadWaiter(nullptr),
      mCurrentScriptProto(nullptr),
      mOffThreadCompiling(false),
      mOffThreadCompileStringBuf(nullptr),
      mOffThreadCompileStringLength(0),
      mStillWalking(false),
      mPendingSheets(0) {}

PrototypeDocumentContentSink::~PrototypeDocumentContentSink() {
  NS_ASSERTION(
      mNextSrcLoadWaiter == nullptr,
      "unreferenced document still waiting for script source to load?");

  if (mOffThreadCompileStringBuf) {
    js_free(mOffThreadCompileStringBuf);
  }
}

nsresult PrototypeDocumentContentSink::Init(Document* aDoc, nsIURI* aURI,
                                            nsISupports* aContainer,
                                            nsIChannel* aChannel) {
  MOZ_ASSERT(aDoc, "null ptr");
  MOZ_ASSERT(aURI, "null ptr");

  mDocument = aDoc;

  mDocument->SetDelayFrameLoaderInitialization(true);
  mDocument->SetMayStartLayout(false);

  // Get the URI.  Note that this should match nsDocShell::OnLoadingSite
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(mDocumentURI));
  NS_ENSURE_SUCCESS(rv, rv);

  mScriptLoader = mDocument->ScriptLoader();

  mNodeInfoManager = aDoc->NodeInfoManager();

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION(PrototypeDocumentContentSink, mParser, mDocumentURI,
                         mDocument, mNodeInfoManager, mScriptLoader,
                         mCurrentPrototype)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PrototypeDocumentContentSink)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIOffThreadScriptReceiver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PrototypeDocumentContentSink)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PrototypeDocumentContentSink)

//----------------------------------------------------------------------
//
// nsIContentSink interface
//

void PrototypeDocumentContentSink::SetDocumentCharset(
    NotNull<const Encoding*> aEncoding) {
  if (mDocument) {
    mDocument->SetDocumentCharacterSet(aEncoding);
  }
}

nsISupports* PrototypeDocumentContentSink::GetTarget() {
  return ToSupports(mDocument);
}

bool PrototypeDocumentContentSink::IsScriptExecuting() {
  return !!mScriptLoader->GetCurrentScript();
}

NS_IMETHODIMP
PrototypeDocumentContentSink::SetParser(nsParserBase* aParser) {
  MOZ_ASSERT(aParser, "Should have a parser here!");
  mParser = aParser;
  return NS_OK;
}

nsIParser* PrototypeDocumentContentSink::GetParser() {
  return static_cast<nsIParser*>(mParser.get());
}

void PrototypeDocumentContentSink::ContinueInterruptedParsingIfEnabled() {
  if (mParser && mParser->IsParserEnabled()) {
    GetParser()->ContinueInterruptedParsing();
  }
}

void PrototypeDocumentContentSink::ContinueInterruptedParsingAsync() {
  nsCOMPtr<nsIRunnable> ev = NewRunnableMethod(
      "PrototypeDocumentContentSink::ContinueInterruptedParsingIfEnabled", this,
      &PrototypeDocumentContentSink::ContinueInterruptedParsingIfEnabled);

  mDocument->Dispatch(mozilla::TaskCategory::Other, ev.forget());
}

//----------------------------------------------------------------------
//
// PrototypeDocumentContentSink::ContextStack
//

PrototypeDocumentContentSink::ContextStack::ContextStack()
    : mTop(nullptr), mDepth(0) {}

PrototypeDocumentContentSink::ContextStack::~ContextStack() {
  while (mTop) {
    Entry* doomed = mTop;
    mTop = mTop->mNext;
    NS_IF_RELEASE(doomed->mElement);
    delete doomed;
  }
}

nsresult PrototypeDocumentContentSink::ContextStack::Push(
    nsXULPrototypeElement* aPrototype, nsIContent* aElement) {
  Entry* entry = new Entry;
  entry->mPrototype = aPrototype;
  entry->mElement = aElement;
  NS_IF_ADDREF(entry->mElement);
  entry->mIndex = 0;

  entry->mNext = mTop;
  mTop = entry;

  ++mDepth;
  return NS_OK;
}

nsresult PrototypeDocumentContentSink::ContextStack::Pop() {
  if (mDepth == 0) return NS_ERROR_UNEXPECTED;

  Entry* doomed = mTop;
  mTop = mTop->mNext;
  --mDepth;

  NS_IF_RELEASE(doomed->mElement);
  delete doomed;
  return NS_OK;
}

nsresult PrototypeDocumentContentSink::ContextStack::Peek(
    nsXULPrototypeElement** aPrototype, nsIContent** aElement,
    int32_t* aIndex) {
  if (mDepth == 0) return NS_ERROR_UNEXPECTED;

  *aPrototype = mTop->mPrototype;
  *aElement = mTop->mElement;
  NS_IF_ADDREF(*aElement);
  *aIndex = mTop->mIndex;

  return NS_OK;
}

nsresult PrototypeDocumentContentSink::ContextStack::SetTopIndex(
    int32_t aIndex) {
  if (mDepth == 0) return NS_ERROR_UNEXPECTED;

  mTop->mIndex = aIndex;
  return NS_OK;
}

//----------------------------------------------------------------------
//
// Content model walking routines
//

nsresult PrototypeDocumentContentSink::OnPrototypeLoadDone(
    nsXULPrototypeDocument* aPrototype) {
  mCurrentPrototype = aPrototype;
  mDocument->SetPrototypeDocument(aPrototype);

  nsresult rv = PrepareToWalk();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ResumeWalk();

  return rv;
}

nsresult PrototypeDocumentContentSink::PrepareToWalk() {
  MOZ_ASSERT(mCurrentPrototype);
  nsresult rv;

  mStillWalking = true;

  // Notify document that the load is beginning
  mDocument->BeginLoad();

  // Get the prototype's root element and initialize the context
  // stack for the prototype walk.
  nsXULPrototypeElement* proto = mCurrentPrototype->GetRootElement();

  if (!proto) {
    if (MOZ_LOG_TEST(gLog, LogLevel::Error)) {
      nsCOMPtr<nsIURI> url = mCurrentPrototype->GetURI();

      nsAutoCString urlspec;
      rv = url->GetSpec(urlspec);
      if (NS_FAILED(rv)) return rv;

      MOZ_LOG(gLog, LogLevel::Error,
              ("prototype: error parsing '%s'", urlspec.get()));
    }

    return NS_OK;
  }

  nsINode* nodeToInsertBefore = mDocument->GetFirstChild();

  const nsTArray<RefPtr<nsXULPrototypePI> >& processingInstructions =
      mCurrentPrototype->GetProcessingInstructions();

  uint32_t total = processingInstructions.Length();
  for (uint32_t i = 0; i < total; ++i) {
    rv = CreateAndInsertPI(processingInstructions[i], mDocument,
                           nodeToInsertBefore);
    if (NS_FAILED(rv)) return rv;
  }

  // Do one-time initialization.
  RefPtr<Element> root;

  // Add the root element
  rv = CreateElementFromPrototype(proto, getter_AddRefs(root), true);
  if (NS_FAILED(rv)) return rv;

  rv = mDocument->AppendChildTo(root, false);
  if (NS_FAILED(rv)) return rv;

  // TODO(emilio): Should this really notify? We don't notify of appends anyhow,
  // and we just appended the root so no styles can possibly depend on it.
  mDocument->UpdateDocumentStates(NS_DOCUMENT_STATE_RTL_LOCALE, true);

  nsContentUtils::AddScriptRunner(
      new nsDocElementCreatedNotificationRunner(mDocument));

  // There'd better not be anything on the context stack at this
  // point! This is the basis case for our "induction" in
  // ResumeWalk(), below, which'll assume that there's always a
  // content element on the context stack if we're in the document.
  NS_ASSERTION(mContextStack.Depth() == 0,
               "something's on the context stack already");
  if (mContextStack.Depth() != 0) return NS_ERROR_UNEXPECTED;

  rv = mContextStack.Push(proto, root);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

nsresult PrototypeDocumentContentSink::CreateAndInsertPI(
    const nsXULPrototypePI* aProtoPI, nsINode* aParent, nsINode* aBeforeThis) {
  MOZ_ASSERT(aProtoPI, "null ptr");
  MOZ_ASSERT(aParent, "null ptr");

  RefPtr<ProcessingInstruction> node = NS_NewXMLProcessingInstruction(
      mNodeInfoManager, aProtoPI->mTarget, aProtoPI->mData);

  nsresult rv;
  if (aProtoPI->mTarget.EqualsLiteral("xml-stylesheet")) {
    rv = InsertXMLStylesheetPI(aProtoPI, aParent, aBeforeThis, node);
  } else {
    // No special processing, just add the PI to the document.
    rv = aParent->InsertChildBefore(
        node->AsContent(), aBeforeThis ? aBeforeThis->AsContent() : nullptr,
        false);
  }

  return rv;
}

nsresult PrototypeDocumentContentSink::InsertXMLStylesheetPI(
    const nsXULPrototypePI* aProtoPI, nsINode* aParent, nsINode* aBeforeThis,
    nsIContent* aPINode) {
  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aPINode));
  NS_ASSERTION(ssle,
               "passed XML Stylesheet node does not "
               "implement nsIStyleSheetLinkingElement!");

  nsresult rv;

  ssle->InitStyleLinkElement(false);
  // We want to be notified when the style sheet finishes loading, so
  // disable style sheet loading for now.
  ssle->SetEnableUpdates(false);
  ssle->OverrideBaseURI(mCurrentPrototype->GetURI());

  rv = aParent->InsertChildBefore(
      aPINode->AsContent(), aBeforeThis ? aBeforeThis->AsContent() : nullptr,
      false);
  if (NS_FAILED(rv)) return rv;

  ssle->SetEnableUpdates(true);

  // load the stylesheet if necessary, passing ourselves as
  // nsICSSObserver
  auto result = ssle->UpdateStyleSheet(this);
  if (result.isErr()) {
    // Ignore errors from UpdateStyleSheet; we don't want failure to
    // do that to break the XUL document load.  But do propagate out
    // NS_ERROR_OUT_OF_MEMORY.
    if (result.unwrapErr() == NS_ERROR_OUT_OF_MEMORY) {
      return result.unwrapErr();
    }
    return NS_OK;
  }

  auto update = result.unwrap();
  if (update.ShouldBlock()) {
    ++mPendingSheets;
  }

  return NS_OK;
}

void PrototypeDocumentContentSink::CloseElement(Element* aElement) {
  if (nsIContent::RequiresDoneAddingChildren(
          aElement->NodeInfo()->NamespaceID(),
          aElement->NodeInfo()->NameAtom())) {
    aElement->DoneAddingChildren(false);
  }
}

nsresult PrototypeDocumentContentSink::ResumeWalk() {
  nsresult rv = ResumeWalkInternal();
  if (NS_FAILED(rv)) {
    nsContentUtils::ReportToConsoleNonLocalized(
        NS_LITERAL_STRING("Failed to load document from prototype document."),
        nsIScriptError::errorFlag, NS_LITERAL_CSTRING("Prototype Document"),
        mDocument, mDocumentURI);
  }
  return rv;
}

nsresult PrototypeDocumentContentSink::ResumeWalkInternal() {
  MOZ_ASSERT(mStillWalking);
  // Walk the prototype and build the delegate content model. The
  // walk is performed in a top-down, left-to-right fashion. That
  // is, a parent is built before any of its children; a node is
  // only built after all of its siblings to the left are fully
  // constructed.
  //
  // It is interruptable so that transcluded documents (e.g.,
  // <html:script src="..." />) can be properly re-loaded if the
  // cached copy of the document becomes stale.
  nsresult rv;
  nsCOMPtr<nsIURI> docURI =
      mCurrentPrototype ? mCurrentPrototype->GetURI() : nullptr;

  while (1) {
    // Begin (or resume) walking the current prototype.

    while (mContextStack.Depth() > 0) {
      // Look at the top of the stack to determine what we're
      // currently working on.
      // This will always be a node already constructed and
      // inserted to the actual document.
      nsXULPrototypeElement* proto;
      nsCOMPtr<nsIContent> element;
      nsCOMPtr<nsIContent> nodeToPushTo;
      int32_t indx;  // all children of proto before indx (not
                     // inclusive) have already been constructed
      rv = mContextStack.Peek(&proto, getter_AddRefs(element), &indx);
      if (NS_FAILED(rv)) return rv;

      if (indx >= (int32_t)proto->mChildren.Length()) {
        if (element) {
          // We've processed all of the prototype's children.
          CloseElement(element->AsElement());
          if (element->NodeInfo()->Equals(nsGkAtoms::style,
                                          kNameSpaceID_XHTML) ||
              element->NodeInfo()->Equals(nsGkAtoms::style, kNameSpaceID_SVG)) {
            // XXX sucks that we have to do this -
            // see bug 370111
            nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
                do_QueryInterface(element);
            NS_ASSERTION(ssle,
                         "<html:style> doesn't implement "
                         "nsIStyleSheetLinkingElement?");
            Unused << ssle->UpdateStyleSheet(nullptr);
          }
        }
        // Now pop the context stack back up to the parent
        // element and continue the prototype walk.
        mContextStack.Pop();
        continue;
      }

      nodeToPushTo = element;
      // For template elements append the content to the template's document
      // fragment.
      if (element->IsHTMLElement(nsGkAtoms::_template)) {
        HTMLTemplateElement* templateElement =
            static_cast<HTMLTemplateElement*>(element.get());
        nodeToPushTo = templateElement->Content();
      }

      // Grab the next child, and advance the current context stack
      // to the next sibling to our right.
      nsXULPrototypeNode* childproto = proto->mChildren[indx];
      mContextStack.SetTopIndex(++indx);

      NS_ASSERTION(element, "no element on context stack");

      switch (childproto->mType) {
        case nsXULPrototypeNode::eType_Element: {
          // An 'element', which may contain more content.
          nsXULPrototypeElement* protoele =
              static_cast<nsXULPrototypeElement*>(childproto);

          RefPtr<Element> child;

          rv = CreateElementFromPrototype(protoele, getter_AddRefs(child),
                                          false);
          if (NS_FAILED(rv)) return rv;

          // ...and append it to the content model.
          rv = nodeToPushTo->AppendChildTo(child, false);
          if (NS_FAILED(rv)) return rv;

          if (nsIContent::RequiresDoneCreatingElement(
                  protoele->mNodeInfo->NamespaceID(),
                  protoele->mNodeInfo->NameAtom())) {
            child->DoneCreatingElement();
          }

          // If it has children, push the element onto the context
          // stack and begin to process them.
          if (protoele->mChildren.Length() > 0) {
            rv = mContextStack.Push(protoele, child);
            if (NS_FAILED(rv)) return rv;
          } else {
            // If there are no children, close the element immediately.
            CloseElement(child);
          }
        } break;

        case nsXULPrototypeNode::eType_Script: {
          // A script reference. Execute the script immediately;
          // this may have side effects in the content model.
          nsXULPrototypeScript* scriptproto =
              static_cast<nsXULPrototypeScript*>(childproto);

          if (scriptproto->mSrcURI) {
            // A transcluded script reference; this may
            // "block" our prototype walk if the script isn't
            // cached, or the cached copy of the script is
            // stale and must be reloaded.
            bool blocked;
            rv = LoadScript(scriptproto, &blocked);
            // If the script cannot be loaded, just keep going!

            if (NS_SUCCEEDED(rv) && blocked) return NS_OK;
          } else if (scriptproto->HasScriptObject()) {
            // An inline script
            rv = ExecuteScript(scriptproto);
            if (NS_FAILED(rv)) return rv;
          }
        } break;

        case nsXULPrototypeNode::eType_Text: {
          // A simple text node.
          RefPtr<nsTextNode> text =
              new (mNodeInfoManager) nsTextNode(mNodeInfoManager);

          nsXULPrototypeText* textproto =
              static_cast<nsXULPrototypeText*>(childproto);
          text->SetText(textproto->mValue, false);

          rv = nodeToPushTo->AppendChildTo(text, false);
          NS_ENSURE_SUCCESS(rv, rv);
        } break;

        case nsXULPrototypeNode::eType_PI: {
          nsXULPrototypePI* piProto =
              static_cast<nsXULPrototypePI*>(childproto);

          // <?xml-stylesheet?> doesn't have an effect
          // outside the prolog, like it used to. Issue a warning.

          if (piProto->mTarget.EqualsLiteral("xml-stylesheet")) {
            AutoTArray<nsString, 1> params = {piProto->mTarget};

            nsContentUtils::ReportToConsole(
                nsIScriptError::warningFlag, NS_LITERAL_CSTRING("XUL Document"),
                nullptr, nsContentUtils::eXUL_PROPERTIES, "PINotInProlog",
                params, docURI);
          }

          nsIContent* parent = element.get();

          if (parent) {
            // an inline script could have removed the root element
            rv = CreateAndInsertPI(piProto, parent, nullptr);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        } break;

        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected nsXULPrototypeNode::Type");
      }
    }

    // Once we get here, the context stack will have been
    // depleted. That means that the entire prototype has been
    // walked and content has been constructed.
    break;
  }

  mStillWalking = false;
  return MaybeDoneWalking();
}

void PrototypeDocumentContentSink::InitialDocumentTranslationCompleted() {
  MaybeDoneWalking();
}

nsresult PrototypeDocumentContentSink::MaybeDoneWalking() {
  if (mPendingSheets > 0 || mStillWalking) {
    return NS_OK;
  }

  if (mDocument->HasPendingInitialTranslation()) {
    mDocument->TriggerInitialDocumentTranslation();
    return NS_OK;
  }

  return DoneWalking();
}

nsresult PrototypeDocumentContentSink::DoneWalking() {
  MOZ_ASSERT(mPendingSheets == 0, "there are sheets to be loaded");
  MOZ_ASSERT(!mStillWalking, "walk not done");
  MOZ_ASSERT(!mDocument->HasPendingInitialTranslation(), "translation pending");

  if (mDocument) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() == Document::READYSTATE_LOADING,
               "Bad readyState");
    mDocument->SetReadyStateInternal(Document::READYSTATE_INTERACTIVE);
    mDocument->NotifyPossibleTitleChange(false);

    nsContentUtils::DispatchTrustedEvent(
        mDocument, ToSupports(mDocument),
        NS_LITERAL_STRING("MozBeforeInitialXULLayout"), CanBubble::eYes,
        Cancelable::eNo);
  }

  if (mScriptLoader) {
    mScriptLoader->ParsingComplete(false);
    mScriptLoader->DeferCheckpointReached();
  }

  StartLayout();

  if (IsChromeURI(mDocumentURI) &&
      nsXULPrototypeCache::GetInstance()->IsEnabled()) {
    bool isCachedOnDisk;
    nsXULPrototypeCache::GetInstance()->HasData(mDocumentURI, &isCachedOnDisk);
    if (!isCachedOnDisk) {
      nsXULPrototypeCache::GetInstance()->WritePrototype(mCurrentPrototype);
    }
  }

  mDocument->SetDelayFrameLoaderInitialization(false);
  mDocument->MaybeInitializeFinalizeFrameLoaders();

  // If the document we are loading has a reference or it is a
  // frameset document, disable the scroll bars on the views.

  mDocument->SetScrollToRef(mDocument->GetDocumentURI());

  mDocument->EndLoad();

  return NS_OK;
}

void PrototypeDocumentContentSink::StartLayout() {
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
      "PrototypeDocumentContentSink::StartLayout", LAYOUT,
      mDocumentURI->GetSpecOrDefault());
  mDocument->SetMayStartLayout(true);
  RefPtr<PresShell> presShell = mDocument->GetPresShell();
  if (presShell && !presShell->DidInitialize()) {
    nsresult rv = presShell->Initialize();
    if (NS_FAILED(rv)) {
      return;
    }
  }
}

NS_IMETHODIMP
PrototypeDocumentContentSink::StyleSheetLoaded(StyleSheet* aSheet,
                                               bool aWasDeferred,
                                               nsresult aStatus) {
  if (!aWasDeferred) {
    // Don't care about when alternate sheets finish loading
    MOZ_ASSERT(mPendingSheets > 0, "Unexpected StyleSheetLoaded notification");

    --mPendingSheets;

    return MaybeDoneWalking();
  }

  return NS_OK;
}

nsresult PrototypeDocumentContentSink::LoadScript(
    nsXULPrototypeScript* aScriptProto, bool* aBlock) {
  // Load a transcluded script
  nsresult rv;

  bool isChromeDoc = IsChromeURI(mDocumentURI);

  if (isChromeDoc && aScriptProto->HasScriptObject()) {
    rv = ExecuteScript(aScriptProto);

    // Ignore return value from execution, and don't block
    *aBlock = false;
    return NS_OK;
  }

  // Try the XUL script cache, in case two XUL documents source the same
  // .js file (e.g., strres.js from navigator.xul and utilityOverlay.xul).
  // XXXbe the cache relies on aScriptProto's GC root!
  bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();

  if (isChromeDoc && useXULCache) {
    JSScript* newScriptObject =
        nsXULPrototypeCache::GetInstance()->GetScript(aScriptProto->mSrcURI);
    if (newScriptObject) {
      // The script language for a proto must remain constant - we
      // can't just change it for this unexpected language.
      aScriptProto->Set(newScriptObject);
    }

    if (aScriptProto->HasScriptObject()) {
      rv = ExecuteScript(aScriptProto);

      // Ignore return value from execution, and don't block
      *aBlock = false;
      return NS_OK;
    }
  }

  // Release script objects from FastLoad since we decided against using them
  aScriptProto->UnlinkJSObjects();

  // Set the current script prototype so that OnStreamComplete can report
  // the right file if there are errors in the script.
  NS_ASSERTION(!mCurrentScriptProto,
               "still loading a script when starting another load?");
  mCurrentScriptProto = aScriptProto;

  if (isChromeDoc && aScriptProto->mSrcLoading) {
    // Another document load has started, which is still in progress.
    // Remember to ResumeWalk this document when the load completes.
    mNextSrcLoadWaiter = aScriptProto->mSrcLoadWaiters;
    aScriptProto->mSrcLoadWaiters = this;
    NS_ADDREF_THIS();
  } else {
    nsCOMPtr<nsILoadGroup> group =
        mDocument
            ->GetDocumentLoadGroup();  // found in
                                       // mozilla::dom::Document::SetScriptGlobalObject

    // Note: the loader will keep itself alive while it's loading.
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), aScriptProto->mSrcURI,
                            this,       // aObserver
                            mDocument,  // aRequestingContext
                            nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
                            nsIContentPolicy::TYPE_INTERNAL_SCRIPT, group);

    if (NS_FAILED(rv)) {
      mCurrentScriptProto = nullptr;
      return rv;
    }

    aScriptProto->mSrcLoading = true;
  }

  // Block until OnStreamComplete resumes us.
  *aBlock = true;
  return NS_OK;
}

NS_IMETHODIMP
PrototypeDocumentContentSink::OnStreamComplete(nsIStreamLoader* aLoader,
                                               nsISupports* context,
                                               nsresult aStatus,
                                               uint32_t stringLen,
                                               const uint8_t* string) {
  nsCOMPtr<nsIRequest> request;
  aLoader->GetRequest(getter_AddRefs(request));
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);

#ifdef DEBUG
  // print a load error on bad status
  if (NS_FAILED(aStatus)) {
    if (channel) {
      nsCOMPtr<nsIURI> uri;
      channel->GetURI(getter_AddRefs(uri));
      if (uri) {
        printf("Failed to load %s\n", uri->GetSpecOrDefault().get());
      }
    }
  }
#endif

  // This is the completion routine that will be called when a
  // transcluded script completes. Compile and execute the script
  // if the load was successful, then continue building content
  // from the prototype.
  nsresult rv = aStatus;

  NS_ASSERTION(mCurrentScriptProto && mCurrentScriptProto->mSrcLoading,
               "script source not loading on unichar stream complete?");
  if (!mCurrentScriptProto) {
    // XXX Wallpaper for bug 270042
    return NS_OK;
  }

  if (NS_SUCCEEDED(aStatus)) {
    // If the including document is a FastLoad document, and we're
    // compiling an out-of-line script (one with src=...), then we must
    // be writing a new FastLoad file.  If we were reading this script
    // from the FastLoad file, XULContentSinkImpl::OpenScript (over in
    // nsXULContentSink.cpp) would have already deserialized a non-null
    // script->mScriptObject, causing control flow at the top of LoadScript
    // not to reach here.
    nsCOMPtr<nsIURI> uri = mCurrentScriptProto->mSrcURI;

    // XXX should also check nsIHttpChannel::requestSucceeded

    MOZ_ASSERT(!mOffThreadCompiling && (mOffThreadCompileStringLength == 0 &&
                                        !mOffThreadCompileStringBuf),
               "PrototypeDocument can't load multiple scripts at once");

    rv = ScriptLoader::ConvertToUTF16(channel, string, stringLen, EmptyString(),
                                      mDocument, mOffThreadCompileStringBuf,
                                      mOffThreadCompileStringLength);
    if (NS_SUCCEEDED(rv)) {
      // Pass ownership of the buffer, carefully emptying the existing
      // fields in the process.  Note that the |Compile| function called
      // below always takes ownership of the buffer.
      char16_t* units = nullptr;
      size_t unitsLength = 0;

      std::swap(units, mOffThreadCompileStringBuf);
      std::swap(unitsLength, mOffThreadCompileStringLength);

      rv = mCurrentScriptProto->Compile(units, unitsLength,
                                        JS::SourceOwnership::TakeOwnership, uri,
                                        1, mDocument, this);
      if (NS_SUCCEEDED(rv) && !mCurrentScriptProto->HasScriptObject()) {
        mOffThreadCompiling = true;
        mDocument->BlockOnload();
        return NS_OK;
      }
    }
  }

  return OnScriptCompileComplete(mCurrentScriptProto->GetScriptObject(), rv);
}

NS_IMETHODIMP
PrototypeDocumentContentSink::OnScriptCompileComplete(JSScript* aScript,
                                                      nsresult aStatus) {
  // When compiling off thread the script will not have been attached to the
  // script proto yet.
  if (aScript && !mCurrentScriptProto->HasScriptObject())
    mCurrentScriptProto->Set(aScript);

  // Allow load events to be fired once off thread compilation finishes.
  if (mOffThreadCompiling) {
    mOffThreadCompiling = false;
    mDocument->UnblockOnload(false);
  }

  // After compilation finishes the script's characters are no longer needed.
  if (mOffThreadCompileStringBuf) {
    js_free(mOffThreadCompileStringBuf);
    mOffThreadCompileStringBuf = nullptr;
    mOffThreadCompileStringLength = 0;
  }

  // Clear mCurrentScriptProto now, but save it first for use below in
  // the execute code, and in the while loop that resumes walks of other
  // documents that raced to load this script.
  nsXULPrototypeScript* scriptProto = mCurrentScriptProto;
  mCurrentScriptProto = nullptr;

  // Clear the prototype's loading flag before executing the script or
  // resuming document walks, in case any of those control flows starts a
  // new script load.
  scriptProto->mSrcLoading = false;

  nsresult rv = aStatus;
  if (NS_SUCCEEDED(rv)) {
    rv = ExecuteScript(scriptProto);

    // If the XUL cache is enabled, save the script object there in
    // case different XUL documents source the same script.
    //
    // But don't save the script in the cache unless the master XUL
    // document URL is a chrome: URL.  It is valid for a URL such as
    // about:config to translate into a master document URL, whose
    // prototype document nodes -- including prototype scripts that
    // hold GC roots protecting their mJSObject pointers -- are not
    // cached in the XUL prototype cache.  See StartDocumentLoad,
    // the fillXULCache logic.
    //
    // A document such as about:config is free to load a script via
    // a URL such as chrome://global/content/config.js, and we must
    // not cache that script object without a prototype cache entry
    // containing a companion nsXULPrototypeScript node that owns a
    // GC root protecting the script object.  Otherwise, the script
    // cache entry will dangle once the uncached prototype document
    // is released when its owning document is unloaded.
    //
    // (See http://bugzilla.mozilla.org/show_bug.cgi?id=98207 for
    // the true crime story.)
    bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();

    if (useXULCache && IsChromeURI(mDocumentURI) &&
        scriptProto->HasScriptObject()) {
      JS::Rooted<JSScript*> script(RootingCx(), scriptProto->GetScriptObject());
      nsXULPrototypeCache::GetInstance()->PutScript(scriptProto->mSrcURI,
                                                    script);
    }
    // ignore any evaluation errors
  }

  rv = ResumeWalk();

  // Load a pointer to the prototype-script's list of documents who
  // raced to load the same script
  PrototypeDocumentContentSink** docp = &scriptProto->mSrcLoadWaiters;

  // Resume walking other documents that waited for this one's load, first
  // executing the script we just compiled, in each doc's script context
  PrototypeDocumentContentSink* doc;
  while ((doc = *docp) != nullptr) {
    NS_ASSERTION(doc->mCurrentScriptProto == scriptProto,
                 "waiting for wrong script to load?");
    doc->mCurrentScriptProto = nullptr;

    // Unlink doc from scriptProto's list before executing and resuming
    *docp = doc->mNextSrcLoadWaiter;
    doc->mNextSrcLoadWaiter = nullptr;

    if (aStatus == NS_BINDING_ABORTED && !scriptProto->HasScriptObject()) {
      // If the previous doc load was aborted, we want to try loading
      // again for the next doc. Otherwise, one abort would lead to all
      // subsequent waiting docs to abort as well.
      bool block = false;
      doc->LoadScript(scriptProto, &block);
      NS_RELEASE(doc);
      return rv;
    }

    // Execute only if we loaded and compiled successfully, then resume
    if (NS_SUCCEEDED(aStatus) && scriptProto->HasScriptObject()) {
      doc->ExecuteScript(scriptProto);
    }
    doc->ResumeWalk();
    NS_RELEASE(doc);
  }

  return rv;
}

nsresult PrototypeDocumentContentSink::ExecuteScript(
    nsXULPrototypeScript* aScript) {
  MOZ_ASSERT(aScript != nullptr, "null ptr");
  NS_ENSURE_TRUE(aScript, NS_ERROR_NULL_POINTER);

  nsIScriptGlobalObject* scriptGlobalObject;
  bool aHasHadScriptHandlingObject;
  scriptGlobalObject =
      mDocument->GetScriptHandlingObject(aHasHadScriptHandlingObject);

  NS_ENSURE_TRUE(scriptGlobalObject, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  rv = scriptGlobalObject->EnsureScriptEnvironment();
  NS_ENSURE_SUCCESS(rv, rv);

  // Execute the precompiled script with the given version
  nsAutoMicroTask mt;

  // We're about to run script via JS::CloneAndExecuteScript, so we need an
  // AutoEntryScript. This is Gecko specific and not in any spec.
  AutoEntryScript aes(scriptGlobalObject, "precompiled XUL <script> element");
  JSContext* cx = aes.cx();

  JS::Rooted<JSScript*> scriptObject(cx, aScript->GetScriptObject());
  NS_ENSURE_TRUE(scriptObject, NS_ERROR_UNEXPECTED);

  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  NS_ENSURE_TRUE(xpc::Scriptability::Get(global).Allowed(), NS_OK);

  // The script is in the compilation scope. Clone it into the target scope
  // and execute it. On failure, ~AutoScriptEntry will handle exceptions, so
  // there is no need to manually check the return value.
  JS::RootedValue rval(cx);
  JS::CloneAndExecuteScript(cx, scriptObject, &rval);

  return NS_OK;
}

nsresult PrototypeDocumentContentSink::CreateElementFromPrototype(
    nsXULPrototypeElement* aPrototype, Element** aResult, bool aIsRoot) {
  // Create a content model element from a prototype element.
  MOZ_ASSERT(aPrototype != nullptr, "null ptr");
  if (!aPrototype) return NS_ERROR_NULL_POINTER;

  *aResult = nullptr;
  nsresult rv = NS_OK;

  if (MOZ_LOG_TEST(gLog, LogLevel::Debug)) {
    MOZ_LOG(
        gLog, LogLevel::Debug,
        ("prototype: creating <%s> from prototype",
         NS_ConvertUTF16toUTF8(aPrototype->mNodeInfo->QualifiedName()).get()));
  }

  RefPtr<Element> result;

  if (aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
    // If it's a XUL element, it'll be lightweight until somebody
    // monkeys with it.
    rv = nsXULElement::CreateFromPrototype(aPrototype, mDocument, true, aIsRoot,
                                           getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;
  } else {
    // If it's not a XUL element, it's gonna be heavyweight no matter
    // what. So we need to copy everything out of the prototype
    // into the element.  Get a nodeinfo from our nodeinfo manager
    // for this node.
    RefPtr<mozilla::dom::NodeInfo> newNodeInfo;
    newNodeInfo = mNodeInfoManager->GetNodeInfo(
        aPrototype->mNodeInfo->NameAtom(),
        aPrototype->mNodeInfo->GetPrefixAtom(),
        aPrototype->mNodeInfo->NamespaceID(), nsINode::ELEMENT_NODE);
    if (!newNodeInfo) return NS_ERROR_OUT_OF_MEMORY;
    RefPtr<mozilla::dom::NodeInfo> xtfNi = newNodeInfo;
    if (aPrototype->mIsAtom &&
        newNodeInfo->NamespaceID() == kNameSpaceID_XHTML) {
      rv = NS_NewHTMLElement(getter_AddRefs(result), newNodeInfo.forget(),
                             NOT_FROM_PARSER, aPrototype->mIsAtom);
    } else {
      rv = NS_NewElement(getter_AddRefs(result), newNodeInfo.forget(),
                         NOT_FROM_PARSER);
    }
    if (NS_FAILED(rv)) return rv;

    rv = AddAttributes(aPrototype, result);
    if (NS_FAILED(rv)) return rv;

    if (xtfNi->Equals(nsGkAtoms::script, kNameSpaceID_XHTML) ||
        xtfNi->Equals(nsGkAtoms::script, kNameSpaceID_SVG)) {
      nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(result);
      MOZ_ASSERT(sele, "Node didn't QI to script.");
      // Script loading is handled by the this content sink, so prevent the
      // script from loading when it is bound to the document.
      sele->PreventExecution();
    }
  }

  if (result->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nid)) {
    mDocument->mL10nProtoElements.Put(result, RefPtr{aPrototype});
    result->SetElementCreatedFromPrototypeAndHasUnmodifiedL10n();
  }
  result.forget(aResult);

  return NS_OK;
}

nsresult PrototypeDocumentContentSink::AddAttributes(
    nsXULPrototypeElement* aPrototype, Element* aElement) {
  nsresult rv;

  for (size_t i = 0; i < aPrototype->mAttributes.Length(); ++i) {
    nsXULPrototypeAttribute* protoattr = &(aPrototype->mAttributes[i]);
    nsAutoString valueStr;
    protoattr->mValue.ToString(valueStr);

    rv = aElement->SetAttr(protoattr->mName.NamespaceID(),
                           protoattr->mName.LocalName(),
                           protoattr->mName.GetPrefix(), valueStr, false);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

}  // namespace mozilla::dom
