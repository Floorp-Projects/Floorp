/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Dispatcher.h"
#include "mozilla/Move.h"
#include "nsINamed.h"
#include "nsQueryObject.h"

using namespace mozilla;

nsresult
DispatcherTrait::Dispatch(const char* aName,
                          TaskCategory aCategory,
                          already_AddRefed<nsIRunnable>&& aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  if (aName) {
    if (nsCOMPtr<nsINamed> named = do_QueryInterface(runnable)) {
      named->SetName(aName);
    }
  }
  if (NS_IsMainThread()) {
    return NS_DispatchToCurrentThread(runnable.forget());
  } else {
    return NS_DispatchToMainThread(runnable.forget());
  }
}

already_AddRefed<nsIEventTarget>
DispatcherTrait::EventTargetFor(TaskCategory aCategory) const
{
  nsCOMPtr<nsIEventTarget> main = do_GetMainThread();
  return main.forget();
}

namespace {

#define NS_DISPATCHEREVENTTARGET_IID \
{ 0xbf4e36c8, 0x7d04, 0x4ef4, \
  { 0xbb, 0xd8, 0x11, 0x09, 0x0a, 0xdb, 0x4d, 0xf7 } }

class DispatcherEventTarget final : public nsIEventTarget
{
  RefPtr<dom::Dispatcher> mDispatcher;
  TaskCategory mCategory;

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DISPATCHEREVENTTARGET_IID)

  DispatcherEventTarget(dom::Dispatcher* aDispatcher, TaskCategory aCategory)
   : mDispatcher(aDispatcher)
   , mCategory(aCategory)
  {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET

  dom::Dispatcher* Dispatcher() const { return mDispatcher; }

private:
  virtual ~DispatcherEventTarget() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(DispatcherEventTarget, NS_DISPATCHEREVENTTARGET_IID)

} // namespace

NS_IMPL_ISUPPORTS(DispatcherEventTarget, DispatcherEventTarget, nsIEventTarget)

NS_IMETHODIMP
DispatcherEventTarget::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags)
{
  return Dispatch(do_AddRef(aRunnable), aFlags);
}

NS_IMETHODIMP
DispatcherEventTarget::Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags)
{
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mDispatcher->Dispatch(nullptr, mCategory, Move(aRunnable));
}

NS_IMETHODIMP
DispatcherEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DispatcherEventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  *aIsOnCurrentThread = NS_IsMainThread();
  return NS_OK;
}

already_AddRefed<nsIEventTarget>
Dispatcher::CreateEventTargetFor(TaskCategory aCategory)
{
  RefPtr<DispatcherEventTarget> target =
    new DispatcherEventTarget(this, aCategory);
  return target.forget();
}

/* static */ Dispatcher*
Dispatcher::FromEventTarget(nsIEventTarget* aEventTarget)
{
  RefPtr<DispatcherEventTarget> target = do_QueryObject(aEventTarget);
  if (!target) {
    return nullptr;
  }
  return target->Dispatcher();
}
