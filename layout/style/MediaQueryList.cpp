/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
#include "nsCSSParser.h"
#include "nsIDocument.h"

#define ONCHANGE_STRING NS_LITERAL_STRING("change")

namespace mozilla {
namespace dom {

MediaQueryList::MediaQueryList(nsIDocument* aDocument,
                               const nsAString& aMediaQueryList)
  : mDocument(aDocument)
  , mMatchesValid(false)
{
  mMediaList =
    MediaList::Create(aDocument->GetStyleBackendType(), aMediaQueryList);

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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaQueryList)
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
MediaQueryList::AddEventListener(const nsAString& aType,
                                 EventListener* aCallback,
                                 const AddEventListenerOptionsOrBoolean& aOptions,
                                 const dom::Nullable<bool>& aWantsUntrusted,
                                 ErrorResult& aRv)
{
  if (!mMatchesValid) {
    MOZ_ASSERT(!HasListeners(),
               "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  DOMEventTargetHelper::AddEventListener(aType, aCallback, aOptions,
                                         aWantsUntrusted, aRv);
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

  nsIPresShell* shell = mDocument->GetShell();
  if (!shell) {
    // XXXbz What's the right behavior here?  Spec doesn't say.
    return;
  }

  nsPresContext* presContext = shell->GetPresContext();
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

  bool dummy;
  DispatchEvent(event, &dummy);
}

} // namespace dom
} // namespace mozilla
