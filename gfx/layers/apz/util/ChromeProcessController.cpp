/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeProcessController.h"

#include "MainThreadUtils.h"    // for NS_IsMainThread()
#include "base/message_loop.h"  // for MessageLoop
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsView.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::widget;

ChromeProcessController::ChromeProcessController(nsIWidget* aWidget)
  : mWidget(aWidget)
  , mUILoop(MessageLoop::current())
{
  // Otherwise we're initializing mUILoop incorrectly.
  MOZ_ASSERT(NS_IsMainThread());

  mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromeProcessController::InitializeRoot));
}

void
ChromeProcessController::InitializeRoot()
{
  // Create a view-id and set a zero-margin displayport for the root element
  // of the root document in the chrome process. This ensures that the scroll
  // frame for this element gets an APZC, which in turn ensures that all content
  // in the chrome processes is covered by an APZC.
  // The displayport is zero-margin because this element is generally not
  // actually scrollable (if it is, APZC will set proper margins when it's
  // scrolled).
  nsView* view = nsView::GetViewFor(mWidget);
  MOZ_ASSERT(view);
  nsIPresShell* presShell = view->GetPresShell();
  MOZ_ASSERT(presShell);
  MOZ_ASSERT(presShell->GetDocument());
  nsIContent* content = presShell->GetDocument()->GetDocumentElement();
  MOZ_ASSERT(content);
  uint32_t presShellId;
  FrameMetrics::ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(content, &presShellId, &viewId)) {
    nsLayoutUtils::SetDisplayPortMargins(content, presShell, ScreenMargin(), 0,
        nsLayoutUtils::RepaintMode::DoNotRepaint);
  }
}

void
ChromeProcessController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aFrameMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
    return;
  }

  nsCOMPtr<nsIContent> targetContent = nsLayoutUtils::FindContentFor(aFrameMetrics.GetScrollId());
  if (targetContent) {
    FrameMetrics metrics = aFrameMetrics;
    APZCCallbackHelper::UpdateSubFrame(targetContent, metrics);
  }
}

void
ChromeProcessController::PostDelayedTask(Task* aTask, int aDelayMs)
{
  MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
}

void
ChromeProcessController::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                                 const uint32_t& aScrollGeneration)
{
  APZCCallbackHelper::AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
}

void
ChromeProcessController::Destroy()
{
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromeProcessController::Destroy));
    return;
  }

  MOZ_ASSERT(MessageLoop::current() == mUILoop);
  mWidget = nullptr;
}
