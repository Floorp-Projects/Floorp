/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsXBLService.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"
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

#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLDocumentInfo.h"
#include "mozilla/dom/XBLChildrenElement.h"

#include "nsIStyleRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include "nsIWeakReference.h"

#include "jsapi.h"
#include "nsIXPConnect.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIScriptGlobalObject.h"
#include "nsTHashtable.h"

#include "nsIScriptContext.h"
#include "nsBindingManager.h"
#include "nsCxPusher.h"

#include "nsThreadUtils.h"
#include "mozilla/dom/NodeListBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

//
// Generic pldhash table stuff for mapping one nsISupports to another
//
// These values are never null - a null value implies that this
// whole key should be removed (See SetOrRemoveObject)
class ObjectEntry : public PLDHashEntryHdr
{
public:

  // note that these are allocated within the PLDHashTable, but we
  // want to keep track of them anyway
  ObjectEntry() { MOZ_COUNT_CTOR(ObjectEntry); }
  ~ObjectEntry() { MOZ_COUNT_DTOR(ObjectEntry); }
  
  nsISupports* GetValue() { return mValue; }
  nsISupports* GetKey() { return mKey; }
  void SetValue(nsISupports* aValue) { mValue = aValue; }
  void SetKey(nsISupports* aKey) { mKey = aKey; }
  
private:
  nsCOMPtr<nsISupports> mKey;
  nsCOMPtr<nsISupports> mValue;
};

static void
ClearObjectEntry(PLDHashTable* table, PLDHashEntryHdr *entry)
{
  ObjectEntry* objEntry = static_cast<ObjectEntry*>(entry);
  objEntry->~ObjectEntry();
}

static bool
InitObjectEntry(PLDHashTable* table, PLDHashEntryHdr* entry, const void* key)
{
  new (entry) ObjectEntry;
  return true;
}
  


static PLDHashTableOps ObjectTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  PL_DHashMatchEntryStub,
  PL_DHashMoveEntryStub,
  ClearObjectEntry,
  PL_DHashFinalizeStub,
  InitObjectEntry
};

// helper routine for adding a new entry
static nsresult
AddObjectEntry(PLDHashTable& table, nsISupports* aKey, nsISupports* aValue)
{
  NS_ASSERTION(aKey, "key must be non-null");
  if (!aKey) return NS_ERROR_INVALID_ARG;
  
  ObjectEntry *entry =
    static_cast<ObjectEntry*>
               (PL_DHashTableOperate(&table, aKey, PL_DHASH_ADD));

  if (!entry)
    return NS_ERROR_OUT_OF_MEMORY;

  // only add the key if the entry is new
  if (!entry->GetKey())
    entry->SetKey(aKey);

  // now attach the new entry - note that entry->mValue could possibly
  // have a value already, this will release that.
  entry->SetValue(aValue);
  
  return NS_OK;
}

// helper routine for looking up an existing entry. Note that the
// return result is NOT addreffed
static nsISupports*
LookupObject(PLDHashTable& table, nsIContent* aKey)
{
  if (aKey && aKey->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    ObjectEntry *entry =
      static_cast<ObjectEntry*>
                 (PL_DHashTableOperate(&table, aKey, PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(entry))
      return entry->GetValue();
  }

  return nullptr;
}

inline void
RemoveObjectEntry(PLDHashTable& table, nsISupports* aKey)
{
  PL_DHashTableOperate(&table, aKey, PL_DHASH_REMOVE);
}

static nsresult
SetOrRemoveObject(PLDHashTable& table, nsIContent* aKey, nsISupports* aValue)
{
  if (aValue) {
    // lazily create the table, but only when adding elements
    if (!table.ops &&
        !PL_DHashTableInit(&table, &ObjectTableOps, nullptr,
                           sizeof(ObjectEntry), 16)) {
      table.ops = nullptr;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aKey->SetFlags(NODE_MAY_BE_IN_BINDING_MNGR);
    return AddObjectEntry(table, aKey, aValue);
  }

  // no value, so remove the key from the table
  if (table.ops) {
    ObjectEntry* entry =
      static_cast<ObjectEntry*>
        (PL_DHashTableOperate(&table, aKey, PL_DHASH_LOOKUP));
    if (entry && PL_DHASH_ENTRY_IS_BUSY(entry)) {
      // Keep key and value alive while removing the entry.
      nsCOMPtr<nsISupports> key = entry->GetKey();
      nsCOMPtr<nsISupports> value = entry->GetValue();
      RemoveObjectEntry(table, aKey);
    }
  }
  return NS_OK;
}

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods

NS_IMPL_CYCLE_COLLECTION_CLASS(nsBindingManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsBindingManager)
  tmp->mDestroyed = true;

  if (tmp->mBoundContentSet.IsInitialized())
    tmp->mBoundContentSet.Clear();

  if (tmp->mDocumentTable.IsInitialized())
    tmp->mDocumentTable.Clear();

  if (tmp->mLoadingDocTable.IsInitialized())
    tmp->mLoadingDocTable.Clear();

  if (tmp->mWrapperTable.ops)
    PL_DHashTableFinish(&(tmp->mWrapperTable));
  tmp->mWrapperTable.ops = nullptr;

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
  if (tmp->mDocumentTable.IsInitialized())
      tmp->mDocumentTable.EnumerateRead(&DocumentInfoHashtableTraverser, &cb);
  if (tmp->mLoadingDocTable.IsInitialized())
      tmp->mLoadingDocTable.EnumerateRead(&LoadingDocHashtableTraverser, &cb);
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
  mWrapperTable.ops = nullptr;
}

nsBindingManager::~nsBindingManager(void)
{
  mDestroyed = true;

  if (mWrapperTable.ops)
    PL_DHashTableFinish(&mWrapperTable);
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
  if (!mBoundContentSet.IsInitialized()) {
    mBoundContentSet.Init();
  }

  mBoundContentSet.PutEntry(aContent);
}

void
nsBindingManager::RemoveBoundContent(nsIContent* aContent)
{
  if (mBoundContentSet.IsInitialized()) {
    mBoundContentSet.RemoveEntry(aContent);
  }

  // The death of the bindings means the death of the JS wrapper.
  SetWrappedJS(aContent, nullptr);
}

nsIXPConnectWrappedJS*
nsBindingManager::GetWrappedJS(nsIContent* aContent)
{ 
  if (mWrapperTable.ops) {
    return static_cast<nsIXPConnectWrappedJS*>(LookupObject(mWrapperTable, aContent));
  }

  return nullptr;
}

nsresult
nsBindingManager::SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aWrappedJS)
{
  if (mDestroyed) {
    return NS_OK;
  }

  return SetOrRemoveObject(mWrapperTable, aContent, aWrappedJS);
}

void
nsBindingManager::RemovedFromDocumentInternal(nsIContent* aContent,
                                              nsIDocument* aOldDocument)
{
  NS_PRECONDITION(aOldDocument != nullptr, "no old document");

  if (mDestroyed)
    return;

  nsRefPtr<nsXBLBinding> binding = aContent->GetXBLBinding();
  if (binding) {
    binding->PrototypeBinding()->BindingDetached(binding->GetBoundElement());
    binding->ChangeDocument(aOldDocument, nullptr);
    aContent->SetXBLBinding(nullptr, this);
  }

  // Clear out insertion parents and content lists.
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
  return aContent->Tag();
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
  uint32_t index = mAttachedStack.IndexOf(aBinding);
  if (index != mAttachedStack.NoIndex) {
    mAttachedStack[index] = nullptr;
  }
}

nsresult
nsBindingManager::AddToAttachedQueue(nsXBLBinding* aBinding)
{
  if (!mAttachedStack.AppendElement(aBinding))
    return NS_ERROR_OUT_OF_MEMORY;

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
    PostProcessAttachedQueueEvent();
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
  if (mBoundContentSet.IsInitialized()) {
    BindingTableReadClosure closure;
    mBoundContentSet.EnumerateEntries(AccumulateBindingsToDetach, &closure);
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
  
  if (!mDocumentTable.IsInitialized())
    mDocumentTable.Init(16);

  mDocumentTable.Put(aDocumentInfo->DocumentURI(),
                     aDocumentInfo);

  return NS_OK;
}

void
nsBindingManager::RemoveXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo)
{
  if (mDocumentTable.IsInitialized()) {
    mDocumentTable.Remove(aDocumentInfo->DocumentURI());
  }
}

nsXBLDocumentInfo*
nsBindingManager::GetXBLDocumentInfo(nsIURI* aURL)
{
  if (!mDocumentTable.IsInitialized())
    return nullptr;

  return mDocumentTable.GetWeak(aURL);
}

nsresult
nsBindingManager::PutLoadingDocListener(nsIURI* aURL, nsIStreamListener* aListener)
{
  NS_PRECONDITION(aListener, "Must have a non-null listener!");
  
  if (!mLoadingDocTable.IsInitialized())
    mLoadingDocTable.Init(16);
  
  mLoadingDocTable.Put(aURL, aListener);

  return NS_OK;
}

nsIStreamListener*
nsBindingManager::GetLoadingDocListener(nsIURI* aURL)
{
  if (!mLoadingDocTable.IsInitialized())
    return nullptr;

  return mLoadingDocTable.GetWeak(aURL);
}

void
nsBindingManager::RemoveLoadingDocListener(nsIURI* aURL)
{
  if (mLoadingDocTable.IsInitialized()) {
    mLoadingDocTable.Remove(aURL);
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
  if (mBoundContentSet.IsInitialized()) {
    mBoundContentSet.EnumerateEntries(MarkForDeath, nullptr);
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

      nsIDocument* doc = aContent->OwnerDoc();

      nsCOMPtr<nsIScriptGlobalObject> global =
        do_QueryInterface(doc->GetWindow());
      if (!global)
        return NS_NOINTERFACE;

      nsIScriptContext *context = global->GetContext();
      if (!context)
        return NS_NOINTERFACE;

      AutoPushJSContext jscontext(context->GetNativeContext());
      if (!jscontext)
        return NS_NOINTERFACE;

      nsIXPConnect *xpConnect = nsContentUtils::XPConnect();

      JSObject* jsobj = aContent->GetWrapper();
      NS_ENSURE_TRUE(jsobj, NS_NOINTERFACE);

      nsresult rv = xpConnect->WrapJSAggregatedToNative(aContent, jscontext,
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
  RuleProcessorSet *set = static_cast<RuleProcessorSet*>(aClosure);
  for (nsXBLBinding *binding = boundContent->GetXBLBinding(); binding;
       binding = binding->GetBaseBinding()) {
    nsIStyleRuleProcessor *ruleProc =
      binding->PrototypeBinding()->GetRuleProcessor();
    if (ruleProc) {
      if (!set->IsInitialized()) {
        set->Init(16);
      }
      set->PutEntry(ruleProc);
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
  if (!mBoundContentSet.IsInitialized()) {
    return;
  }

  RuleProcessorSet set;
  mBoundContentSet.EnumerateEntries(EnumRuleProcessors, &set);
  if (!set.IsInitialized())
    return;

  WalkAllRulesData data = { aFunc, aData };
  set.EnumerateEntries(EnumWalkAllRules, &data);
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
  if (!mBoundContentSet.IsInitialized()) {
    return NS_OK;
  }

  RuleProcessorSet set;
  mBoundContentSet.EnumerateEntries(EnumRuleProcessors, &set);
  if (!set.IsInitialized())
    return NS_OK;

  MediumFeaturesChangedData data = { aPresContext, aRulesChanged };
  set.EnumerateEntries(EnumMediumFeaturesChanged, &data);
  return NS_OK;
}

static PLDHashOperator
EnumAppendAllSheets(nsRefPtrHashKey<nsIContent> *aKey, void* aClosure)
{
  nsIContent *boundContent = aKey->GetKey();
  nsTArray<nsCSSStyleSheet*>* array =
    static_cast<nsTArray<nsCSSStyleSheet*>*>(aClosure);
  for (nsXBLBinding *binding = boundContent->GetXBLBinding(); binding;
       binding = binding->GetBaseBinding()) {
    nsXBLPrototypeResources::sheet_array_type* sheets =
      binding->PrototypeBinding()->GetStyleSheets();
    if (sheets) {
      // Copy from nsTArray<nsRefPtr<nsCSSStyleSheet> > to
      // nsTArray<nsCSSStyleSheet*>.
      array->AppendElements(*sheets);
    }
  }
  return PL_DHASH_NEXT;
}

void
nsBindingManager::AppendAllSheets(nsTArray<nsCSSStyleSheet*>& aArray)
{
  if (mBoundContentSet.IsInitialized()) {
    mBoundContentSet.EnumerateEntries(EnumAppendAllSheets, &aArray);
  }
}

static void
InsertAppendedContent(XBLChildrenElement* aPoint,
                      nsIContent* aFirstNewContent)
{
  uint32_t insertionIndex;
  if (nsIContent* prevSibling = aFirstNewContent->GetPreviousSibling()) {
    // If we have a previous sibling, then it must already be in aPoint. Find
    // it and insert after it.
    insertionIndex = aPoint->IndexOfInsertedChild(prevSibling);
    MOZ_ASSERT(insertionIndex != aPoint->NoIndex);

    // Our insertion index is one after our previous sibling's index.
    ++insertionIndex;
  } else {
    // Otherwise, we append.
    // TODO This is wrong for nested insertion points. In that case, we need to
    // keep track of the right index to insert into.
    insertionIndex = aPoint->mInsertedChildren.Length();
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
    static_cast<XBLChildrenElement*>(aContent)->ClearInsertedChildrenAndInsertionParents();
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

  if (mBoundContentSet.IsInitialized()) {
    mBoundContentSet.Clear();
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

  if (mBoundContentSet.IsInitialized() && mBoundContentSet.Contains(aContent)) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "[via binding manager] mBoundContentSet entry");
    cb.NoteXPCOMChild(aContent);
  }

  nsISupports *value;
  if (mWrapperTable.ops &&
      (value = LookupObject(mWrapperTable, aContent))) {
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
    uint32_t index = aAppend ? point->mInsertedChildren.Length() : 0;
    for (nsIContent* currentSibling = aChild->GetPreviousSibling();
         currentSibling;
         currentSibling = currentSibling->GetPreviousSibling()) {
      // If we find one of our previous siblings in the insertion point, the
      // index following it is the correct insertion point. Otherwise, we guess
      // based on whether we're appending or inserting.
      uint32_t pointIndex = point->IndexOfInsertedChild(currentSibling);
      if (pointIndex != point->NoIndex) {
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
