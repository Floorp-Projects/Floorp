/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "ChildIDThunk.h"
#include "Compatibility.h"
#include "mozilla/mscom/interceptor.h"
#include "nsCoreUtils.h"
#include "nsIXULRuntime.h"
#include "nsWinUtils.h"

using namespace mozilla::a11y;
using namespace mozilla::mscom;

////////////////////////////////////////////////////////////////////////////////
// Constructor/destructor

RootAccessibleWrap::
  RootAccessibleWrap(nsIDocument* aDocument, nsIPresShell* aPresShell) :
  RootAccessible(aDocument, aPresShell)
{
}

RootAccessibleWrap::~RootAccessibleWrap()
{
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

void
RootAccessibleWrap::GetNativeInterface(void** aOutAccessible)
{
  MOZ_ASSERT(aOutAccessible);
  if (!aOutAccessible) {
    return;
  }

  if (mInterceptor &&
      SUCCEEDED(mInterceptor->Resolve(IID_IAccessible, aOutAccessible))) {
    return;
  }

  *aOutAccessible = nullptr;

  RefPtr<IAccessible> rootAccessible;
  RootAccessible::GetNativeInterface((void**)getter_AddRefs(rootAccessible));

  if (!mozilla::BrowserTabsRemoteAutostart() || XRE_IsContentProcess()) {
    // We only need to wrap this interface if our process is non-content e10s
    rootAccessible.forget(aOutAccessible);
    return;
  }

  // Otherwise, we need to wrap that IAccessible with an interceptor
  RefPtr<IInterceptorSink> eventSink(MakeAndAddRef<ChildIDThunk>());

  RefPtr<IAccessible> interceptor;
  HRESULT hr = CreateInterceptor<IAccessible>(
      STAUniquePtr<IAccessible>(rootAccessible.forget().take()), eventSink,
      getter_AddRefs(interceptor));
  if (FAILED(hr)) {
    return;
  }

  RefPtr<IWeakReferenceSource> weakRefSrc;
  hr = interceptor->QueryInterface(IID_IWeakReferenceSource,
                                   (void**)getter_AddRefs(weakRefSrc));
  if (FAILED(hr)) {
    return;
  }

  hr = weakRefSrc->GetWeakReference(getter_AddRefs(mInterceptor));
  if (FAILED(hr)) {
    return;
  }

  interceptor.forget(aOutAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// RootAccessible

void
RootAccessibleWrap::DocumentActivated(DocAccessible* aDocument)
{
  if (Compatibility::IsDolphin() &&
      nsCoreUtils::IsTabDocument(aDocument->DocumentNode())) {
    uint32_t count = mChildDocuments.Length();
    for (uint32_t idx = 0; idx < count; idx++) {
      DocAccessible* childDoc = mChildDocuments[idx];
      HWND childDocHWND = static_cast<HWND>(childDoc->GetNativeWindow());
      if (childDoc != aDocument)
        nsWinUtils::HideNativeWindow(childDocHWND);
      else
        nsWinUtils::ShowNativeWindow(childDocHWND);
    }
  }
}
