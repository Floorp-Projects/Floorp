/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserHost.h"

#include "mozilla/dom/CancelContentJSOptionsBinding.h"
#include "mozilla/dom/WindowGlobalParent.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(BrowserHost, nsIRemoteTab)

BrowserHost::BrowserHost(BrowserParent* aParent) : mRoot(aParent) {
  mRoot->SetBrowserHost(this);
}

BrowserHost* BrowserHost::GetFrom(nsIRemoteTab* aRemoteTab) {
  return static_cast<BrowserHost*>(aRemoteTab);
}

mozilla::layers::LayersId BrowserHost::GetLayersId() const {
  return mRoot->GetRenderFrame()->GetLayersId();
}

BrowsingContext* BrowserHost::GetBrowsingContext() const {
  return mRoot->GetBrowsingContext();
}

nsILoadContext* BrowserHost::GetLoadContext() const {
  RefPtr<nsILoadContext> loadContext = mRoot->GetLoadContext();
  return loadContext;
}

a11y::DocAccessibleParent* BrowserHost::GetTopLevelDocAccessible() const {
  return mRoot->GetTopLevelDocAccessible();
}

void BrowserHost::LoadURL(nsIURI* aURI) { mRoot->LoadURL(aURI); }

void BrowserHost::ResumeLoad(uint64_t aPendingSwitchId) {
  mRoot->ResumeLoad(aPendingSwitchId);
}

void BrowserHost::DestroyStart() { mRoot->Destroy(); }

void BrowserHost::DestroyComplete() {
  if (!mRoot) {
    return;
  }
  mRoot->SetOwnerElement(nullptr);
  mRoot->Destroy();
  mRoot = nullptr;
}

bool BrowserHost::Show(const ScreenIntSize& aSize, bool aParentIsActive) {
  return mRoot->Show(aSize, aParentIsActive);
}

void BrowserHost::UpdateDimensions(const nsIntRect& aRect,
                                   const ScreenIntSize& aSize) {
  mRoot->UpdateDimensions(aRect, aSize);
}

/* attribute boolean docShellIsActive; */
NS_IMETHODIMP
BrowserHost::GetDocShellIsActive(bool* aDocShellIsActive) {
  return mRoot->GetDocShellIsActive(aDocShellIsActive);
}

NS_IMETHODIMP
BrowserHost::SetDocShellIsActive(bool aDocShellIsActive) {
  return mRoot->SetDocShellIsActive(aDocShellIsActive);
}

/* attribute boolean renderLayers; */
NS_IMETHODIMP
BrowserHost::GetRenderLayers(bool* aRenderLayers) {
  return mRoot->GetRenderLayers(aRenderLayers);
}

NS_IMETHODIMP
BrowserHost::SetRenderLayers(bool aRenderLayers) {
  return mRoot->SetRenderLayers(aRenderLayers);
}

/* readonly attribute boolean hasLayers; */
NS_IMETHODIMP
BrowserHost::GetHasLayers(bool* aHasLayers) {
  return mRoot->GetHasLayers(aHasLayers);
}

/* void forceRepaint (); */
NS_IMETHODIMP
BrowserHost::ForceRepaint(void) { return mRoot->ForceRepaint(); }

/* void resolutionChanged (); */
NS_IMETHODIMP
BrowserHost::NotifyResolutionChanged(void) {
  return mRoot->NotifyResolutionChanged();
}

/* void deprioritize (); */
NS_IMETHODIMP
BrowserHost::Deprioritize(void) { return mRoot->Deprioritize(); }

/* void preserveLayers (in boolean aPreserveLayers); */
NS_IMETHODIMP
BrowserHost::PreserveLayers(bool aPreserveLayers) {
  return mRoot->PreserveLayers(aPreserveLayers);
}

/* readonly attribute uint64_t tabId; */
NS_IMETHODIMP
BrowserHost::GetTabId(uint64_t* aTabId) { return mRoot->GetTabId(aTabId); }

/* readonly attribute uint64_t contentProcessId; */
NS_IMETHODIMP
BrowserHost::GetContentProcessId(uint64_t* aContentProcessId) {
  return mRoot->GetContentProcessId(aContentProcessId);
}

/* readonly attribute int32_t osPid; */
NS_IMETHODIMP
BrowserHost::GetOsPid(int32_t* aOsPid) { return mRoot->GetOsPid(aOsPid); }

/* readonly attribute boolean hasContentOpener; */
NS_IMETHODIMP
BrowserHost::GetHasContentOpener(bool* aHasContentOpener) {
  return mRoot->GetHasContentOpener(aHasContentOpener);
}

/* readonly attribute boolean hasPresented; */
NS_IMETHODIMP
BrowserHost::GetHasPresented(bool* aHasPresented) {
  return mRoot->GetHasPresented(aHasPresented);
}

NS_IMETHODIMP
BrowserHost::GetWindowGlobalParents(
    nsTArray<RefPtr<WindowGlobalParent>>& aWindowGlobalParents) {
  return mRoot->GetWindowGlobalParents(aWindowGlobalParents);
}

/* void transmitPermissionsForPrincipal (in nsIPrincipal aPrincipal); */
NS_IMETHODIMP
BrowserHost::TransmitPermissionsForPrincipal(nsIPrincipal* aPrincipal) {
  return mRoot->TransmitPermissionsForPrincipal(aPrincipal);
}

/* readonly attribute boolean hasBeforeUnload; */
NS_IMETHODIMP
BrowserHost::GetHasBeforeUnload(bool* aHasBeforeUnload) {
  return mRoot->GetHasBeforeUnload(aHasBeforeUnload);
}

/* readonly attribute Element ownerElement; */
NS_IMETHODIMP
BrowserHost::GetOwnerElement(mozilla::dom::Element** aOwnerElement) {
  *aOwnerElement = mRoot->GetOwnerElement();
  return NS_OK;
}

/* boolean startApzAutoscroll (in float aAnchorX, in float aAnchorY, in nsViewID
 * aScrollId, in uint32_t aPresShellId); */
NS_IMETHODIMP
BrowserHost::StartApzAutoscroll(float aAnchorX, float aAnchorY,
                                nsViewID aScrollId, uint32_t aPresShellId,
                                bool* _retval) {
  return mRoot->StartApzAutoscroll(aAnchorX, aAnchorY, aScrollId, aPresShellId,
                                   _retval);
}

/* void stopApzAutoscroll (in nsViewID aScrollId, in uint32_t aPresShellId); */
NS_IMETHODIMP
BrowserHost::StopApzAutoscroll(nsViewID aScrollId, uint32_t aPresShellId) {
  return mRoot->StopApzAutoscroll(aScrollId, aPresShellId);
}

/* bool saveRecording (in AString aFileName); */
NS_IMETHODIMP
BrowserHost::SaveRecording(const nsAString& aFileName, bool* _retval) {
  return mRoot->SaveRecording(aFileName, _retval);
}

/* Promise getContentBlockingLog (); */
NS_IMETHODIMP
BrowserHost::GetContentBlockingLog(::mozilla::dom::Promise** _retval) {
  return mRoot->GetContentBlockingLog(_retval);
}

NS_IMETHODIMP
BrowserHost::MaybeCancelContentJSExecutionFromScript(
    nsIRemoteTab::NavigationType aNavigationType,
    JS::Handle<JS::Value> aCancelContentJSOptions, JSContext* aCx) {
  return mRoot->MaybeCancelContentJSExecutionFromScript(
      aNavigationType, aCancelContentJSOptions, aCx);
}

}  // namespace dom
}  // namespace mozilla
