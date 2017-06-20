/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchConsumer_h
#define mozilla_dom_FetchConsumer_h

class nsIThread;

namespace mozilla {
namespace dom {

namespace workers {
class WorkerPrivate;
class WorkerHolder;
}

template <class Derived> class FetchBody;

// FetchBody is not thread-safe but we need to move it around threads.
// In order to keep it alive all the time, we use a WorkerHolder, if created on
// workers, plus a this consumer.
template <class Derived>
class FetchBodyConsumer final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchBodyConsumer<Derived>)

  static already_AddRefed<FetchBodyConsumer<Derived>>
  Create(FetchBody<Derived>* aBody);

  void
  ReleaseObject();

  FetchBody<Derived>*
  Body() const
  {
    return mBody;
  }

private:
  explicit FetchBodyConsumer(FetchBody<Derived>* aBody);

  ~FetchBodyConsumer();

  void
  AssertIsOnTargetThread() const;

  bool
  RegisterWorkerHolder(workers::WorkerPrivate* aWorkerPrivate);

  nsCOMPtr<nsIThread> mTargetThread;
  RefPtr<FetchBody<Derived>> mBody;

  // Set when consuming the body is attempted on a worker.
  // Unset when consumption is done/aborted.
  // This WorkerHolder keeps alive the consumer via a cycle.
  UniquePtr<workers::WorkerHolder> mWorkerHolder;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FetchConsumer_h
