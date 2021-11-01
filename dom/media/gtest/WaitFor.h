/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef WAITFOR_H_
#define WAITFOR_H_

#include "MediaEventSource.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Result.h"
#include "mozilla/SpinEventLoopUntil.h"

namespace mozilla {

/**
 * Waits for an occurrence of aEvent on the current thread (by blocking it,
 * except tasks added to the event loop may run) and returns the event's
 * templated value, if it's non-void.
 *
 * The caller must be wary of eventloop issues, in
 * particular cases where we rely on a stable state runnable, but there is never
 * a task to trigger stable state. In such cases it is the responsibility of the
 * caller to create the needed tasks, as JS would. A noteworthy API that relies
 * on stable state is MediaTrackGraph::GetInstance.
 */
template <typename T>
T WaitFor(MediaEventSource<T>& aEvent) {
  Maybe<T> value;
  MediaEventListener listener = aEvent.Connect(
      AbstractThread::GetCurrent(), [&](T aValue) { value = Some(aValue); });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "WaitFor(MediaEventSource<T>& aEvent)"_ns,
      [&] { return value.isSome(); });
  listener.Disconnect();
  return value.value();
}

/**
 * Specialization of WaitFor<T> for void.
 */
void WaitFor(MediaEventSource<void>& aEvent);

/**
 * Variant of WaitFor that blocks the caller until a MozPromise has either been
 * resolved or rejected.
 */
template <typename R, typename E, bool Exc>
Result<R, E> WaitFor(const RefPtr<MozPromise<R, E, Exc>>& aPromise) {
  Maybe<R> success;
  Maybe<E> error;
  aPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&](R aResult) { success = Some(aResult); },
      [&](E aError) { error = Some(aError); });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "WaitFor(const RefPtr<MozPromise<R, E, Exc>>& aPromise)"_ns,
      [&] { return success.isSome() || error.isSome(); });
  if (success.isSome()) {
    return success.extract();
  }
  return Err(error.extract());
}

/**
 * A variation of WaitFor that takes a callback to be called each time aEvent is
 * raised. Blocks the caller until the callback function returns true.
 */
template <typename T, typename CallbackFunction>
void WaitUntil(MediaEventSource<T>& aEvent, const CallbackFunction& aF) {
  bool done = false;
  MediaEventListener listener =
      aEvent.Connect(AbstractThread::GetCurrent(), [&](T aValue) {
        if (!done) {
          done = aF(aValue);
        }
      });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "WaitUntil(MediaEventSource<T>& aEvent, const CallbackFunction& aF)"_ns,
      [&] { return done; });
  listener.Disconnect();
}

/**
 * Helper that, given that canonicals have just been updated on the current
 * thread, will block its execution until mirrors and their watchers have
 * executed on aTarget.
 */
inline void WaitForMirrors(const RefPtr<nsISerialEventTarget>& aTarget) {
  Unused << WaitFor(InvokeAsync(aTarget, __func__, [] {
    return GenericPromise::CreateAndResolve(true, "WaitForMirrors resolver");
  }));
}

/**
 * Short form of WaitForMirrors that assumes mirrors are on the current thread
 * (like canonicals).
 */
inline void WaitForMirrors() { WaitForMirrors(GetCurrentSerialEventTarget()); }

}  // namespace mozilla

#endif  // WAITFOR_H_
