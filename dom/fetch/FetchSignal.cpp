/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchSignal.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FetchSignalBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(FetchSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FetchSignal,
                                                  DOMEventTargetHelper)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mController)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FetchSignal,
                                                DOMEventTargetHelper)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mController)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FetchSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FetchSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FetchSignal, DOMEventTargetHelper)

FetchSignal::FetchSignal(FetchController* aController,
                         bool aAborted)
  : DOMEventTargetHelper(aController->GetParentObject())
  , mController(aController)
  , mAborted(aAborted)
{}

JSObject*
FetchSignal::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FetchSignalBinding::Wrap(aCx, this, aGivenProto);
}

bool
FetchSignal::Aborted() const
{
  return mAborted;
}

void
FetchSignal::Abort()
{
  MOZ_ASSERT(!mAborted);
  mAborted = true;

  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  // TODO which kind of event should we dispatch here?

  RefPtr<Event> event =
    Event::Constructor(this, NS_LITERAL_STRING("abort"), init);
  event->SetTrusted(true);

  DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

} // dom namespace
} // mozilla namespace
