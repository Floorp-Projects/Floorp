/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EnumVariant.h"
#include "ia2AccessibleApplication.h"
#include "ia2AccessibleHypertext.h"
#include "ia2AccessibleImage.h"
#include "ia2AccessibleTable.h"
#include "ia2AccessibleTableCell.h"
#include "mozilla/a11y/AccessibleWrap.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/mscom/Interceptor.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "MsaaAccessible.h"
#include "MsaaDocAccessible.h"
#include "MsaaRootAccessible.h"
#include "MsaaXULMenuAccessible.h"
#include "nsEventMap.h"
#include "nsViewManager.h"
#include "nsWinUtils.h"
#include "Relation.h"
#include "sdnAccessible.h"
#include "sdnTextAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

const uint32_t USE_ROLE_STRING = 0;
static const VARIANT kVarChildIdSelf = {{{VT_I4}}};

MsaaIdGenerator MsaaAccessible::sIDGen;
ITypeInfo* MsaaAccessible::gTypeInfo = nullptr;

/* static */
MsaaAccessible* MsaaAccessible::Create(Accessible* aAcc) {
  // If the cache is enabled, this should only ever be called in the parent
  // process.
  MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup() ||
             XRE_IsParentProcess());
  if (!StaticPrefs::accessibility_cache_enabled_AtStartup() &&
      aAcc->IsRemote()) {
    // MsaaAccessible is just a stub to hold the id in this case.
    return new MsaaAccessible(aAcc);
  }
  // The order of some of these is important! For example, when isRoot is true,
  // IsDoc will also be true, so we must check IsRoot first. IsTable/Cell and
  // IsHyperText are a similar case.
  if (aAcc->IsLocal() && aAcc->IsRoot()) {
    return new MsaaRootAccessible(aAcc);
  }
  if (aAcc->IsDoc()) {
    return new MsaaDocAccessible(aAcc);
  }
  if (aAcc->IsLocal()) {
    // XXX These classes don't support RemoteAccessible yet.
    if (aAcc->IsTable()) {
      return new ia2AccessibleTable(aAcc);
    }
    if (aAcc->IsTableCell()) {
      return new ia2AccessibleTableCell(aAcc);
    }
    if (aAcc->IsApplication()) {
      return new ia2AccessibleApplication(aAcc);
    }
    if (aAcc->IsHyperText()) {
      return new ia2AccessibleHypertext(aAcc);
    }
    if (aAcc->IsImage()) {
      return new ia2AccessibleImage(aAcc);
    }
    if (aAcc->AsLocal()->GetContent() &&
        aAcc->AsLocal()->GetContent()->IsXULElement(nsGkAtoms::menuitem)) {
      return new MsaaXULMenuitemAccessible(aAcc);
    }
  }
  return new MsaaAccessible(aAcc);
}

MsaaAccessible::MsaaAccessible(Accessible* aAcc) : mAcc(aAcc), mID(kNoID) {}

MsaaAccessible::~MsaaAccessible() {
  MOZ_ASSERT(!mAcc, "MsaaShutdown wasn't called!");
  if (mID != kNoID) {
    sIDGen.ReleaseID(WrapNotNull(this));
  }
}

void MsaaAccessible::MsaaShutdown() {
  // Accessibles can be shut down twice in some cases. If that happens,
  // MsaaShutdown will also be called twice because AccessibleWrap holds
  // the reference until its destructor is called; see the comments in
  // AccessibleWrap::Shutdown.
  if (!mAcc) {
    return;
  }

  if (!StaticPrefs::accessibility_cache_enabled_AtStartup() &&
      mAcc->IsRemote()) {
    // This MsaaAccessible is just a stub. We just need to clear mAcc.
    mAcc = nullptr;
    return;
  }

  if (mID != kNoID) {
    auto doc = MsaaDocAccessible::GetFromOwned(mAcc);
    MOZ_ASSERT(doc);
    doc->RemoveID(mID);
  }

  if (XRE_IsContentProcess()) {
    // Bug 1434822: To improve performance for cross-process COM, we disable COM
    // garbage collection. However, this means we never receive Release calls
    // from clients, so defunct accessibles can never be deleted. Since we
    // know when an accessible is shutting down, we can work around this by
    // forcing COM to disconnect this object from all of its remote clients,
    // which will cause associated references to be released.
    IUnknown* unk = static_cast<IAccessible*>(this);
    mscom::Interceptor::DisconnectRemotesForTarget(unk);
    // If an accessible was retrieved via IAccessibleHypertext::hyperlink*,
    // it will have a different Interceptor that won't be matched by the above
    // call, even though it's the same object. Therefore, call it again with
    // the IAccessibleHyperlink pointer. We can remove this horrible hack once
    // bug 1440267 is fixed.
    unk = static_cast<IAccessibleHyperlink*>(this);
    mscom::Interceptor::DisconnectRemotesForTarget(unk);
    for (auto& assocUnk : mAssociatedCOMObjectsForDisconnection) {
      mscom::Interceptor::DisconnectRemotesForTarget(assocUnk);
    }
    mAssociatedCOMObjectsForDisconnection.Clear();
  }

  mAcc = nullptr;
}

void MsaaAccessible::SetID(uint32_t aID) {
  MOZ_ASSERT(XRE_IsParentProcess() && mAcc &&
             !StaticPrefs::accessibility_cache_enabled_AtStartup());
  mID = aID;
}

int32_t MsaaAccessible::GetChildIDFor(Accessible* aAccessible) {
  // A child ID of the window is required, when we use NotifyWinEvent,
  // so that the 3rd party application can call back and get the IAccessible
  // the event occurred on.

  if (!aAccessible) {
    return 0;
  }

  // If the cache is disabled, chrome should use mID which has been generated by
  // the content process.
  if (!StaticPrefs::accessibility_cache_enabled_AtStartup() &&
      aAccessible->IsRemote()) {
    const uint32_t id = MsaaAccessible::GetFrom(aAccessible)->mID;
    MOZ_ASSERT(id != kNoID);
    return id;
  }

  auto doc = MsaaDocAccessible::GetFromOwned(aAccessible);
  if (!doc) {
    return 0;
  }

  uint32_t* id = &MsaaAccessible::GetFrom(aAccessible)->mID;
  if (*id != kNoID) return *id;

  *id = sIDGen.GetID();
  doc->AddID(*id, aAccessible);

  return *id;
}

/* static */
uint32_t MsaaAccessible::GetContentProcessIdFor(
    dom::ContentParentId aIPCContentId) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return 0;
  }
  return sIDGen.GetContentProcessIDFor(aIPCContentId);
}

/* static */
void MsaaAccessible::ReleaseContentProcessIdFor(
    dom::ContentParentId aIPCContentId) {
  sIDGen.ReleaseContentProcessIDFor(aIPCContentId);
}

/* static */
void MsaaAccessible::AssignChildIDTo(NotNull<sdnAccessible*> aSdnAcc) {
  aSdnAcc->SetUniqueID(sIDGen.GetID());
}

/* static */
void MsaaAccessible::ReleaseChildID(NotNull<sdnAccessible*> aSdnAcc) {
  sIDGen.ReleaseID(aSdnAcc);
}

HWND MsaaAccessible::GetHWNDFor(Accessible* aAccessible) {
  if (!aAccessible) {
    return nullptr;
  }

  LocalAccessible* localAcc = aAccessible->AsLocal();
  if (!localAcc) {
    RemoteAccessible* proxy = aAccessible->AsRemote();
    if (!proxy) {
      return nullptr;
    }

    // If window emulation is enabled, retrieve the emulated window from the
    // containing document document proxy.
    if (nsWinUtils::IsWindowEmulationStarted()) {
      DocAccessibleParent* doc = proxy->Document();
      HWND hWnd = doc->GetEmulatedWindowHandle();
      if (hWnd) {
        return hWnd;
      }
    }

    // Accessibles in child processes are said to have the HWND of the window
    // their tab is within.  Popups are always in the parent process, and so
    // never proxied, which means this is basically correct.
    LocalAccessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    if (!outerDoc) {
      // In some cases, the outer document accessible may be unattached from its
      // document at this point, if it is scheduled for removal. Do not assert
      // in such case. An example: putting aria-hidden="true" on HTML:iframe
      // element will destroy iframe's document asynchroniously, but
      // the document may be a target of selection events until then, and thus
      // it may attempt to deliever these events to MSAA clients.
      return nullptr;
    }

    return GetHWNDFor(outerDoc);
  }

  DocAccessible* document = localAcc->Document();
  if (!document) return nullptr;

  // Popup lives in own windows, use its HWND until the popup window is
  // hidden to make old JAWS versions work with collapsed comboboxes (see
  // discussion in bug 379678).
  nsIFrame* frame = localAcc->GetFrame();
  if (frame) {
    nsIWidget* widget = frame->GetNearestWidget();
    if (widget && widget->IsVisible()) {
      if (nsViewManager* vm = document->PresShellPtr()->GetViewManager()) {
        nsCOMPtr<nsIWidget> rootWidget;
        vm->GetRootWidget(getter_AddRefs(rootWidget));
        // Make sure the accessible belongs to popup. If not then use
        // document HWND (which might be different from root widget in the
        // case of window emulation).
        if (rootWidget != widget)
          return static_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW));
      }
    }
  }

  return static_cast<HWND>(document->GetNativeWindow());
}

static bool IsHandlerInvalidationNeeded(uint32_t aEvent) {
  // We want to return true for any events that would indicate that something
  // in the handler cache is out of date.
  switch (aEvent) {
    case EVENT_OBJECT_STATECHANGE:
    case EVENT_OBJECT_LOCATIONCHANGE:
    case EVENT_OBJECT_NAMECHANGE:
    case EVENT_OBJECT_DESCRIPTIONCHANGE:
    case EVENT_OBJECT_VALUECHANGE:
    case EVENT_OBJECT_FOCUS:
    case IA2_EVENT_ACTION_CHANGED:
    case IA2_EVENT_DOCUMENT_LOAD_COMPLETE:
    case IA2_EVENT_DOCUMENT_LOAD_STOPPED:
    case IA2_EVENT_DOCUMENT_ATTRIBUTE_CHANGED:
    case IA2_EVENT_DOCUMENT_CONTENT_CHANGED:
    case IA2_EVENT_PAGE_CHANGED:
    case IA2_EVENT_TEXT_ATTRIBUTE_CHANGED:
    case IA2_EVENT_TEXT_CHANGED:
    case IA2_EVENT_TEXT_INSERTED:
    case IA2_EVENT_TEXT_REMOVED:
    case IA2_EVENT_TEXT_UPDATED:
    case IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED:
      return true;
    default:
      return false;
  }
}

void MsaaAccessible::FireWinEvent(Accessible* aTarget, uint32_t aEventType) {
  MOZ_ASSERT(XRE_IsParentProcess());
  static_assert(sizeof(gWinEventMap) / sizeof(gWinEventMap[0]) ==
                    nsIAccessibleEvent::EVENT_LAST_ENTRY,
                "MSAA event map skewed");

  NS_ASSERTION(aEventType > 0 && aEventType < ArrayLength(gWinEventMap),
               "invalid event type");

  uint32_t winEvent = gWinEventMap[aEventType];
  if (!winEvent) return;

  int32_t childID = MsaaAccessible::GetChildIDFor(aTarget);
  if (!childID) return;  // Can't fire an event without a child ID

  HWND hwnd = GetHWNDFor(aTarget);
  if (!hwnd) {
    return;
  }

  if (IsHandlerInvalidationNeeded(winEvent)) {
    AccessibleWrap::InvalidateHandlers();
  }

  // Fire MSAA event for client area window.
  ::NotifyWinEvent(winEvent, hwnd, OBJID_CLIENT, childID);
}

AccessibleWrap* MsaaAccessible::LocalAcc() {
  if (!mAcc || mAcc->IsRemote()) {
    return nullptr;
  }
  auto acc = static_cast<AccessibleWrap*>(mAcc);
  MOZ_ASSERT(!acc || !acc->IsDefunct(),
             "mAcc defunct but MsaaShutdown wasn't called");
  return acc;
}

/**
 * This function is a helper for implementing IAccessible methods that accept
 * a Child ID as a parameter. If the child ID is CHILDID_SELF, the function
 * returns S_OK but a null *aOutInterface. Otherwise, *aOutInterface points
 * to the resolved IAccessible.
 *
 * The CHILDID_SELF case is special because in that case we actually execute
 * the implementation of the IAccessible method, whereas in the non-self case,
 * we delegate the method call to that object for execution.
 *
 * A sample invocation of this would look like:
 *
 *  RefPtr<IAccessible> accessible;
 *  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
 *  if (FAILED(hr)) {
 *    return hr;
 *  }
 *
 *  if (accessible) {
 *    return accessible->get_accFoo(kVarChildIdSelf, pszName);
 *  }
 *
 *  // Implementation for CHILDID_SELF case goes here
 */
HRESULT
MsaaAccessible::ResolveChild(const VARIANT& aVarChild,
                             IAccessible** aOutInterface) {
  MOZ_ASSERT(aOutInterface);
  *aOutInterface = nullptr;

  if (aVarChild.vt != VT_I4) {
    return E_INVALIDARG;
  }

  if (!mAcc) {
    return CO_E_OBJNOTCONNECTED;
  }

  if (aVarChild.lVal == CHILDID_SELF) {
    return S_OK;
  }

  bool isDefunct = false;
  RefPtr<IAccessible> accessible = GetIAccessibleFor(aVarChild, &isDefunct);
  if (!accessible) {
    return E_INVALIDARG;
  }

  if (isDefunct) {
    return CO_E_OBJNOTCONNECTED;
  }

  accessible.forget(aOutInterface);
  return S_OK;
}

static Accessible* GetAccessibleInSubtree(DocAccessible* aDoc, uint32_t aID) {
  Accessible* child = MsaaDocAccessible::GetFrom(aDoc)->GetAccessibleByID(aID);
  if (child) return child;

  uint32_t childDocCount = aDoc->ChildDocumentCount();
  for (uint32_t i = 0; i < childDocCount; i++) {
    child = GetAccessibleInSubtree(aDoc->GetChildDocumentAt(i), aID);
    if (child) return child;
  }

  return nullptr;
}

static Accessible* GetAccessibleInSubtree(DocAccessibleParent* aDoc,
                                          uint32_t aID) {
  Accessible* child = MsaaDocAccessible::GetFrom(aDoc)->GetAccessibleByID(aID);
  if (child) {
    return child;
  }

  size_t childDocCount = aDoc->ChildDocCount();
  for (size_t i = 0; i < childDocCount; i++) {
    child = GetAccessibleInSubtree(aDoc->ChildDocAt(i), aID);
    if (child) {
      return child;
    }
  }

  return nullptr;
}

static already_AddRefed<IDispatch> GetProxiedAccessibleInSubtree(
    DocAccessibleParent* aDoc, const VARIANT& aVarChild) {
  MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup());
  RefPtr<IAccessible> comProxy;
  int32_t docWrapperChildId = MsaaAccessible::GetChildIDFor(aDoc);
  // Only document accessible proxies at the top level of their content process
  // are created with a pointer to their COM proxy.
  if (aDoc->IsTopLevelInContentProcess()) {
    aDoc->GetCOMInterface(getter_AddRefs(comProxy));
  } else {
    auto tab = static_cast<dom::BrowserParent*>(aDoc->Manager());
    MOZ_ASSERT(tab);
    DocAccessibleParent* topLevelDoc = tab->GetTopLevelDocAccessible();
    MOZ_ASSERT(topLevelDoc && topLevelDoc->IsTopLevelInContentProcess());
    VARIANT docId = {{{VT_I4}}};
    docId.lVal = docWrapperChildId;
    RefPtr<IDispatch> disp = GetProxiedAccessibleInSubtree(topLevelDoc, docId);
    if (!disp) {
      return nullptr;
    }

    DebugOnly<HRESULT> hr =
        disp->QueryInterface(IID_IAccessible, getter_AddRefs(comProxy));
    MOZ_ASSERT(SUCCEEDED(hr));
  }

  MOZ_ASSERT(comProxy);
  if (!comProxy) {
    return nullptr;
  }

  if (docWrapperChildId == aVarChild.lVal) {
    return comProxy.forget();
  }

  RefPtr<IDispatch> disp;
  if (FAILED(comProxy->get_accChild(aVarChild, getter_AddRefs(disp)))) {
    return nullptr;
  }

  return disp.forget();
}

/**
 * If this is an OOP iframe in a content process, return the COM proxy for the
 * child document.
 * This will only ever return something when the cache is disabled. When the
 * cache is enabled, traversing to OOP iframe documents is handled in the
 * parent process via the RemoteAccessible hierarchy.
 */
static already_AddRefed<IDispatch> MaybeGetOOPIframeDocCOMProxy(
    Accessible* aAcc) {
  if (!XRE_IsContentProcess() || !aAcc->IsOuterDoc()) {
    return nullptr;
  }
  MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup());
  LocalAccessible* outerDoc = aAcc->AsLocal();
  auto bridge = dom::BrowserBridgeChild::GetFrom(outerDoc->GetContent());
  if (!bridge) {
    // This isn't an OOP iframe.
    return nullptr;
  }
  return bridge->GetEmbeddedDocAccessible();
}

already_AddRefed<IAccessible> MsaaAccessible::GetIAccessibleFor(
    const VARIANT& aVarChild, bool* aIsDefunct) {
  if (aVarChild.vt != VT_I4) return nullptr;

  VARIANT varChild = aVarChild;

  MOZ_ASSERT(aIsDefunct);
  *aIsDefunct = false;

  RefPtr<IAccessible> result;

  if (!mAcc) {
    *aIsDefunct = true;
    return nullptr;
  }

  AccessibleWrap* localAcc = static_cast<AccessibleWrap*>(mAcc->AsLocal());
  if (varChild.lVal == CHILDID_SELF) {
    MOZ_ASSERT(localAcc ||
               StaticPrefs::accessibility_cache_enabled_AtStartup());
    result = this;
    return result.forget();
  }

  if (varChild.ulVal != GetExistingID() && nsAccUtils::MustPrune(mAcc)) {
    // This accessible should have no subtree in platform, return null for its
    // children.
    return nullptr;
  }

  // If the MSAA ID is not a chrome id then we already know that we won't
  // find it here and should look remotely instead. This handles the case when
  // accessible is part of the chrome process and is part of the xul browser
  // window and the child id points in the content documents.
  // Bug 1422674: We must only handle remote ids here (< 0), not child indices.
  // Child indices (> 0) are handled below for both local and remote children.
  if (XRE_IsParentProcess() && localAcc && varChild.lVal < 0 &&
      !sIDGen.IsChromeID(varChild.lVal)) {
    MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup());
    if (!localAcc->IsRootForHWND()) {
      // Bug 1422201, 1424657: accChild with a remote id is only valid on the
      // root accessible for an HWND.
      // Otherwise, we might return remote accessibles which aren't descendants
      // of this accessible. This would confuse clients which use accChild to
      // check whether something is a descendant of a document.
      return nullptr;
    }
    return GetRemoteIAccessibleFor(varChild);
  }

  if (varChild.lVal > 0) {
    // Gecko child indices are 0-based in contrast to indices used in MSAA.
    if (varChild.lVal == 1) {
      RefPtr<IDispatch> disp = MaybeGetOOPIframeDocCOMProxy(localAcc);
      if (disp) {
        // We need to return an IAccessible here, but
        // MaybeGetOOPIframeDocCOMProxy returns an IDispatch. We know this
        // IDispatch is also an IAccessible, so this code is safe, albeit ugly.
        disp.forget((void**)getter_AddRefs(result));
        return result.forget();
      }
    }
    Accessible* xpAcc = mAcc->ChildAt(varChild.lVal - 1);
    if (!xpAcc) {
      return nullptr;
    }
    MOZ_ASSERT(xpAcc->IsRemote() || !xpAcc->AsLocal()->IsDefunct(),
               "Shouldn't get a defunct child");
    if (!StaticPrefs::accessibility_cache_enabled_AtStartup() && localAcc &&
        xpAcc->IsRemote()) {
      // We're crossing from a local OuterDoc to a remote document and caching
      // isn't enabled. We must return the COM proxy.
      MOZ_ASSERT(localAcc->IsOuterDoc());
      xpAcc->AsRemote()->GetCOMInterface(getter_AddRefs(result));
      return result.forget();
    }
    result = MsaaAccessible::GetFrom(xpAcc);
    return result.forget();
  }

  // If lVal negative then it is treated as child ID and we should look for
  // accessible through whole accessible subtree including subdocuments.
  // First see about the case that both this accessible and the target one are
  // RemoteAccessibles and the cache is disabled.
  if (!localAcc && !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    DocAccessibleParent* proxyDoc = mAcc->AsRemote()->Document();
    RefPtr<IDispatch> disp = GetProxiedAccessibleInSubtree(proxyDoc, varChild);
    if (!disp) {
      return nullptr;
    }

    MOZ_ASSERT(mscom::IsProxy(disp));
    DebugOnly<HRESULT> hr =
        disp->QueryInterface(IID_IAccessible, getter_AddRefs(result));
    MOZ_ASSERT(SUCCEEDED(hr));
    return result.forget();
  }

  Accessible* doc = nullptr;
  Accessible* child = nullptr;
  auto id = static_cast<uint32_t>(varChild.lVal);
  if (localAcc) {
    DocAccessible* localDoc = localAcc->Document();
    doc = localDoc;
    child = GetAccessibleInSubtree(localDoc, id);
    if (!child && StaticPrefs::accessibility_cache_enabled_AtStartup()) {
      // Search remote documents which are descendants of this local document.
      const auto remoteDocs = DocManager::TopLevelRemoteDocs();
      if (!remoteDocs) {
        return nullptr;
      }
      for (DocAccessibleParent* remoteDoc : *remoteDocs) {
        LocalAccessible* outerDoc = remoteDoc->OuterDocOfRemoteBrowser();
        if (!outerDoc || outerDoc->Document() != localDoc) {
          // The OuterDoc isn't inside our document, so this isn't a
          // descendant.
          continue;
        }
        child = GetAccessibleInSubtree(remoteDoc, id);
        if (child) {
          break;
        }
      }
    }
  } else if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    DocAccessibleParent* remoteDoc = mAcc->AsRemote()->Document();
    doc = remoteDoc;
    child = GetAccessibleInSubtree(remoteDoc, id);
  }
  if (!child) {
    return nullptr;
  }

  MOZ_ASSERT(child->IsLocal() ||
             StaticPrefs::accessibility_cache_enabled_AtStartup());
  MOZ_ASSERT(child->IsRemote() || !child->AsLocal()->IsDefunct(),
             "Shouldn't get a defunct child");
  // If this method is being called on the document we searched, we can just
  // return child.
  if (mAcc == doc) {
    result = MsaaAccessible::GetFrom(child);
    return result.forget();
  }

  // Otherwise, this method was called on a descendant, so we searched an
  // ancestor. We must check whether child is really a descendant. This is used
  // for ARIA documents and popups.
  Accessible* parent = child;
  while (parent && parent != doc) {
    if (parent == mAcc) {
      result = MsaaAccessible::GetFrom(child);
      return result.forget();
    }

    parent = parent->Parent();
  }

  return nullptr;
}

/**
 * Visit DocAccessibleParent descendants of `aBrowser` that are at the top
 * level of their content process.
 * That is, IsTopLevelInContentProcess() will be true for each visited actor.
 * Each visited actor will be an embedded document in a different content
 * process to its embedder.
 * The DocAccessibleParent for `aBrowser` itself is excluded.
 * `aCallback` will be called for each DocAccessibleParent.
 * The callback should return true to continue traversal, false to cease.
 */
template <typename Callback>
static bool VisitDocAccessibleParentDescendantsAtTopLevelInContentProcess(
    dom::BrowserParent* aBrowser, Callback aCallback) {
  // We can't use BrowserBridgeParent::VisitAllDescendants because it doesn't
  // provide a way to stop the search.
  const auto& bridges = aBrowser->ManagedPBrowserBridgeParent();
  return std::all_of(bridges.cbegin(), bridges.cend(), [&](const auto& key) {
    auto* bridge = static_cast<dom::BrowserBridgeParent*>(key);
    dom::BrowserParent* childBrowser = bridge->GetBrowserParent();
    DocAccessibleParent* childDocAcc = childBrowser->GetTopLevelDocAccessible();
    if (!childDocAcc || childDocAcc->IsShutdown()) {
      return true;
    }
    if (!aCallback(childDocAcc)) {
      return false;  // Stop traversal.
    }
    if (!VisitDocAccessibleParentDescendantsAtTopLevelInContentProcess(
            childBrowser, aCallback)) {
      return false;  // Stop traversal.
    }
    return true;  // Continue traversal.
  });
}

already_AddRefed<IAccessible> MsaaAccessible::GetRemoteIAccessibleFor(
    const VARIANT& aVarChild) {
  a11y::RootAccessible* root = LocalAcc()->RootAccessible();
  const nsTArray<DocAccessibleParent*>* remoteDocs =
      DocManager::TopLevelRemoteDocs();
  if (!remoteDocs) {
    return nullptr;
  }

  RefPtr<IAccessible> result;

  // We intentionally leave the call to remoteDocs->Length() inside the loop
  // condition because it is possible for reentry to occur in the call to
  // GetProxiedAccessibleInSubtree() such that remoteDocs->Length() is mutated.
  for (size_t i = 0; i < remoteDocs->Length(); i++) {
    DocAccessibleParent* topRemoteDoc = remoteDocs->ElementAt(i);

    LocalAccessible* outerDoc = topRemoteDoc->OuterDocOfRemoteBrowser();
    if (!outerDoc) {
      continue;
    }

    if (outerDoc->RootAccessible() != root) {
      continue;
    }

    RefPtr<IDispatch> disp;
    auto checkDoc = [&aVarChild,
                     &disp](DocAccessibleParent* aRemoteDoc) -> bool {
      uint32_t remoteDocMsaaId =
          MsaaAccessible::GetFrom(aRemoteDoc)->GetExistingID();
      if (!sIDGen.IsSameContentProcessFor(aVarChild.lVal, remoteDocMsaaId)) {
        return true;  // Continue the search.
      }
      if ((disp = GetProxiedAccessibleInSubtree(aRemoteDoc, aVarChild))) {
        return false;  // Found it! Stop traversal!
      }
      return true;  // Continue the search.
    };

    // Check the top level document for this id.
    checkDoc(topRemoteDoc);
    if (!disp) {
      // The top level document doesn't contain this id. Recursively check any
      // out-of-process iframe documents it embeds.
      VisitDocAccessibleParentDescendantsAtTopLevelInContentProcess(
          static_cast<dom::BrowserParent*>(topRemoteDoc->Manager()), checkDoc);
    }

    if (!disp) {
      continue;
    }

    DebugOnly<HRESULT> hr =
        disp->QueryInterface(IID_IAccessible, getter_AddRefs(result));
    // QI can fail on rare occasions if the LocalAccessible dies after we
    // fetched disp but before we QI.
    NS_WARNING_ASSERTION(SUCCEEDED(hr), "QI failed on remote IDispatch");
    return result.forget();
  }

  return nullptr;
}

IDispatch* MsaaAccessible::NativeAccessible(Accessible* aAccessible) {
  if (!aAccessible) {
    NS_WARNING("Not passing in an aAccessible");
    return nullptr;
  }

  RefPtr<IDispatch> disp;
  if (!StaticPrefs::accessibility_cache_enabled_AtStartup() &&
      aAccessible->IsRemote()) {
    // This is a RemoteAccessible and caching isn't enabled. We must return
    // the COM proxy.
    aAccessible->AsRemote()->GetCOMInterface(getter_AddRefs(disp));
  } else {
    disp = MsaaAccessible::GetFrom(aAccessible);
  }
  IDispatch* rawDisp;
  disp.forget(&rawDisp);
  return rawDisp;
}

ITypeInfo* MsaaAccessible::GetTI(LCID lcid) {
  if (gTypeInfo) return gTypeInfo;

  ITypeLib* typeLib = nullptr;
  HRESULT hr = LoadRegTypeLib(LIBID_Accessibility, 1, 0, lcid, &typeLib);
  if (FAILED(hr)) return nullptr;

  hr = typeLib->GetTypeInfoOfGuid(IID_IAccessible, &gTypeInfo);
  typeLib->Release();

  if (FAILED(hr)) return nullptr;

  return gTypeInfo;
}

/* static */
MsaaAccessible* MsaaAccessible::GetFrom(Accessible* aAcc) {
  if (RemoteAccessible* remoteAcc = aAcc->AsRemote()) {
    return reinterpret_cast<MsaaAccessible*>(remoteAcc->GetWrapper());
  }
  return static_cast<AccessibleWrap*>(aAcc)->GetMsaa();
}

// IUnknown methods
STDMETHODIMP
MsaaAccessible::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IClientSecurity == iid) {
    // Some code might QI(IID_IClientSecurity) to detect whether or not we are
    // a proxy. Right now that can potentially happen off the main thread, so we
    // look for this condition immediately so that we don't trigger other code
    // that might not be thread-safe.
    return E_NOINTERFACE;
  }

  // These interfaces are always available. We can support querying to them
  // even if the Accessible is dead.
  if (IID_IUnknown == iid) {
    *ppv = static_cast<IAccessible*>(this);
  } else if (IID_IDispatch == iid || IID_IAccessible == iid) {
    *ppv = static_cast<IAccessible*>(this);
  } else if (IID_IServiceProvider == iid) {
    *ppv = new ServiceProvider(this);
  } else {
    HRESULT hr = ia2Accessible::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) {
      return hr;
    }
  }
  if (*ppv) {
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  // For interfaces below this point, we have to query the Accessible to
  // determine if they are available.
  if (!mAcc) {
    // mscom::Interceptor (and maybe other callers) expects either S_OK or
    // E_NOINTERFACE, so don't return CO_E_OBJNOTCONNECTED like we normally
    // would for a dead object.
    return E_NOINTERFACE;
  }
  AccessibleWrap* localAcc = LocalAcc();
  if (IID_IEnumVARIANT == iid && localAcc) {
    if (
        // Don't support this interface for leaf elements.
        !localAcc->HasChildren() || nsAccUtils::MustPrune(localAcc) ||
        // We also don't support this for local OuterDocAccessibles with remote
        // document children when the cache is disabled. Handling this is tricky
        // and there's no benefit to IEnumVARIANT when there's only one child
        // anyway.
        (localAcc->IsOuterDoc() &&
         !StaticPrefs::accessibility_cache_enabled_AtStartup() &&
         localAcc->FirstChild()->IsRemote())) {
      return E_NOINTERFACE;
    }
    *ppv = static_cast<IEnumVARIANT*>(new ChildrenEnumVariant(this));
  } else if (IID_ISimpleDOMNode == iid && localAcc) {
    if (!localAcc->HasOwnContent() && !localAcc->IsDoc()) {
      return E_NOINTERFACE;
    }

    *ppv = static_cast<ISimpleDOMNode*>(new sdnAccessible(WrapNotNull(this)));
  } else if (iid == IID_ISimpleDOMText && localAcc && localAcc->IsTextLeaf()) {
    statistics::ISimpleDOMUsed();
    *ppv = static_cast<ISimpleDOMText*>(new sdnTextAccessible(this));
    static_cast<IUnknown*>(*ppv)->AddRef();
    return S_OK;
  }

  if (!*ppv && localAcc) {
    HRESULT hr = ia2AccessibleComponent::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) return hr;
  }

  if (!*ppv && localAcc) {
    HRESULT hr = ia2AccessibleHyperlink::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) return hr;
  }

  if (!*ppv && localAcc) {
    HRESULT hr = ia2AccessibleValue::QueryInterface(iid, ppv);
    if (SUCCEEDED(hr)) return hr;
  }

  if (!*ppv && iid == IID_IGeckoCustom) {
    MOZ_ASSERT(XRE_IsContentProcess() &&
               !StaticPrefs::accessibility_cache_enabled_AtStartup());
    RefPtr<GeckoCustom> gkCrap = new GeckoCustom(this);
    gkCrap.forget(ppv);
    return S_OK;
  }

  if (nullptr == *ppv) return E_NOINTERFACE;

  (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
  return S_OK;
}

// IAccessible methods

STDMETHODIMP
MsaaAccessible::get_accParent(IDispatch __RPC_FAR* __RPC_FAR* ppdispParent) {
  if (!ppdispParent) return E_INVALIDARG;

  *ppdispParent = nullptr;

  if (!mAcc) {
    return CO_E_OBJNOTCONNECTED;
  }

  Accessible* xpParentAcc = mAcc->Parent();
  if (!xpParentAcc) return S_FALSE;
  MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup() ||
             xpParentAcc->IsLocal());

  *ppdispParent = NativeAccessible(xpParentAcc);
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::get_accChildCount(long __RPC_FAR* pcountChildren) {
  if (!pcountChildren) return E_INVALIDARG;

  *pcountChildren = 0;

  if (!mAcc) return CO_E_OBJNOTCONNECTED;

  if (nsAccUtils::MustPrune(mAcc)) return S_OK;

  *pcountChildren = mAcc->ChildCount();
  if (*pcountChildren == 0 && RefPtr{MaybeGetOOPIframeDocCOMProxy(mAcc)}) {
    *pcountChildren = 1;
  }
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::get_accChild(
    /* [in] */ VARIANT varChild,
    /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispChild) {
  if (!ppdispChild) return E_INVALIDARG;

  *ppdispChild = nullptr;
  if (!mAcc) return CO_E_OBJNOTCONNECTED;

  // IAccessible::accChild is used to return this accessible or child accessible
  // at the given index or to get an accessible by child ID in the case of
  // document accessible.
  // The getting an accessible by child ID is used by
  // AccessibleObjectFromEvent() called by AT when AT handles our MSAA event.
  bool isDefunct = false;
  RefPtr<IAccessible> child = GetIAccessibleFor(varChild, &isDefunct);
  if (!child) {
    return E_INVALIDARG;
  }

  if (isDefunct) {
    return CO_E_OBJNOTCONNECTED;
  }

  child.forget(ppdispChild);
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::get_accName(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszName) {
  if (!pszName || varChild.vt != VT_I4) return E_INVALIDARG;

  *pszName = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accName(kVarChildIdSelf, pszName);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  nsAutoString name;
  LocalAcc()->Name(name);

  // The name was not provided, e.g. no alt attribute for an image. A screen
  // reader may choose to invent its own accessible name, e.g. from an image src
  // attribute. Refer to eNoNameOnPurpose return value.
  if (name.IsVoid()) return S_FALSE;

  *pszName = ::SysAllocStringLen(name.get(), name.Length());
  if (!*pszName) return E_OUTOFMEMORY;
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::get_accValue(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszValue) {
  if (!pszValue) return E_INVALIDARG;

  *pszValue = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accValue(kVarChildIdSelf, pszValue);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  nsAutoString value;
  LocalAcc()->Value(value);

  // See bug 438784: need to expose URL on doc's value attribute. For this,
  // reverting part of fix for bug 425693 to make this MSAA method behave
  // IAccessible2-style.
  if (value.IsEmpty()) return S_FALSE;

  *pszValue = ::SysAllocStringLen(value.get(), value.Length());
  if (!*pszValue) return E_OUTOFMEMORY;
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::get_accDescription(VARIANT varChild,
                                   BSTR __RPC_FAR* pszDescription) {
  if (!pszDescription) return E_INVALIDARG;

  *pszDescription = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accDescription(kVarChildIdSelf, pszDescription);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  nsAutoString description;
  LocalAcc()->Description(description);

  *pszDescription =
      ::SysAllocStringLen(description.get(), description.Length());
  return *pszDescription ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
MsaaAccessible::get_accRole(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarRole) {
  if (!pvarRole) return E_INVALIDARG;

  VariantInit(pvarRole);

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accRole(kVarChildIdSelf, pvarRole);
  }

  a11y::role geckoRole;
#ifdef DEBUG
  if (mAcc->IsLocal()) {
    NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(mAcc->AsLocal()),
                 "Does not support Text when it should");
  }
#endif
  geckoRole = mAcc->Role();

  uint32_t msaaRole = 0;

#define ROLE(_geckoRole, stringRole, atkRole, macRole, macSubrole, _msaaRole, \
             ia2Role, androidClass, nameRule)                                 \
  case roles::_geckoRole:                                                     \
    msaaRole = _msaaRole;                                                     \
    break;

  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  }

#undef ROLE

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call
  // the MSAA role a ROLE_OUTLINEITEM for consistency and compatibility. We need
  // this because ARIA has a role of "row" for both grid and treegrid
  if (geckoRole == roles::ROW) {
    Accessible* xpParent = mAcc->Parent();
    if (xpParent && xpParent->Role() == roles::TREE_TABLE)
      msaaRole = ROLE_SYSTEM_OUTLINEITEM;
  }

  // -- Try enumerated role
  if (msaaRole != USE_ROLE_STRING) {
    pvarRole->vt = VT_I4;
    pvarRole->lVal = msaaRole;  // Normal enumerated role
    return S_OK;
  }

  // -- Try BSTR role
  // Could not map to known enumerated MSAA role like ROLE_BUTTON
  // Use BSTR role to expose role attribute or tag name + namespace
  LocalAccessible* localAcc = mAcc->AsLocal();
  if (!localAcc) {
    return E_FAIL;
  }
  nsIContent* content = localAcc->GetContent();
  if (!content) return E_FAIL;

  if (content->IsElement()) {
    nsAutoString roleString;
    // Try the role attribute.
    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::role,
                                  roleString);

    if (roleString.IsEmpty()) {
      // No role attribute (or it is an empty string).
      // Use the tag name.
      dom::Document* document = content->GetUncomposedDoc();
      if (!document) return E_FAIL;

      dom::NodeInfo* nodeInfo = content->NodeInfo();
      nodeInfo->GetName(roleString);

      // Only append name space if different from that of current document.
      if (!nodeInfo->NamespaceEquals(document->GetDefaultNamespaceID())) {
        nsAutoString nameSpaceURI;
        nodeInfo->GetNamespaceURI(nameSpaceURI);
        roleString += u", "_ns + nameSpaceURI;
      }
    }

    if (!roleString.IsEmpty()) {
      pvarRole->vt = VT_BSTR;
      pvarRole->bstrVal = ::SysAllocString(roleString.get());
      return S_OK;
    }
  }

  return E_FAIL;
}

STDMETHODIMP
MsaaAccessible::get_accState(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarState) {
  if (!pvarState) return E_INVALIDARG;

  VariantInit(pvarState);
  pvarState->vt = VT_I4;
  pvarState->lVal = 0;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accState(kVarChildIdSelf, pvarState);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  // MSAA only has 31 states and the lowest 31 bits of our state bit mask
  // are the same states as MSAA.
  // Note: we map the following Gecko states to different MSAA states:
  //   REQUIRED -> ALERT_LOW
  //   ALERT -> ALERT_MEDIUM
  //   INVALID -> ALERT_HIGH
  //   CHECKABLE -> MARQUEED

  uint64_t state = LocalAcc()->State();

  uint32_t msaaState = 0;
  nsAccUtils::To32States(state, &msaaState, nullptr);
  pvarState->lVal = msaaState;
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::get_accHelp(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszHelp) {
  if (!pszHelp) return E_INVALIDARG;

  *pszHelp = nullptr;
  return S_FALSE;
}

STDMETHODIMP
MsaaAccessible::get_accHelpTopic(
    /* [out] */ BSTR __RPC_FAR* pszHelpFile,
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ long __RPC_FAR* pidTopic) {
  if (!pszHelpFile || !pidTopic) return E_INVALIDARG;

  *pszHelpFile = nullptr;
  *pidTopic = 0;
  return S_FALSE;
}

STDMETHODIMP
MsaaAccessible::get_accKeyboardShortcut(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszKeyboardShortcut) {
  if (!pszKeyboardShortcut) return E_INVALIDARG;
  *pszKeyboardShortcut = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accKeyboardShortcut(kVarChildIdSelf,
                                               pszKeyboardShortcut);
  }

  LocalAccessible* localAcc = LocalAcc();
  if (!localAcc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }
  KeyBinding keyBinding = localAcc->AccessKey();
  if (keyBinding.IsEmpty()) keyBinding = localAcc->KeyboardShortcut();

  nsAutoString shortcut;
  keyBinding.ToString(shortcut);

  *pszKeyboardShortcut = ::SysAllocStringLen(shortcut.get(), shortcut.Length());
  return *pszKeyboardShortcut ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
MsaaAccessible::get_accFocus(
    /* [retval][out] */ VARIANT __RPC_FAR* pvarChild) {
  if (!pvarChild) return E_INVALIDARG;

  VariantInit(pvarChild);

  // clang-format off
  // VT_EMPTY:    None. This object does not have the keyboard focus itself
  //              and does not contain a child that has the keyboard focus.
  // VT_I4:       lVal is CHILDID_SELF. The object itself has the keyboard focus.
  // VT_I4:       lVal contains the child ID of the child element with the keyboard focus.
  // VT_DISPATCH: pdispVal member is the address of the IDispatch interface
  //              for the child object with the keyboard focus.
  // clang-format on
  if (!mAcc) {
    return CO_E_OBJNOTCONNECTED;
  }
  LocalAccessible* localAcc = LocalAcc();
  if (!localAcc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  // Return the current IAccessible child that has focus
  LocalAccessible* focusedAccessible = localAcc->FocusedChild();

  if (focusedAccessible == localAcc) {
    pvarChild->vt = VT_I4;
    pvarChild->lVal = CHILDID_SELF;
  } else if (focusedAccessible) {
    pvarChild->vt = VT_DISPATCH;
    pvarChild->pdispVal = NativeAccessible(focusedAccessible);
  } else {
    pvarChild->vt = VT_EMPTY;  // No focus or focus is not a child
  }

  return S_OK;
}

/**
 * This helper class implements IEnumVARIANT for a nsTArray containing
 * accessible objects.
 */
class AccessibleEnumerator final : public IEnumVARIANT {
 public:
  explicit AccessibleEnumerator(const nsTArray<LocalAccessible*>& aArray)
      : mArray(aArray.Clone()), mCurIndex(0) {}
  AccessibleEnumerator(const AccessibleEnumerator& toCopy)
      : mArray(toCopy.mArray.Clone()), mCurIndex(toCopy.mCurIndex) {}
  ~AccessibleEnumerator() {}

  // IUnknown
  DECL_IUNKNOWN

  // IEnumVARIANT
  STDMETHODIMP Next(unsigned long celt, VARIANT FAR* rgvar,
                    unsigned long FAR* pceltFetched);
  STDMETHODIMP Skip(unsigned long celt);
  STDMETHODIMP Reset() {
    mCurIndex = 0;
    return S_OK;
  }
  STDMETHODIMP Clone(IEnumVARIANT FAR* FAR* ppenum);

 private:
  nsTArray<LocalAccessible*> mArray;
  uint32_t mCurIndex;
};

STDMETHODIMP
AccessibleEnumerator::QueryInterface(REFIID iid, void** ppvObject) {
  if (iid == IID_IEnumVARIANT) {
    *ppvObject = static_cast<IEnumVARIANT*>(this);
    AddRef();
    return S_OK;
  }
  if (iid == IID_IUnknown) {
    *ppvObject = static_cast<IUnknown*>(this);
    AddRef();
    return S_OK;
  }

  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

STDMETHODIMP
AccessibleEnumerator::Next(unsigned long celt, VARIANT FAR* rgvar,
                           unsigned long FAR* pceltFetched) {
  uint32_t length = mArray.Length();
  HRESULT hr = S_OK;

  // Can't get more elements than there are...
  if (celt > length - mCurIndex) {
    hr = S_FALSE;
    celt = length - mCurIndex;
  }

  // Copy the elements of the array into rgvar.
  for (uint32_t i = 0; i < celt; ++i, ++mCurIndex) {
    rgvar[i].vt = VT_DISPATCH;
    rgvar[i].pdispVal = MsaaAccessible::NativeAccessible(mArray[mCurIndex]);
  }

  if (pceltFetched) *pceltFetched = celt;

  return hr;
}

STDMETHODIMP
AccessibleEnumerator::Clone(IEnumVARIANT FAR* FAR* ppenum) {
  *ppenum = new AccessibleEnumerator(*this);
  NS_ADDREF(*ppenum);
  return S_OK;
}

STDMETHODIMP
AccessibleEnumerator::Skip(unsigned long celt) {
  uint32_t length = mArray.Length();
  // Check if we can skip the requested number of elements
  if (celt > length - mCurIndex) {
    mCurIndex = length;
    return S_FALSE;
  }
  mCurIndex += celt;
  return S_OK;
}

/**
 * This method is called when a client wants to know which children of a node
 *  are selected. Note that this method can only find selected children for
 *  accessible object which implement SelectAccessible.
 *
 * The VARIANT return value arguement is expected to either contain a single
 * IAccessible or an IEnumVARIANT of IAccessibles. We return the IEnumVARIANT
 * regardless of the number of children selected, unless there are none selected
 * in which case we return an empty VARIANT.
 *
 * We get the selected options from the select's accessible object and wrap
 *  those in an AccessibleEnumerator which we then put in the return VARIANT.
 *
 * returns a VT_EMPTY VARIANT if:
 *  - there are no selected children for this object
 *  - the object is not the type that can have children selected
 */
STDMETHODIMP
MsaaAccessible::get_accSelection(VARIANT __RPC_FAR* pvarChildren) {
  if (!pvarChildren) return E_INVALIDARG;

  VariantInit(pvarChildren);
  pvarChildren->vt = VT_EMPTY;

  if (!mAcc) {
    return CO_E_OBJNOTCONNECTED;
  }
  LocalAccessible* localAcc = LocalAcc();
  if (!localAcc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  if (!localAcc->IsSelect()) {
    return S_OK;
  }

  AutoTArray<LocalAccessible*, 10> selectedItems;
  localAcc->SelectedItems(&selectedItems);
  uint32_t count = selectedItems.Length();
  if (count == 1) {
    pvarChildren->vt = VT_DISPATCH;
    pvarChildren->pdispVal = NativeAccessible(selectedItems[0]);
  } else if (count > 1) {
    RefPtr<AccessibleEnumerator> pEnum =
        new AccessibleEnumerator(selectedItems);
    AssociateCOMObjectForDisconnection(pEnum);
    pvarChildren->vt =
        VT_UNKNOWN;  // this must be VT_UNKNOWN for an IEnumVARIANT
    NS_ADDREF(pvarChildren->punkVal = pEnum);
  }
  // If count == 0, vt is already VT_EMPTY, so there's nothing else to do.

  return S_OK;
}

STDMETHODIMP
MsaaAccessible::get_accDefaultAction(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszDefaultAction) {
  if (!pszDefaultAction) return E_INVALIDARG;

  *pszDefaultAction = nullptr;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->get_accDefaultAction(kVarChildIdSelf, pszDefaultAction);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  nsAutoString defaultAction;
  LocalAcc()->ActionNameAt(0, defaultAction);

  *pszDefaultAction =
      ::SysAllocStringLen(defaultAction.get(), defaultAction.Length());
  return *pszDefaultAction ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
MsaaAccessible::accSelect(
    /* [in] */ long flagsSelect,
    /* [optional][in] */ VARIANT varChild) {
  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accSelect(flagsSelect, kVarChildIdSelf);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  LocalAccessible* localAcc = LocalAcc();
  if (flagsSelect & SELFLAG_TAKEFOCUS) {
    if (XRE_IsContentProcess()) {
      // In this case we might have been invoked while the IPC MessageChannel is
      // waiting on a sync reply. We cannot dispatch additional IPC while that
      // is happening, so we dispatch TakeFocus from the main thread to
      // guarantee that we are outside any IPC.
      nsCOMPtr<nsIRunnable> runnable = mozilla::NewRunnableMethod(
          "LocalAccessible::TakeFocus", localAcc, &LocalAccessible::TakeFocus);
      NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
      return S_OK;
    }
    localAcc->TakeFocus();
    return S_OK;
  }

  if (flagsSelect & SELFLAG_TAKESELECTION) {
    localAcc->TakeSelection();
    return S_OK;
  }

  if (flagsSelect & SELFLAG_ADDSELECTION) {
    localAcc->SetSelected(true);
    return S_OK;
  }

  if (flagsSelect & SELFLAG_REMOVESELECTION) {
    localAcc->SetSelected(false);
    return S_OK;
  }

  return E_FAIL;
}

STDMETHODIMP
MsaaAccessible::accLocation(
    /* [out] */ long __RPC_FAR* pxLeft,
    /* [out] */ long __RPC_FAR* pyTop,
    /* [out] */ long __RPC_FAR* pcxWidth,
    /* [out] */ long __RPC_FAR* pcyHeight,
    /* [optional][in] */ VARIANT varChild) {
  if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight) return E_INVALIDARG;

  *pxLeft = 0;
  *pyTop = 0;
  *pcxWidth = 0;
  *pcyHeight = 0;

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accLocation(pxLeft, pyTop, pcxWidth, pcyHeight,
                                   kVarChildIdSelf);
  }

  LocalAccessible* localAcc = LocalAcc();
  if (!localAcc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  nsIntRect rect = localAcc->Bounds();

  *pxLeft = rect.X();
  *pyTop = rect.Y();
  *pcxWidth = rect.Width();
  *pcyHeight = rect.Height();
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::accNavigate(
    /* [in] */ long navDir,
    /* [optional][in] */ VARIANT varStart,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarEndUpAt) {
  if (!pvarEndUpAt) return E_INVALIDARG;

  VariantInit(pvarEndUpAt);

  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varStart, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accNavigate(navDir, kVarChildIdSelf, pvarEndUpAt);
  }
  // This should never be called on a RemoteAccessible if the cache is disabled.
  MOZ_ASSERT(mAcc->IsLocal() ||
             StaticPrefs::accessibility_cache_enabled_AtStartup());

  Accessible* navAccessible = nullptr;
  Maybe<RelationType> xpRelation;

#define RELATIONTYPE(geckoType, stringType, atkType, msaaType, ia2Type) \
  case msaaType:                                                        \
    xpRelation.emplace(RelationType::geckoType);                        \
    break;

  switch (navDir) {
    case NAVDIR_FIRSTCHILD:
      if (RefPtr<IDispatch> disp = MaybeGetOOPIframeDocCOMProxy(mAcc)) {
        pvarEndUpAt->vt = VT_DISPATCH;
        disp.forget(&pvarEndUpAt->pdispVal);
        return S_OK;
      } else if (!nsAccUtils::MustPrune(mAcc)) {
        navAccessible = mAcc->FirstChild();
      }
      break;
    case NAVDIR_LASTCHILD:
      if (RefPtr<IDispatch> disp = MaybeGetOOPIframeDocCOMProxy(mAcc)) {
        pvarEndUpAt->vt = VT_DISPATCH;
        disp.forget(&pvarEndUpAt->pdispVal);
        return S_OK;
      } else if (!nsAccUtils::MustPrune(mAcc)) {
        navAccessible = mAcc->LastChild();
      }
      break;
    case NAVDIR_NEXT:
      navAccessible = mAcc->NextSibling();
      break;
    case NAVDIR_PREVIOUS:
      navAccessible = mAcc->PrevSibling();
      break;
    case NAVDIR_DOWN:
    case NAVDIR_LEFT:
    case NAVDIR_RIGHT:
    case NAVDIR_UP:
      return E_NOTIMPL;

      // MSAA relationship extensions to accNavigate
#include "RelationTypeMap.h"

    default:
      return E_INVALIDARG;
  }

#undef RELATIONTYPE

  pvarEndUpAt->vt = VT_EMPTY;

  if (xpRelation) {
    if (mAcc->IsRemote()) {
      return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
    }
    Relation rel = mAcc->AsLocal()->RelationByType(*xpRelation);
    navAccessible = rel.Next();
  }

  if (!navAccessible) return E_FAIL;

  pvarEndUpAt->pdispVal = NativeAccessible(navAccessible);
  pvarEndUpAt->vt = VT_DISPATCH;
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::accHitTest(
    /* [in] */ long xLeft,
    /* [in] */ long yTop,
    /* [retval][out] */ VARIANT __RPC_FAR* pvarChild) {
  if (!pvarChild) return E_INVALIDARG;

  VariantInit(pvarChild);

  if (!mAcc) {
    return CO_E_OBJNOTCONNECTED;
  }
  LocalAccessible* localAcc = LocalAcc();
  if (!localAcc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  Accessible* accessible = mAcc->ChildAtPoint(
      xLeft, yTop, Accessible::EWhichChildAtPoint::DirectChild);

  // if we got a child
  if (accessible) {
    // if the child is us
    if (accessible == mAcc) {
      pvarChild->vt = VT_I4;
      pvarChild->lVal = CHILDID_SELF;
    } else {  // its not create a LocalAccessible for it.
      pvarChild->vt = VT_DISPATCH;
      pvarChild->pdispVal = NativeAccessible(accessible);
    }
  } else {
    // no child at that point
    if (RefPtr<IDispatch> disp = MaybeGetOOPIframeDocCOMProxy(mAcc)) {
      // This is an OOP iframe. ChildAtPoint can't traverse inside it. If the
      // coordinates are inside this iframe, return the COM proxy for the
      // OOP document.
      nsIntRect docRect = mAcc->AsLocal()->Bounds();
      if (docRect.Contains(xLeft, yTop)) {
        pvarChild->vt = VT_DISPATCH;
        disp.forget(&pvarChild->pdispVal);
        return S_OK;
      }
    }
    pvarChild->vt = VT_EMPTY;
    return S_FALSE;
  }
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::accDoDefaultAction(
    /* [optional][in] */ VARIANT varChild) {
  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->accDoDefaultAction(kVarChildIdSelf);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  return LocalAcc()->DoAction(0) ? S_OK : E_INVALIDARG;
}

STDMETHODIMP
MsaaAccessible::put_accName(
    /* [optional][in] */ VARIANT varChild,
    /* [in] */ BSTR szName) {
  return E_NOTIMPL;
}

STDMETHODIMP
MsaaAccessible::put_accValue(
    /* [optional][in] */ VARIANT varChild,
    /* [in] */ BSTR szValue) {
  RefPtr<IAccessible> accessible;
  HRESULT hr = ResolveChild(varChild, getter_AddRefs(accessible));
  if (FAILED(hr)) {
    return hr;
  }

  if (accessible) {
    return accessible->put_accValue(kVarChildIdSelf, szValue);
  }
  if (mAcc->IsRemote()) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  HyperTextAccessible* ht = LocalAcc()->AsHyperText();
  if (!ht) {
    return E_NOTIMPL;
  }

  uint32_t length = ::SysStringLen(szValue);
  nsAutoString text(szValue, length);
  ht->ReplaceText(text);
  return S_OK;
}

// IDispatch methods

STDMETHODIMP
MsaaAccessible::GetTypeInfoCount(UINT* pctinfo) {
  if (!pctinfo) return E_INVALIDARG;

  *pctinfo = 1;
  return S_OK;
}

STDMETHODIMP
MsaaAccessible::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
  if (!ppTInfo) return E_INVALIDARG;

  *ppTInfo = nullptr;

  if (iTInfo != 0) return DISP_E_BADINDEX;

  ITypeInfo* typeInfo = GetTI(lcid);
  if (!typeInfo) return E_FAIL;

  typeInfo->AddRef();
  *ppTInfo = typeInfo;

  return S_OK;
}

STDMETHODIMP
MsaaAccessible::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                              LCID lcid, DISPID* rgDispId) {
  ITypeInfo* typeInfo = GetTI(lcid);
  if (!typeInfo) return E_FAIL;

  HRESULT hr = DispGetIDsOfNames(typeInfo, rgszNames, cNames, rgDispId);
  return hr;
}

STDMETHODIMP
MsaaAccessible::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                       DISPPARAMS* pDispParams, VARIANT* pVarResult,
                       EXCEPINFO* pExcepInfo, UINT* puArgErr) {
  ITypeInfo* typeInfo = GetTI(lcid);
  if (!typeInfo) return E_FAIL;

  return typeInfo->Invoke(static_cast<IAccessible*>(this), dispIdMember, wFlags,
                          pDispParams, pVarResult, pExcepInfo, puArgErr);
}
