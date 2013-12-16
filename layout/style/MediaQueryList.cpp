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

MediaQueryList::MediaQueryList(nsPresContext *aPresContext,
                               const nsAString &aMediaQueryList)
  : mPresContext(aPresContext),
    mMediaList(new nsMediaList),
    mMatchesValid(false)
{
  PR_INIT_CLIST(this);

  SetIsDOMBinding();

  nsCSSParser parser;
  parser.ParseMediaList(aMediaQueryList, nullptr, 0, mMediaList, false);
}

MediaQueryList::~MediaQueryList()
{
  if (mPresContext) {
    PR_REMOVE_LINK(this);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaQueryList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaQueryList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPresContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallbacks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaQueryList)
  if (tmp->mPresContext) {
    PR_REMOVE_LINK(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mPresContext)
  }
  tmp->RemoveAllListeners();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(MediaQueryList)

NS_INTERFACE_MAP_BEGIN(MediaQueryList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaQueryList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(MediaQueryList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaQueryList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaQueryList)

NS_IMETHODIMP
MediaQueryList::GetMedia(nsAString &aMedia)
{
  mMediaList->GetText(aMedia);
  return NS_OK;
}

NS_IMETHODIMP
MediaQueryList::GetMatches(bool *aMatches)
{
  *aMatches = Matches();
  return NS_OK;
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

NS_IMETHODIMP
MediaQueryList::AddListener(nsIDOMMediaQueryListListener *aListener)
{
  if (!aListener) {
    return NS_OK;
  }

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

  if (!mListeners.Contains(aListener)) {
    mListeners.AppendElement(aListener);
    if (!HasListeners()) {
      // Append failed; undo the AddRef above.
      NS_RELEASE_THIS();
    }
  }
  return NS_OK;
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

  CallbackType callback(&aListener);

  for (uint32_t i = 0; i < mCallbacks.Length(); ++i) {
    CallbackType thisCallback(mCallbacks[i]);
    if (callback == thisCallback) {
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

NS_IMETHODIMP
MediaQueryList::RemoveListener(nsIDOMMediaQueryListListener *aListener)
{
  bool removed = mListeners.RemoveElement(aListener);
  NS_ABORT_IF_FALSE(!mListeners.Contains(aListener),
                    "duplicate occurrence of listeners");

  if (removed && !HasListeners()) {
    // See NS_ADDREF_THIS() in AddListener.
    NS_RELEASE_THIS();
  }

  return NS_OK;
}

void
MediaQueryList::RemoveListener(MediaQueryListListener& aListener)
{
  CallbackType callback(&aListener);

  for (uint32_t i = 0; i < mCallbacks.Length(); ++i) {
    CallbackType thisCallback(mCallbacks[i]);
    if (callback == thisCallback) {
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
  mListeners.Clear();
  mCallbacks.Clear();
  if (hadListeners) {
    // See NS_ADDREF_THIS() in AddListener.
    NS_RELEASE_THIS();
  }
}

void
MediaQueryList::RecomputeMatches()
{
  if (!mPresContext) {
    return;
  }

  mMatches = mMediaList->Matches(mPresContext, nullptr);
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
      for (uint32_t i = 0, i_end = mListeners.Length(); i != i_end; ++i) {
        HandleChangeData *d = aListenersToNotify.AppendElement();
        if (d) {
          d->mql = this;
          d->listener = mListeners[i];
        }
      }
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
  if (!mPresContext) {
    return nullptr;
  }
  return mPresContext->Document();
}

JSObject*
MediaQueryList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MediaQueryListBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
