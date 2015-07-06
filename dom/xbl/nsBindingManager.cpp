/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBindingManager.h"

#include "nsCOMPtr.h"
#include "nsXBLService.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsIPresShell.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "mozilla/dom/XMLDocument.h"
#include "nsIStreamListener.h"
#include "ChildIterator.h"
#include "nsITimer.h"

#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLDocumentInfo.h"
#include "mozilla/dom/XBLChildrenElement.h"

#include "nsIStyleRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include "nsIWeakReference.h"

#include "nsWrapperCacheInlines.h"
#include "nsIXPConnect.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIScriptGlobalObject.h"
#include "nsTHashtable.h"

#include "nsIScriptContext.h"
#include "xpcpublic.h"
#include "jswrapper.h"

#include "nsThreadUtils.h"
#include "mozilla/dom/NodeListBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/unused.h"

using namespace mozilla;
using namespace mozilla::dom;

// Implement our nsISupports methods

NS_IMPL_CYCLE_COLLECTION_CLASS(nsBindingManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsBindingManager)
  tmp->mDestroyed = true;

  if (tmp->mBoundContentSet)
    tmp->mBoundContentSet->Clear();

  if (tmp->mDocumentTable)
    tmp->mDocumentTable->Clear();

  if (tmp->mLoadingDocTable)
    tmp->mLoadingDocTable->Clear();

  if (tmp->mWrapperTable) {
    tmp->mWrapperTable->Clear();
    tmp->mWrapperTable = nullptr;
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAttachedStack)

  if (tmp->mProcessAttachedQueueEvent) {
    tmp->mProcessAttachedQueueEvent->Revoke();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


static PLDHashOperator
DocumentInfoHashtableTraverser(nsIURI* key,
                               nsXBLDocumentInfo* di,
                               void* userArg)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(userArg);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mDocumentTable value");
  cb->NoteXPCOMChild(di);
  return PL_DHASH_NEXT;
}

static PLDHashOperator
LoadingDocHashtableTraverser(nsIURI* key,
                             nsIStreamListener* sl,
                             void* userArg)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(userArg);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mLoadingDocTable value");
  cb->NoteXPCOMChild(sl);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsBindingManager)
  // The hashes keyed on nsIContent are traversed from the nsIContent itself.
  if (tmp->mDocumentTable)
      tmp->mDocumentTable->EnumerateRead(&DocumentInfoHashtableTraverser, &cb);
  if (tmp->mLoadingDocTable)
      tmp->mLoadingDocTable->EnumerateRead(&LoadingDocHashtableTraverser, &cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAttachedStack)
  // No need to traverse mProcessAttachedQueueEvent, since it'll just
  // fire at some point or become revoke and drop its ref to us.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsBindingManager)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsBindingManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsBindingManager)

// Constructors/Destructors
nsBindingManager::nsBindingManager(nsIDocument* aDocument)
  : mProcessingAttachedStack(false),
    mDestroyed(false),
    mAttachedStackSizeOnOutermost(0),
    mDocument(aDocument)
{
}

nsBindingManager::~nsBindingManager(void)
{
  mDestroyed = true;
}

nsXBLBinding*
nsBindingManager::GetBindingWithContent(nsIContent* aContent)
{
  nsXBLBinding* binding = aContent ? aContent->GetXBLBinding() : nullptr;
  return binding ? binding->GetBindingWithContent() : nullptr;
}

void
nsBindingManager::AddBoundContent(nsIContent* aContent)
{
  if (!mBoundContentSet) {
    mBoundContentSet = new nsTHashtable<nsRefPtrHashKey<nsIContent> >;
  }
  mBoundContentSet->PutEntry(aContent);
}

void
nsBindingManager::RemoveBoundContent(nsIContent* aContent)
{
  if (mBoundContentSet) {
    mBoundContentSet->RemoveEntry(aContent);
  }

  // The death of the bindings means the death of the JS wrapper.
  SetWrappedJS(aContent, nullptr);
}

nsIXPConnectWrappedJS*
nsBindingManager::GetWrappedJS(nsIContent* aContent)
{
  if (!mWrapperTable) {
    return nullptr;
  }

  if (!aContent || !aContent->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    return nullptr;
  }

  return mWrapperTable->GetWeak(aContent);
}

nsresult
nsBindingManager::SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aWrappedJS)
{
  if (mDestroyed) {
    return NS_OK;
  }

  if (aWrappedJS) {
    // lazily create the table, but only when adding elements
    if (!mWrapperTable) {
      mWrapperTable = new WrapperHashtable();
    }
    aContent->SetFlags(NODE_MAY_BE_IN_BINDING_MNGR);

    NS_ASSERTION(aContent, "key must be non-null");
    if (!aContent) return NS_ERROR_INVALID_ARG;

    mWrapperTable->Put(aContent, aWrappedJS);

    return NS_OK;
  }

  // no value, so remove the key from the table
  if (mWrapperTable) {
    mWrapperTable->Remove(aContent);
  }

  return NS_OK;
}

void
nsBindingManager::RemovedFromDocumentInternal(nsIContent* aContent,
                                              nsIDocument* aOldDocument)
{
  NS_PRECONDITION(aOldDocument != nullptr, "no old document");

  nsRefPtr<nsXBLBinding> binding = aContent->GetXBLBinding();
  if (binding) {
    // The binding manager may have been destroyed before a runnable
    // has had a chance to reach this point. If so, we bail out on calling
    // BindingDetached (which may invoke a XBL destructor) and
    // ChangeDocument, but we still want to clear out the binding
    // and insertion parent that may hold references.
    if (!mDestroyed) {
      binding->PrototypeBinding()->BindingDetached(binding->GetBoundElement());
      binding->ChangeDocument(aOldDocument, nullptr);
    }

    aContent->SetXBLBinding(nullptr, this);
  }

  // Clear out insertion parent and content lists.
  aContent->SetXBLInsertionParent(nullptr);
}

nsIAtom*
nsBindingManager::ResolveTag(nsIContent* aContent, int32_t* aNameSpaceID)
{
  nsXBLBinding *binding = aContent->GetXBLBinding();

  if (binding) {
    nsIAtom* base = binding->GetBaseTag(aNameSpaceID);

    if (base) {
      return base;
    }
  }

  *aNameSpaceID = aContent->GetNameSpaceID();
  return aContent->NodeInfo()->NameAtom();
}

nsresult
nsBindingManager::GetAnonymousNodesFor(nsIContent* aContent,
                                       nsIDOMNodeList** aResult)
{
  NS_IF_ADDREF(*aResult = GetAnonymousNodesFor(aContent));
  return NS_OK;
}

nsINodeList*
nsBindingManager::GetAnonymousNodesFor(nsIContent* aContent)
{
  nsXBLBinding* binding = GetBindingWithContent(aContent);
  return binding ? binding->GetAnonymousNodeList() : nullptr;
}

nsresult
nsBindingManager::ClearBinding(nsIContent* aContent)
{
  // Hold a ref to the binding so it won't die when we remove it from our table
  nsRefPtr<nsXBLBinding> binding =
    aContent ? aContent->GetXBLBinding() : nullptr;

  if (!binding) {
    return NS_OK;
  }

  // For now we can only handle removing a binding if it's the only one
  NS_ENSURE_FALSE(binding->GetBaseBinding(), NS_ERROR_FAILURE);

  // Hold strong ref in case removing the binding tries to close the
  // window or something.
  // XXXbz should that be ownerdoc?  Wouldn't we need a ref to the
  // currentdoc too?  What's the one that should be passed to
  // ChangeDocument?
  nsCOMPtr<nsIDocument> doc = aContent->OwnerDoc();

  // Finally remove the binding...
  // XXXbz this doesn't remove the implementation!  Should fix!  Until
  // then we need the explicit UnhookEventHandlers here.
  binding->UnhookEventHandlers();
  binding->ChangeDocument(doc, nullptr);
  aContent->SetXBLBinding(nullptr, this);
  binding->MarkForDeath();

  // ...and recreate its frames. We need to do this since the frames may have
  // been removed and style may have changed due to the removal of the
  // anonymous children.
  // XXXbz this should be using the current doc (if any), not the owner doc.
  nsIPresShell *presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  return presShell->RecreateFramesFor(aContent);;
}

nsresult
nsBindingManager::LoadBindingDocument(nsIDocument* aBoundDoc,
                                      nsIURI* aURL,
                                      nsIPrincipal* aOriginPrincipal)
{
  NS_PRECONDITION(aURL, "Must have a URI to load!");

  // First we need to load our binding.
  nsXBLService* xblService = nsXBLService::GetInstance();
  if (!xblService)
    return NS_ERROR_FAILURE;

  // Load the binding doc.
  nsRefPtr<nsXBLDocumentInfo> info;
  xblService->LoadBindingDocumentInfo(nullptr, aBoundDoc, aURL,
                                      aOriginPrincipal, true,
                                      getter_AddRefs(info));
  if (!info)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

void
nsBindingManager::RemoveFromAttachedQueue(nsXBLBinding* aBinding)
{
  // Don't remove items here as that could mess up an executing
  // ProcessAttachedQueue. Instead, null the entry in the queue.
  size_t index = mAttachedStack.IndexOf(aBinding);
  if (index != mAttachedStack.NoIndex) {
    mAttachedStack[index] = nullptr;
  }
}

nsresult
nsBindingManager::AddToAttachedQueue(nsXBLBinding* aBinding)
{
  mAttachedStack.AppendElement(aBinding);

  // If we're in the middle of processing our queue already, don't
  // bother posting the event.
  if (!mProcessingAttachedStack && !mProcessAttachedQueueEvent) {
    PostProcessAttachedQueueEvent();
  }

  // Make sure that flushes will flush out the new items as needed.
  mDocument->SetNeedStyleFlush();

  return NS_OK;

}

void
nsBindingManager::PostProcessAttachedQueueEvent()
{
  mProcessAttachedQueueEvent =
    NS_NewRunnableMethod(this, &nsBindingManager::DoProcessAttachedQueue);
  nsresult rv = NS_DispatchToCurrentThread(mProcessAttachedQueueEvent);
  if (NS_SUCCEEDED(rv) && mDocument) {
    mDocument->BlockOnload();
  }
}

// static
void
nsBindingManager::PostPAQEventCallback(nsITimer* aTimer, void* aClosure)
{
  nsRefPtr<nsBindingManager> mgr = 
    already_AddRefed<nsBindingManager>(static_cast<nsBindingManager*>(aClosure));
  mgr->PostProcessAttachedQueueEvent();
  NS_RELEASE(aTimer);
}

void
nsBindingManager::DoProcessAttachedQueue()
{
  if (!mProcessingAttachedStack) {
    ProcessAttachedQueue();

    NS_ASSERTION(mAttachedStack.Length() == 0,
               "Shouldn't have pending bindings!");

    mProcessAttachedQueueEvent = nullptr;
  } else {
    // Someone's doing event processing from inside a constructor.
    // They're evil, but we'll fight back!  Just poll on them being
    // done and repost the attached queue event.
    //
    // But don't poll in a tight loop -- otherwise we keep the Gecko
    // event loop non-empty and trigger bug 1021240 on OS X.
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (timer) {
      rv = timer->InitWithFuncCallback(PostPAQEventCallback, this,
                                       100, nsITimer::TYPE_ONE_SHOT);
    }
    if (NS_SUCCEEDED(rv)) {
      NS_ADDREF_THIS();
      // We drop our reference to the timer here, since the timer callback is
      // responsible for releasing the object.
      unused << timer.forget().take();
    }
  }

  // No matter what, unblock onload for the event that's fired.
  if (mDocument) {
    // Hold a strong reference while calling UnblockOnload since that might
    // run script.
    nsCOMPtr<nsIDocument> doc = mDocument;
    doc->UnblockOnload(true);
  }
}

void
nsBindingManager::ProcessAttachedQueue(uint32_t aSkipSize)
{
  if (mProcessingAttachedStack || mAttachedStack.Length() <= aSkipSize)
    return;

  mProcessingAttachedStack = true;

  // Excute constructors. Do this from high index to low
  while (mAttachedStack.Length() > aSkipSize) {
    uint32_t lastItem = mAttachedStack.Length() - 1;
    nsRefPtr<nsXBLBinding> binding = mAttachedStack.ElementAt(lastItem);
    mAttachedStack.RemoveElementAt(lastItem);
    if (binding) {
      binding->ExecuteAttachedHandler();
    }
  }

  // If NodeWillBeDestroyed has run we don't want to clobber
  // mProcessingAttachedStack set there.
  if (mDocument) {
    mProcessingAttachedStack = false;
  }

  NS_ASSERTION(mAttachedStack.Length() == aSkipSize, "How did we get here?");

  mAttachedStack.Compact();
}

// Keep bindings and bound elements alive while executing detached handlers.
struct BindingTableReadClosure
{
  nsCOMArray<nsIContent> mBoundElements;
  nsBindingList          mBindings;
};

static PLDHashOperator
AccumulateBindingsToDetach(nsRefPtrHashKey<nsIContent> *aKey,
                           void* aClosure)
{
  nsXBLBinding *binding = aKey->GetKey()->GetXBLBinding();
  BindingTableReadClosure* closure =
    static_cast<BindingTableReadClosure*>(aClosure);
  if (binding && closure->mBindings.AppendElement(binding)) {
    if (!closure->mBoundElements.AppendObject(binding->GetBoundElement())) {
      closure->mBindings.RemoveElementAt(closure->mBindings.Length() - 1);
    }
  }
  return PL_DHASH_NEXT;
}

void
nsBindingManager::ExecuteDetachedHandlers()
{
  // Walk our hashtable of bindings.
  if (mBoundContentSet) {
    BindingTableReadClosure closure;
    mBoundContentSet->EnumerateEntries(AccumulateBindingsToDetach, &closure);
    uint32_t i, count = closure.mBindings.Length();
    for (i = 0; i < count; ++i) {
      closure.mBindings[i]->ExecuteDetachedHandler();
    }
  }
}

nsresult
nsBindingManager::PutXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo)
{
  NS_PRECONDITION(aDocumentInfo, "Must have a non-null documentinfo!");

  if (!mDocumentTable) {
    mDocumentTable = new nsRefPtrHashtable<nsURIHashKey,nsXBLDocumentInfo>();
  }

  mDocumentTable->Put(aDocumentInfo->DocumentURI(), aDocumentInfo);

  return NS_OK;
}

void
nsBindingManager::RemoveXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo)
{
  if (mDocumentTable) {
    mDocumentTable->Remove(aDocumentInfo->DocumentURI());
  }
}

nsXBLDocumentInfo*
nsBindingManager::GetXBLDocumentInfo(nsIURI* aURL)
{
  if (!mDocumentTable)
    return nullptr;

  return mDocumentTable->GetWeak(aURL);
}

nsresult
nsBindingManager::PutLoadingDocListener(nsIURI* aURL, nsIStreamListener* aListener)
{
  NS_PRECONDITION(aListener, "Must have a non-null listener!");

  if (!mLoadingDocTable) {
    mLoadingDocTable =
      new nsInterfaceHashtable<nsURIHashKey,nsIStreamListener>();
  }
  mLoadingDocTable->Put(aURL, aListener);

  return NS_OK;
}

nsIStreamListener*
nsBindingManager::GetLoadingDocListener(nsIURI* aURL)
{
  if (!mLoadingDocTable)
    return nullptr;

  return mLoadingDocTable->GetWeak(aURL);
}

void
nsBindingManager::RemoveLoadingDocListener(nsIURI* aURL)
{
  if (mLoadingDocTable) {
    mLoadingDocTable->Remove(aURL);
  }
}

static PLDHashOperator
MarkForDeath(nsRefPtrHashKey<nsIContent> *aKey, void* aClosure)
{
  nsXBLBinding *binding = aKey->GetKey()->GetXBLBinding();

  if (binding->MarkedForDeath())
    return PL_DHASH_NEXT; // Already marked for death.

  nsAutoCString path;
  binding->PrototypeBinding()->DocURI()->GetPath(path);

  if (!strncmp(path.get(), "/skin", 5))
    binding->MarkForDeath();

  return PL_DHASH_NEXT;
}

void
nsBindingManager::FlushSkinBindings()
{
  if (mBoundContentSet) {
    mBoundContentSet->EnumerateEntries(MarkForDeath, nullptr);
  }
}

// Used below to protect from recurring in QI calls through XPConnect.
struct AntiRecursionData {
  nsIContent* element;
  REFNSIID iid;
  AntiRecursionData* next;

  AntiRecursionData(nsIContent* aElement,
                    REFNSIID aIID,
                    AntiRecursionData* aNext)
    : element(aElement), iid(aIID), next(aNext) {}
};

nsresult
nsBindingManager::GetBindingImplementation(nsIContent* aContent, REFNSIID aIID,
                                           void** aResult)
{
  *aResult = nullptr;
  nsXBLBinding *binding = aContent ? aContent->GetXBLBinding() : nullptr;
  if (binding) {
    // The binding should not be asked for nsISupports
    NS_ASSERTION(!aIID.Equals(NS_GET_IID(nsISupports)), "Asking a binding for nsISupports");
    if (binding->ImplementsInterface(aIID)) {
      nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = GetWrappedJS(aContent);

      if (wrappedJS) {
        // Protect from recurring in QI calls through XPConnect.
        // This can happen when a second binding is being resolved.
        // At that point a wrappedJS exists, but it doesn't yet know about
        // the iid we are asking for. So, without this protection,
        // AggregatedQueryInterface would end up recurring back into itself
        // through this code.
        //
        // With this protection, when we detect the recursion we return
        // NS_NOINTERFACE in the inner call. The outer call will then fall
        // through (see below) and build a new chained wrappedJS for the iid.
        //
        // We're careful to not assume that only one direct nesting can occur
        // because there is a call into JS in the middle and we can't assume
        // that this code won't be reached by some more complex nesting path.
        //
        // NOTE: We *assume* this is single threaded, so we can use a
        // static linked list to do the check.

        static AntiRecursionData* list = nullptr;

        for (AntiRecursionData* p = list; p; p = p->next) {
          if (p->element == aContent && p->iid.Equals(aIID)) {
            *aResult = nullptr;
            return NS_NOINTERFACE;
          }
        }

        AntiRecursionData item(aContent, aIID, list);
        list = &item;

        nsresult rv = wrappedJS->AggregatedQueryInterface(aIID, aResult);

        list = item.next;

        if (*aResult)
          return rv;

        // No result was found, so this must be another XBL interface.
        // Fall through to create a new wrapper.
      }

      // We have never made a wrapper for this implementation.
      // Create an XPC wrapper for the script object and hand it back.
      AutoJSAPI jsapi;
      jsapi.Init();
      JSContext* cx = jsapi.cx();

      nsIXPConnect *xpConnect = nsContentUtils::XPConnect();

      JS::Rooted<JSObject*> jsobj(cx, aContent->GetWrapper());
      NS_ENSURE_TRUE(jsobj, NS_NOINTERFACE);

      // If we're using an XBL scope, we need to use the Xray view to the bound
      // content in order to view the full array of methods defined in the
      // binding, some of which may not be exposed on the prototype of
      // untrusted content. We don't need to consider add-on scopes here
      // because they're chrome-only and no Xrays are involved.
      //
      // If there's no separate XBL scope, or if the reflector itself lives in
      // the XBL scope, we'll end up with the global of the reflector.
      JS::Rooted<JSObject*> xblScope(cx, xpc::GetXBLScopeOrGlobal(cx, jsobj));
      NS_ENSURE_TRUE(xblScope, NS_ERROR_UNEXPECTED);
      JSAutoCompartment ac(cx, xblScope);
      bool ok = JS_WrapObject(cx, &jsobj);
      NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
      MOZ_ASSERT_IF(js::IsWrapper(jsobj), xpc::IsXrayWrapper(jsobj));

      nsresult rv = xpConnect->WrapJSAggregatedToNative(aContent, cx,
                                                        jsobj, aIID, aResult);
      if (NS_FAILED(rv))
        return rv;

      // We successfully created a wrapper.  We will own this wrapper for as long as the binding remains
      // alive.  At the time the binding is cleared out of the bindingManager, we will remove the wrapper
      // from the bindingManager as well.
      nsISupports* supp = static_cast<nsISupports*>(*aResult);
      wrappedJS = do_QueryInterface(supp);
      SetWrappedJS(aContent, wrappedJS);

      return rv;
    }
  }

  *aResult = nullptr;
  return NS_NOINTERFACE;
}

nsresult
nsBindingManager::WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc,
                            ElementDependentRuleProcessorData* aData,
                            bool* aCutOffInheritance)
{
  *aCutOffInheritance = false;

  NS_ASSERTION(aData->mElement, "How did that happen?");

  // Walk the binding scope chain, starting with the binding attached to our
  // content, up till we run out of scopes or we get cut off.
  nsIContent *content = aData->mElement;

  do {
    nsXBLBinding *binding = content->GetXBLBinding();
    if (binding) {
      aData->mTreeMatchContext.mScopedRoot = content;
      binding->WalkRules(aFunc, aData);
      // If we're not looking at our original content, allow the binding to cut
      // off style inheritance
      if (content != aData->mElement) {
        if (!binding->InheritsStyle()) {
          // Go no further; we're not inheriting style from anything above here
          break;
        }
      }
    }

    if (content->IsRootOfNativeAnonymousSubtree()) {
      break; // Deliberately cut off style inheritance here.
    }

    content = content->GetBindingParent();
  } while (content);

  // If "content" is non-null that means we cut off inheritance at some point
  // in the loop.
  *aCutOffInheritance = (content != nullptr);

  // Null out the scoped root that we set repeatedly
  aData->mTreeMatchContext.mScopedRoot = nullptr;

  return NS_OK;
}

typedef nsTHashtable<nsPtrHashKey<nsIStyleRuleProcessor> > RuleProcessorSet;

static PLDHashOperator
EnumRuleProcessors(nsRefPtrHashKey<nsIContent> *aKey, void* aClosure)
{
  nsIContent *boundContent = aKey->GetKey();
  nsAutoPtr<RuleProcessorSet> *set = static_cast<nsAutoPtr<RuleProcessorSet>*>(aClosure);
  for (nsXBLBinding *binding = boundContent->GetXBLBinding(); binding;
       binding = binding->GetBaseBinding()) {
    nsIStyleRuleProcessor *ruleProc =
      binding->PrototypeBinding()->GetRuleProcessor();
    if (ruleProc) {
      if (!(*set)) {
        *set = new RuleProcessorSet;
      }
      (*set)->PutEntry(ruleProc);
    }
  }
  return PL_DHASH_NEXT;
}

struct WalkAllRulesData {
  nsIStyleRuleProcessor::EnumFunc mFunc;
  ElementDependentRuleProcessorData* mData;
};

static PLDHashOperator
EnumWalkAllRules(nsPtrHashKey<nsIStyleRuleProcessor> *aKey, void* aClosure)
{
  nsIStyleRuleProcessor *ruleProcessor = aKey->GetKey();

  WalkAllRulesData *data = static_cast<WalkAllRulesData*>(aClosure);

  (*(data->mFunc))(ruleProcessor, data->mData);

  return PL_DHASH_NEXT;
}

void
nsBindingManager::WalkAllRules(nsIStyleRuleProcessor::EnumFunc aFunc,
                               ElementDependentRuleProcessorData* aData)
{
  if (!mBoundContentSet) {
    return;
  }

  nsAutoPtr<RuleProcessorSet> set;
  mBoundContentSet->EnumerateEntries(EnumRuleProcessors, &set);
  if (!set)
    return;

  WalkAllRulesData data = { aFunc, aData };
  set->EnumerateEntries(EnumWalkAllRules, &data);
}

struct MediumFeaturesChangedData {
  nsPresContext *mPresContext;
  bool *mRulesChanged;
};

static PLDHashOperator
EnumMediumFeaturesChanged(nsPtrHashKey<nsIStyleRuleProcessor> *aKey, void* aClosure)
{
  nsIStyleRuleProcessor *ruleProcessor = aKey->GetKey();

  MediumFeaturesChangedData *data =
    static_cast<MediumFeaturesChangedData*>(aClosure);

  bool thisChanged = ruleProcessor->MediumFeaturesChanged(data->mPresContext);
  *data->mRulesChanged = *data->mRulesChanged || thisChanged;

  return PL_DHASH_NEXT;
}

nsresult
nsBindingManager::MediumFeaturesChanged(nsPresContext* aPresContext,
                                        bool* aRulesChanged)
{
  *aRulesChanged = false;
  if (!mBoundContentSet) {
    return NS_OK;
  }

  nsAutoPtr<RuleProcessorSet> set;
  mBoundContentSet->EnumerateEntries(EnumRuleProcessors, &set);
  if (!set) {
    return NS_OK;
  }

  MediumFeaturesChangedData data = { aPresContext, aRulesChanged };
  set->EnumerateEntries(EnumMediumFeaturesChanged, &data);
  return NS_OK;
}

static PLDHashOperator
EnumAppendAllSheets(nsRefPtrHashKey<nsIContent> *aKey, void* aClosure)
{
  nsIContent *boundContent = aKey->GetKey();
  nsTArray<CSSStyleSheet*>* array =
    static_cast<nsTArray<CSSStyleSheet*>*>(aClosure);
  for (nsXBLBinding *binding = boundContent->GetXBLBinding(); binding;
       binding = binding->GetBaseBinding()) {
    binding->PrototypeBinding()->AppendStyleSheetsTo(*array);
  }
  return PL_DHASH_NEXT;
}

void
nsBindingManager::AppendAllSheets(nsTArray<CSSStyleSheet*>& aArray)
{
  if (mBoundContentSet) {
    mBoundContentSet->EnumerateEntries(EnumAppendAllSheets, &aArray);
  }
}

static void
InsertAppendedContent(XBLChildrenElement* aPoint,
                      nsIContent* aFirstNewContent)
{
  int32_t insertionIndex;
  if (nsIContent* prevSibling = aFirstNewContent->GetPreviousSibling()) {
    // If we have a previous sibling, then it must already be in aPoint. Find
    // it and insert after it.
    insertionIndex = aPoint->IndexOfInsertedChild(prevSibling);
    MOZ_ASSERT(insertionIndex != -1);

    // Our insertion index is one after our previous sibling's index.
    ++insertionIndex;
  } else {
    // Otherwise, we append.
    // TODO This is wrong for nested insertion points. In that case, we need to
    // keep track of the right index to insert into.
    insertionIndex = aPoint->InsertedChildrenLength();
  }

  // Do the inserting.
  for (nsIContent* currentChild = aFirstNewContent;
       currentChild;
       currentChild = currentChild->GetNextSibling()) {
    aPoint->InsertInsertedChildAt(currentChild, insertionIndex++);
  }
}

void
nsBindingManager::ContentAppended(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aFirstNewContent,
                                  int32_t     aNewIndexInContainer)
{
  if (aNewIndexInContainer == -1) {
    return;
  }

  // Try to find insertion points for all the new kids.
  XBLChildrenElement* point = nullptr;
  nsIContent* parent = aContainer;

  // Handle appending of default content.
  if (parent && parent->IsActiveChildrenElement()) {
    XBLChildrenElement* childrenEl = static_cast<XBLChildrenElement*>(parent);
    if (childrenEl->HasInsertedChildren()) {
      // Appending default content that isn't being used. Ignore.
      return;
    }

    childrenEl->MaybeSetupDefaultContent();
    parent = childrenEl->GetParent();
  }

  bool first = true;
  do {
    nsXBLBinding* binding = GetBindingWithContent(parent);
    if (!binding) {
      break;
    }

    if (binding->HasFilteredInsertionPoints()) {
      // There are filtered insertion points involved, handle each child
      // separately.
      // We could optimize this in the case when we've nested a few levels
      // deep already, without hitting bindings that have filtered insertion
      // points.
      int32_t currentIndex = aNewIndexInContainer;
      for (nsIContent* currentChild = aFirstNewContent; currentChild;
           currentChild = currentChild->GetNextSibling()) {
        HandleChildInsertion(aContainer, currentChild,
                             currentIndex++, true);
      }

      return;
    }

    point = binding->GetDefaultInsertionPoint();
    if (!point) {
      break;
    }

    // Even though we're in ContentAppended, nested insertion points force us
    // to deal with this append as an insertion except in the outermost
    // binding.
    if (first) {
      first = false;
      for (nsIContent* child = aFirstNewContent; child;
           child = child->GetNextSibling()) {
        point->AppendInsertedChild(child);
      }
    } else {
      InsertAppendedContent(point, aFirstNewContent);
    }

    nsIContent* newParent = point->GetParent();
    if (newParent == parent) {
      break;
    }
    parent = newParent;
  } while (parent);
}

void
nsBindingManager::ContentInserted(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aChild,
                                  int32_t aIndexInContainer)
{
  if (aIndexInContainer == -1) {
    return;
  }

  HandleChildInsertion(aContainer, aChild, aIndexInContainer, false);
}

void
nsBindingManager::ContentRemoved(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aChild,
                                 int32_t aIndexInContainer,
                                 nsIContent* aPreviousSibling)
{
  aChild->SetXBLInsertionParent(nullptr);

  XBLChildrenElement* point = nullptr;
  nsIContent* parent = aContainer;

  // Handle appending of default content.
  if (parent && parent->IsActiveChildrenElement()) {
    XBLChildrenElement* childrenEl = static_cast<XBLChildrenElement*>(parent);
    if (childrenEl->HasInsertedChildren()) {
      // Removing default content that isn't being used. Ignore.
      return;
    }

    parent = childrenEl->GetParent();
  }

  do {
    nsXBLBinding* binding = GetBindingWithContent(parent);
    if (!binding) {
      // If aChild is XBL content, it might have <xbl:children> elements
      // somewhere under it. We need to inform those elements that they're no
      // longer in the tree so they can tell their distributed children that
      // they're no longer distributed under them.
      // XXX This is wrong. We need to do far more work to update the parent
      // binding's list of insertion points and to get the new insertion parent
      // for the newly-distributed children correct.
      if (aChild->GetBindingParent()) {
        ClearInsertionPointsRecursively(aChild);
      }
      return;
    }

    point = binding->FindInsertionPointFor(aChild);
    if (!point) {
      break;
    }

    point->RemoveInsertedChild(aChild);

    nsIContent* newParent = point->GetParent();
    if (newParent == parent) {
      break;
    }
    parent = newParent;
  } while (parent);
}

void
nsBindingManager::ClearInsertionPointsRecursively(nsIContent* aContent)
{
  if (aContent->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
    static_cast<XBLChildrenElement*>(aContent)->ClearInsertedChildren();
  }

  for (nsIContent* child = aContent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    ClearInsertionPointsRecursively(child);
  }
}

void
nsBindingManager::DropDocumentReference()
{
  mDestroyed = true;

  // Make sure to not run any more XBL constructors
  mProcessingAttachedStack = true;
  if (mProcessAttachedQueueEvent) {
    mProcessAttachedQueueEvent->Revoke();
  }

  if (mBoundContentSet) {
    mBoundContentSet->Clear();
  }

  mDocument = nullptr;
}

void
nsBindingManager::Traverse(nsIContent *aContent,
                           nsCycleCollectionTraversalCallback &cb)
{
  if (!aContent->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR) ||
      !aContent->IsElement()) {
    // Don't traverse if content is not in this binding manager.
    // We also don't traverse non-elements because there should not
    // be bindings (checking the flag alone is not sufficient because
    // the flag is also set on children of insertion points that may be
    // non-elements).
    return;
  }

  if (mBoundContentSet && mBoundContentSet->Contains(aContent)) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "[via binding manager] mBoundContentSet entry");
    cb.NoteXPCOMChild(aContent);
  }

  nsIXPConnectWrappedJS *value = GetWrappedJS(aContent);
  if (value) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "[via binding manager] mWrapperTable key");
    cb.NoteXPCOMChild(aContent);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "[via binding manager] mWrapperTable value");
    cb.NoteXPCOMChild(value);
  }
}

void
nsBindingManager::BeginOutermostUpdate()
{
  mAttachedStackSizeOnOutermost = mAttachedStack.Length();
}

void
nsBindingManager::EndOutermostUpdate()
{
  if (!mProcessingAttachedStack) {
    ProcessAttachedQueue(mAttachedStackSizeOnOutermost);
    mAttachedStackSizeOnOutermost = 0;
  }
}

void
nsBindingManager::HandleChildInsertion(nsIContent* aContainer,
                                       nsIContent* aChild,
                                       uint32_t aIndexInContainer,
                                       bool aAppend)
{
  NS_PRECONDITION(aChild, "Must have child");
  NS_PRECONDITION(!aContainer ||
                  uint32_t(aContainer->IndexOf(aChild)) == aIndexInContainer,
                  "Child not at the right index?");

  XBLChildrenElement* point = nullptr;
  nsIContent* parent = aContainer;

  // Handle insertion of default content.
  if (parent && parent->IsActiveChildrenElement()) {
    XBLChildrenElement* childrenEl = static_cast<XBLChildrenElement*>(parent);
    if (childrenEl->HasInsertedChildren()) {
      // Inserting default content that isn't being used. Ignore.
      return;
    }

    childrenEl->MaybeSetupDefaultContent();
    parent = childrenEl->GetParent();
  }

  while (parent) {
    nsXBLBinding* binding = GetBindingWithContent(parent);
    if (!binding) {
      break;
    }

    point = binding->FindInsertionPointFor(aChild);
    if (!point) {
      break;
    }

    // Insert the child into the proper insertion point.
    // TODO If there were multiple insertion points, this approximation can be
    // wrong. We need to re-run the distribution algorithm. In the meantime,
    // this should work well enough.
    uint32_t index = aAppend ? point->InsertedChildrenLength() : 0;
    for (nsIContent* currentSibling = aChild->GetPreviousSibling();
         currentSibling;
         currentSibling = currentSibling->GetPreviousSibling()) {
      // If we find one of our previous siblings in the insertion point, the
      // index following it is the correct insertion point. Otherwise, we guess
      // based on whether we're appending or inserting.
      int32_t pointIndex = point->IndexOfInsertedChild(currentSibling);
      if (pointIndex != -1) {
        index = pointIndex + 1;
        break;
      }
    }

    point->InsertInsertedChildAt(aChild, index);

    nsIContent* newParent = point->GetParent();
    if (newParent == parent) {
      break;
    }

    parent = newParent;
  }
}


nsIContent*
nsBindingManager::FindNestedInsertionPoint(nsIContent* aContainer,
                                           nsIContent* aChild)
{
  NS_PRECONDITION(aChild->GetParent() == aContainer,
                  "Wrong container");

  nsIContent* parent = aContainer;
  if (aContainer->IsActiveChildrenElement()) {
    if (static_cast<XBLChildrenElement*>(aContainer)->
          HasInsertedChildren()) {
      return nullptr;
    }
    parent = aContainer->GetParent();
  }

  while (parent) {
    nsXBLBinding* binding = GetBindingWithContent(parent);
    if (!binding) {
      break;
    }

    XBLChildrenElement* point = binding->FindInsertionPointFor(aChild);
    if (!point) {
      return nullptr;
    }

    nsIContent* newParent = point->GetParent();
    if (newParent == parent) {
      break;
    }
    parent = newParent;
  }

  return parent;
}

nsIContent*
nsBindingManager::FindNestedSingleInsertionPoint(nsIContent* aContainer,
                                                 bool* aMulti)
{
  *aMulti = false;

  nsIContent* parent = aContainer;
  if (aContainer->IsActiveChildrenElement()) {
    if (static_cast<XBLChildrenElement*>(aContainer)->
          HasInsertedChildren()) {
      return nullptr;
    }
    parent = aContainer->GetParent();
  }

  while(parent) {
    nsXBLBinding* binding = GetBindingWithContent(parent);
    if (!binding) {
      break;
    }

    if (binding->HasFilteredInsertionPoints()) {
      *aMulti = true;
      return nullptr;
    }

    XBLChildrenElement* point = binding->GetDefaultInsertionPoint();
    if (!point) {
      return nullptr;
    }

    nsIContent* newParent = point->GetParent();
    if (newParent == parent) {
      break;
    }
    parent = newParent;
  }

  return parent;
}
