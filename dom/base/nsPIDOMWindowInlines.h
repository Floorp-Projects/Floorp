/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

template<class T>
nsPIDOMWindowInner*
nsPIDOMWindow<T>::AsInner()
{
  MOZ_ASSERT(IsInnerWindow());
  return reinterpret_cast<nsPIDOMWindowInner*>(this);
}

template<class T>
const nsPIDOMWindowInner*
nsPIDOMWindow<T>::AsInner() const
{
  MOZ_ASSERT(IsInnerWindow());
  return reinterpret_cast<const nsPIDOMWindowInner*>(this);
}

template<class T>
nsPIDOMWindowOuter*
nsPIDOMWindow<T>::AsOuter()
{
  MOZ_ASSERT(IsOuterWindow());
  return reinterpret_cast<nsPIDOMWindowOuter*>(this);
}

template<class T>
const nsPIDOMWindowOuter*
nsPIDOMWindow<T>::AsOuter() const
{
  MOZ_ASSERT(IsOuterWindow());
  return reinterpret_cast<const nsPIDOMWindowOuter*>(this);
}

template <class T>
bool
nsPIDOMWindow<T>::IsLoadingOrRunningTimeout() const
{
  if (IsOuterWindow()) {
    return AsOuter()->GetCurrentInnerWindow()->IsLoadingOrRunningTimeout();
  }
  return !mIsDocumentLoaded || mRunningTimeout;
}

template <class T>
bool
nsPIDOMWindow<T>::IsLoading() const
{
  if (IsOuterWindow()) {
    auto* win = AsOuter()->GetCurrentInnerWindow();

    if (!win) {
      NS_ERROR("No current inner window available!");

      return false;
    }

    return win->IsLoading();
  }

  if (!mOuterWindow) {
    NS_ERROR("IsLoading() called on orphan inner window!");

    return false;
  }

  return !mIsDocumentLoaded;
}

template <class T>
bool
nsPIDOMWindow<T>::IsHandlingResizeEvent() const
{
  if (IsOuterWindow()) {
    auto* win = AsOuter()->GetCurrentInnerWindow();

    if (!win) {
      NS_ERROR("No current inner window available!");

      return false;
    }

    return win->IsHandlingResizeEvent();
  }

  if (!mOuterWindow) {
    NS_ERROR("IsHandlingResizeEvent() called on orphan inner window!");

    return false;
  }

  return mIsHandlingResizeEvent;
}

bool
nsPIDOMWindowInner::IsCurrentInnerWindow() const
{
  return mOuterWindow && mOuterWindow->GetCurrentInnerWindow() == AsInner();
}

bool
nsPIDOMWindowInner::HasActiveDocument()
{
  return IsCurrentInnerWindow() ||
    (mOuterWindow &&
     mOuterWindow->GetCurrentInnerWindow() &&
     mOuterWindow->GetCurrentInnerWindow()->GetDoc() == mDoc);
}

template <class T>
nsIDocShell*
nsPIDOMWindow<T>::GetDocShell() const
{
  if (mOuterWindow) {
    return mOuterWindow->GetDocShell();
  }

  return mDocShell;
}

template <class T>
nsIContent*
nsPIDOMWindow<T>::GetFocusedNode() const
{
  if (IsOuterWindow()) {
    return mInnerWindow ? mInnerWindow->GetFocusedNode() : nullptr;
  }

  return mFocusedNode;
}
