/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleWrap.h"

#include "Compatibility.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "DocAccessibleChild.h"
#include "nsWinUtils.h"
#include "RootAccessible.h"
#include "sdnDocAccessible.h"
#include "Statistics.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// DocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

DocAccessibleWrap::DocAccessibleWrap(dom::Document* aDocument,
                                     PresShell* aPresShell)
    : DocAccessible(aDocument, aPresShell), mHWND(nullptr) {}

DocAccessibleWrap::~DocAccessibleWrap() {}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

void DocAccessibleWrap::Shutdown() {
  // Do window emulation specific shutdown if emulation was started.
  if (nsWinUtils::IsWindowEmulationStarted()) {
    // Destroy window created for root document.
    if (mDocFlags & eTopLevelContentDocInProcess) {
      MOZ_ASSERT(XRE_IsParentProcess());
      HWND hWnd = static_cast<HWND>(mHWND);
      ::RemovePropW(hWnd, kPropNameDocAcc);
      ::DestroyWindow(hWnd);
    }

    mHWND = nullptr;
  }

  DocAccessible::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// DocAccessible public

void* DocAccessibleWrap::GetNativeWindow() const {
  if (XRE_IsContentProcess()) {
    DocAccessibleChild* ipcDoc = IPCDoc();
    if (!ipcDoc) {
      return nullptr;
    }

    return ipcDoc->GetNativeWindowHandle();
  } else if (mHWND) {
    return mHWND;
  }
  return DocAccessible::GetNativeWindow();
}

////////////////////////////////////////////////////////////////////////////////
// DocAccessible protected

void DocAccessibleWrap::DoInitialUpdate() {
  DocAccessible::DoInitialUpdate();

  if (nsWinUtils::IsWindowEmulationStarted()) {
    // Create window for tab document.
    if (mDocFlags & eTopLevelContentDocInProcess) {
      MOZ_ASSERT(XRE_IsParentProcess());
      a11y::RootAccessible* rootDocument = RootAccessible();
      bool isActive = true;
      nsIntRect rect(CW_USEDEFAULT, CW_USEDEFAULT, 0, 0);
      if (Compatibility::IsDolphin()) {
        rect = Bounds();
        nsIntRect rootRect = rootDocument->Bounds();
        rect.MoveToX(rootRect.X() - rect.X());
        rect.MoveByY(-rootRect.Y());

        auto* bc = mDocumentNode->GetBrowsingContext();
        isActive = bc && bc->IsActive();
      }

      RefPtr<DocAccessibleWrap> self(this);
      nsWinUtils::NativeWindowCreateProc onCreate([self](HWND aHwnd) -> void {
        ::SetPropW(aHwnd, kPropNameDocAcc,
                   reinterpret_cast<HANDLE>(self.get()));
      });

      HWND parentWnd = reinterpret_cast<HWND>(rootDocument->GetNativeWindow());
      mHWND = nsWinUtils::CreateNativeWindow(
          kClassNameTabContent, parentWnd, rect.X(), rect.Y(), rect.Width(),
          rect.Height(), isActive, &onCreate);
    } else {
      DocAccessible* parentDocument = ParentDocument();
      if (parentDocument) mHWND = parentDocument->GetNativeWindow();
    }
  }
}
