/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Dispatcher.h"
#include "mozilla/Move.h"

using namespace mozilla;

nsresult
DispatcherTrait::Dispatch(const char* aName,
                          TaskCategory aCategory,
                          already_AddRefed<nsIRunnable>&& aRunnable)
{
  return NS_DispatchToMainThread(Move(aRunnable));
}

already_AddRefed<nsIEventTarget>
DispatcherTrait::CreateEventTarget(const char* aName,
                                   TaskCategory aCategory)
{
  nsCOMPtr<nsIEventTarget> main = do_GetMainThread();
  return main.forget();
}

namespace {

class DispatcherEventTarget final : public nsIEventTarget
{
  RefPtr<dom::Dispatcher> mDispatcher;
  const char* mName;
  TaskCategory mCategory;

public:
  DispatcherEventTarget(dom::Dispatcher* aDispatcher,
                        const char* aName,
                        TaskCategory aCategory)
   : mDispatcher(aDispatcher)
   , mName(aName)
   , mCategory(aCategory)
  {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET

  dom::Dispatcher* Dispatcher() const { return mDispatcher; }

private:
  virtual ~DispatcherEventTarget() {}
};

} // namespace

NS_IMPL_ISUPPORTS(DispatcherEventTarget, nsIEventTarget)

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
  return mDispatcher->Dispatch(mName, mCategory, Move(aRunnable));
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
Dispatcher::CreateEventTarget(const char* aName, TaskCategory aCategory)
{
  RefPtr<DispatcherEventTarget> target =
    new DispatcherEventTarget(this, aName, aCategory);
  return target.forget();
}
