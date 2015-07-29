/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsXBLDocumentInfo.h"
#include "nsIDocument.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "nsIDOMScriptObjectFactory.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsIURI.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIChromeRegistry.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "mozilla/Services.h"
#include "xpcpublic.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "nsCCUncollectableMarker.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/URL.h"

using namespace mozilla;
using namespace mozilla::scache;
using namespace mozilla::dom;

static const char kXBLCachePrefix[] = "xblcache";

/* Implementation file */
NS_IMPL_CYCLE_COLLECTION_CLASS(nsXBLDocumentInfo)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXBLDocumentInfo)
  if (tmp->mBindingTable) {
    for (auto iter = tmp->mBindingTable->ConstIter();
         !iter.Done(); iter.Next()) {
      iter.UserData()->Unlink();
    }
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXBLDocumentInfo)
  if (tmp->mDocument &&
      nsCCUncollectableMarker::InGeneration(cb, tmp->mDocument->GetMarkedCCGeneration())) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  if (tmp->mBindingTable) {
    for (auto iter = tmp->mBindingTable->ConstIter();
         !iter.Done(); iter.Next()) {
      iter.UserData()->Traverse(cb);
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsXBLDocumentInfo)
  if (tmp->mBindingTable) {
    for (auto iter = tmp->mBindingTable->ConstIter();
         !iter.Done(); iter.Next()) {
      iter.UserData()->Trace(aCallbacks, aClosure);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

static void
UnmarkXBLJSObject(JS::GCCellPtr aPtr, const char* aName, void* aClosure)
{
  JS::ExposeObjectToActiveJS(&aPtr.as<JSObject>());
}

static PLDHashOperator
UnmarkProtos(const nsACString &aKey, nsXBLPrototypeBinding *aProto, void* aClosure)
{
  aProto->Trace(TraceCallbackFunc(UnmarkXBLJSObject), nullptr);
  return PL_DHASH_NEXT;
}

void
nsXBLDocumentInfo::MarkInCCGeneration(uint32_t aGeneration)
{
  if (mDocument) {
    mDocument->MarkUncollectableForCCGeneration(aGeneration);
  }
  // Unmark any JS we hold
  if (mBindingTable) {
    mBindingTable->EnumerateRead(UnmarkProtos, nullptr);
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXBLDocumentInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXBLDocumentInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXBLDocumentInfo)

nsXBLDocumentInfo::nsXBLDocumentInfo(nsIDocument* aDocument)
  : mDocument(aDocument),
    mScriptAccess(true),
    mIsChrome(false),
    mFirstBinding(nullptr)
{
  nsIURI* uri = aDocument->GetDocumentURI();
  if (IsChromeURI(uri)) {
    // Cache whether or not this chrome XBL can execute scripts.
    nsCOMPtr<nsIXULChromeRegistry> reg =
      mozilla::services::GetXULChromeRegistryService();
    if (reg) {
      bool allow = true;
      reg->AllowScriptsForPackage(uri, &allow);
      mScriptAccess = allow;
    }
    mIsChrome = true;
  } else {
    // If this binding isn't running with system principal, then it's running
    // from a remote-XUL whitelisted domain. This is already a not-really-
    // supported configuration (among other things, we don't use XBL scopes in
    // that configuration for compatibility reasons). But we should still at
    // least make an effort to prevent binding code from running if content
    // script is disabled or if the source domain is blacklisted (since the
    // source domain for remote XBL must always be the same as the source domain
    // of the bound content).
    //
    // If we just ask the binding document if script is enabled, it will
    // discover that it has no inner window, and return false. So instead, we
    // short-circuit the normal compartment-managed script-disabling machinery,
    // and query the policy for the URI directly.
    bool allow;
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    nsresult rv = ssm->PolicyAllowsScript(uri, &allow);
    mScriptAccess = NS_SUCCEEDED(rv) && allow;
  }
}

nsXBLDocumentInfo::~nsXBLDocumentInfo()
{
  mozilla::DropJSObjects(this);
}

nsXBLPrototypeBinding*
nsXBLDocumentInfo::GetPrototypeBinding(const nsACString& aRef)
{
  if (!mBindingTable)
    return nullptr;

  if (aRef.IsEmpty()) {
    // Return our first binding
    return mFirstBinding;
  }

  return mBindingTable->Get(aRef);
}

nsresult
nsXBLDocumentInfo::SetPrototypeBinding(const nsACString& aRef, nsXBLPrototypeBinding* aBinding)
{
  if (!mBindingTable) {
    mBindingTable = new nsClassHashtable<nsCStringHashKey, nsXBLPrototypeBinding>();
    mozilla::HoldJSObjects(this);
  }

  NS_ENSURE_STATE(!mBindingTable->Get(aRef));
  mBindingTable->Put(aRef, aBinding);

  return NS_OK;
}

void
nsXBLDocumentInfo::RemovePrototypeBinding(const nsACString& aRef)
{
  if (mBindingTable) {
    nsAutoPtr<nsXBLPrototypeBinding> bindingToRemove;
    mBindingTable->RemoveAndForget(aRef, bindingToRemove);

    // We do not want to destroy the binding, so just forget it.
    bindingToRemove.forget();
  }
}

// Callback to enumerate over the bindings from this document and write them
// out to the cache.
static PLDHashOperator
WriteBinding(const nsACString &aKey, nsXBLPrototypeBinding *aProto, void* aClosure)
{
  aProto->Write((nsIObjectOutputStream*)aClosure);

  return PL_DHASH_NEXT;
}

// static
nsresult
nsXBLDocumentInfo::ReadPrototypeBindings(nsIURI* aURI, nsXBLDocumentInfo** aDocInfo)
{
  *aDocInfo = nullptr;

  nsAutoCString spec(kXBLCachePrefix);
  nsresult rv = PathifyURI(aURI, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  StartupCache* startupCache = StartupCache::GetSingleton();
  NS_ENSURE_TRUE(startupCache, NS_ERROR_FAILURE);

  nsAutoArrayPtr<char> buf;
  uint32_t len;
  rv = startupCache->GetBuffer(spec.get(), getter_Transfers(buf), &len);
  // GetBuffer will fail if the binding is not in the cache.
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIObjectInputStream> stream;
  rv = NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);
  buf.forget();

  // The file compatibility.ini stores the build id. This is checked in
  // nsAppRunner.cpp and will delete the cache if a different build is
  // present. However, we check that the version matches here to be safe. 
  uint32_t version;
  rv = stream->Read32(&version);
  NS_ENSURE_SUCCESS(rv, rv);
  if (version != XBLBinding_Serialize_Version) {
    // The version that exists is different than expected, likely created with a
    // different build, so invalidate the cache.
    startupCache->InvalidateCache();
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsContentUtils::GetSecurityManager()->
    GetSystemPrincipal(getter_AddRefs(principal));

  nsCOMPtr<nsIDOMDocument> domdoc;
  rv = NS_NewXBLDocument(getter_AddRefs(domdoc), aURI, nullptr, principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  NS_ASSERTION(doc, "Must have a document!");
  nsRefPtr<nsXBLDocumentInfo> docInfo = new nsXBLDocumentInfo(doc);

  while (1) {
    uint8_t flags;
    nsresult rv = stream->Read8(&flags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (flags == XBLBinding_Serialize_NoMoreBindings)
      break;

    rv = nsXBLPrototypeBinding::ReadNewBinding(stream, docInfo, doc, flags);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  docInfo.swap(*aDocInfo);
  return NS_OK;
}

nsresult
nsXBLDocumentInfo::WritePrototypeBindings()
{
  // Only write out bindings with the system principal
  if (!nsContentUtils::IsSystemPrincipal(mDocument->NodePrincipal()))
    return NS_OK;

  nsAutoCString spec(kXBLCachePrefix);
  nsresult rv = PathifyURI(DocumentURI(), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  StartupCache* startupCache = StartupCache::GetSingleton();
  NS_ENSURE_TRUE(startupCache, rv);

  nsCOMPtr<nsIObjectOutputStream> stream;
  nsCOMPtr<nsIStorageStream> storageStream;
  rv = NewObjectOutputWrappedStorageStream(getter_AddRefs(stream),
                                           getter_AddRefs(storageStream),
                                           true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->Write32(XBLBinding_Serialize_Version);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mBindingTable) {
    mBindingTable->EnumerateRead(WriteBinding, stream);
  }

  // write a end marker at the end
  rv = stream->Write8(XBLBinding_Serialize_NoMoreBindings);
  NS_ENSURE_SUCCESS(rv, rv);

  stream->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t len;
  nsAutoArrayPtr<char> buf;
  rv = NewBufferFromStorageStream(storageStream, getter_Transfers(buf), &len);
  NS_ENSURE_SUCCESS(rv, rv);

  return startupCache->PutBuffer(spec.get(), buf, len);
}

void
nsXBLDocumentInfo::SetFirstPrototypeBinding(nsXBLPrototypeBinding* aBinding)
{
  mFirstBinding = aBinding;
}

static PLDHashOperator
FlushScopedSkinSheets(const nsACString &aKey, nsXBLPrototypeBinding *aProto, void* aClosure)
{
  aProto->FlushSkinSheets();
  return PL_DHASH_NEXT;
}

void
nsXBLDocumentInfo::FlushSkinStylesheets()
{
  if (mBindingTable) {
    mBindingTable->EnumerateRead(FlushScopedSkinSheets, nullptr);
  }
}

#ifdef DEBUG
void
AssertInCompilationScope()
{
  AutoJSContext cx;
  MOZ_ASSERT(xpc::CompilationScope() == JS::CurrentGlobalOrNull(cx));
}
#endif
