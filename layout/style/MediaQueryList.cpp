/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implements DOM interface for querying and observing media queries */

#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/dom/MediaQueryListEvent.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "nsPresContext.h"
#include "nsIDocument.h"

#define ONCHANGE_STRING NS_LITERAL_STRING("change")

namespace mozilla {
namespace dom {

MediaQueryList::MediaQueryList(nsIDocument* aDocument,
                               const nsAString& aMediaQueryList,
                               CallerType aCallerType)
  : DOMEventTargetHelper(aDocument->GetInnerWindow())
  , mDocument(aDocument)
  , mMatches(false)
  , mMatchesValid(false)
{
  mMediaList = MediaList::Create(aMediaQueryList, aCallerType);

  KeepAliveIfHasListenersFor(ONCHANGE_STRING);
}

MediaQueryList::~MediaQueryList()
{}

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaQueryList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaQueryList,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaQueryList,
                                                DOMEventTargetHelper)
  if (tmp->mDocument) {
    tmp->remove();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  }
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaQueryList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaQueryList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaQueryList, DOMEventTargetHelper)

void
MediaQueryList::GetMedia(nsAString &aMedia)
{
  mMediaList->GetText(aMedia);
}

bool
MediaQueryList::Matches()
{
  if (!mMatchesValid) {
    MOZ_ASSERT(!HasListeners(),
               "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  return mMatches;
}

void
MediaQueryList::AddListener(EventListener* aListener, ErrorResult& aRv)
{
  if (!aListener) {
    return;
  }

  AddEventListenerOptionsOrBoolean options;
  options.SetAsBoolean() = false;

  AddEventListener(ONCHANGE_STRING, aListener, options, false, aRv);
}

void
MediaQueryList::EventListenerAdded(nsAtom* aType)
{
  // HasListeners() might still be false if the added thing wasn't a
  // listener we care about.
  if (!mMatchesValid && HasListeners()) {
    RecomputeMatches();
  }

  DOMEventTargetHelper::EventListenerAdded(aType);
}

void
MediaQueryList::RemoveListener(EventListener* aListener, ErrorResult& aRv)
{
  if (!aListener) {
    return;
  }

  EventListenerOptionsOrBoolean options;
  options.SetAsBoolean() = false;

  RemoveEventListener(ONCHANGE_STRING, aListener, options, aRv);
}

bool
MediaQueryList::HasListeners()
{
  return HasListenersFor(ONCHANGE_STRING);
}

void
MediaQueryList::Disconnect()
{
  DisconnectFromOwner();

  IgnoreKeepAliveIfHasListenersFor(ONCHANGE_STRING);
}

void
MediaQueryList::RecomputeMatches()
{
  mMatches = false;

  if (!mDocument) {
    return;
  }

  if (mDocument->GetParentDocument()) {
    // Flush frames on the parent so our prescontext will get
    // recreated as needed.
    mDocument->GetParentDocument()->FlushPendingNotifications(FlushType::Frames);
    // That might have killed our document, so recheck that.
    if (!mDocument) {
      return;
    }
  }

  nsPresContext* presContext = mDocument->GetPresContext();
  if (!presContext) {
    // XXXbz What's the right behavior here?  Spec doesn't say.
    return;
  }

  mMatches = mMediaList->Matches(presContext);
  mMatchesValid = true;
}

nsISupports*
MediaQueryList::GetParentObject() const
{
  return mDocument;
}

JSObject*
MediaQueryList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaQueryListBinding::Wrap(aCx, this, aGivenProto);
}

void
MediaQueryList::MaybeNotify()
{
  mMatchesValid = false;

  if (!HasListeners()) {
    return;
  }

  bool oldMatches = mMatches;
  RecomputeMatches();

  // No need to notify the change.
  if (mMatches == oldMatches) {
    return;
  }

  MediaQueryListEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mMatches = mMatches;
  mMediaList->GetText(init.mMedia);

  RefPtr<MediaQueryListEvent> event =
    MediaQueryListEvent::Constructor(this, ONCHANGE_STRING, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

} // namespace dom
} // namespace mozilla
