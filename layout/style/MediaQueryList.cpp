/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implements DOM interface for querying and observing media queries */

#include "mozilla/dom/MediaQueryList.h"
#include "nsPresContext.h"
#include "nsIMediaList.h"
#include "nsCSSParser.h"
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

MediaQueryList::MediaQueryList(nsIDocument *aDocument,
                               const nsAString &aMediaQueryList)
  : mDocument(aDocument),
    mMediaList(new nsMediaList),
    mMatchesValid(false)
{
  PR_INIT_CLIST(this);

  nsCSSParser parser;
  parser.ParseMediaList(aMediaQueryList, nullptr, 0, mMediaList, false);
}

MediaQueryList::~MediaQueryList()
{
  if (mDocument) {
    PR_REMOVE_LINK(this);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaQueryList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaQueryList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallbacks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaQueryList)
  if (tmp->mDocument) {
    PR_REMOVE_LINK(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  }
  tmp->RemoveAllListeners();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(MediaQueryList)

NS_INTERFACE_MAP_BEGIN(MediaQueryList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(MediaQueryList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaQueryList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaQueryList)

void
MediaQueryList::GetMedia(nsAString &aMedia)
{
  mMediaList->GetText(aMedia);
}

bool
MediaQueryList::Matches()
{
  if (!mMatchesValid) {
    NS_ABORT_IF_FALSE(!HasListeners(),
                      "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  return mMatches;
}

void
MediaQueryList::AddListener(MediaQueryListListener& aListener)
{
  if (!HasListeners()) {
    // When we have listeners, the pres context owns a reference to
    // this.  This is a cyclic reference that can only be broken by
    // cycle collection.
    NS_ADDREF_THIS();
  }

  if (!mMatchesValid) {
    NS_ABORT_IF_FALSE(!HasListeners(),
                      "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }

  for (uint32_t i = 0; i < mCallbacks.Length(); ++i) {
    if (aListener == *mCallbacks[i]) {
      // Already registered
      return;
    }
  }

  mCallbacks.AppendElement(&aListener);
  if (!HasListeners()) {
    // Append failed; undo the AddRef above.
    NS_RELEASE_THIS();
  }
}

void
MediaQueryList::RemoveListener(MediaQueryListListener& aListener)
{
  for (uint32_t i = 0; i < mCallbacks.Length(); ++i) {
    if (aListener == *mCallbacks[i]) {
      mCallbacks.RemoveElementAt(i);
      if (!HasListeners()) {
        // See NS_ADDREF_THIS() in AddListener.
        NS_RELEASE_THIS();
      }
      break;
    }
  }
}

void
MediaQueryList::RemoveAllListeners()
{
  bool hadListeners = HasListeners();
  mCallbacks.Clear();
  if (hadListeners) {
    // See NS_ADDREF_THIS() in AddListener.
    NS_RELEASE_THIS();
  }
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
    mDocument->GetParentDocument()->FlushPendingNotifications(Flush_Frames);
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

  mMatches = mMediaList->Matches(presContext, nullptr);
  mMatchesValid = true;
}

void
MediaQueryList::MediumFeaturesChanged(NotifyList &aListenersToNotify)
{
  mMatchesValid = false;

  if (HasListeners()) {
    bool oldMatches = mMatches;
    RecomputeMatches();
    if (mMatches != oldMatches) {
      for (uint32_t i = 0, i_end = mCallbacks.Length(); i != i_end; ++i) {
        HandleChangeData *d = aListenersToNotify.AppendElement();
        if (d) {
          d->mql = this;
          d->callback = mCallbacks[i];
        }
      }
    }
  }
}

nsISupports*
MediaQueryList::GetParentObject() const
{
  return mDocument;
}

JSObject*
MediaQueryList::WrapObject(JSContext* aCx)
{
  return MediaQueryListBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
