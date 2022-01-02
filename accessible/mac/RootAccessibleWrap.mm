/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "mozRootAccessible.h"

#include "gfxPlatform.h"
#include "nsCOMPtr.h"
#include "nsObjCExceptions.h"
#include "nsIFrame.h"
#include "nsView.h"
#include "nsIWidget.h"

using namespace mozilla;
using namespace mozilla::a11y;

RootAccessibleWrap::RootAccessibleWrap(dom::Document* aDocument,
                                       PresShell* aPresShell)
    : RootAccessible(aDocument, aPresShell) {}

RootAccessibleWrap::~RootAccessibleWrap() {}

Class RootAccessibleWrap::GetNativeType() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return [mozRootAccessible class];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

void RootAccessibleWrap::GetNativeWidget(void** aOutView) {
  nsIFrame* frame = GetFrame();
  if (frame) {
    nsView* view = frame->GetView();
    if (view) {
      nsIWidget* widget = view->GetWidget();
      if (widget) {
        *aOutView = (void**)widget->GetNativeData(NS_NATIVE_WIDGET);
        MOZ_ASSERT(
            *aOutView || gfxPlatform::IsHeadless(),
            "Couldn't get the native NSView parent we need to connect the "
            "accessibility hierarchy!");
      }
    }
  }
}
