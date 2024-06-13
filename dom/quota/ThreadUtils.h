/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_THREADUTILS_H_
#define DOM_QUOTA_THREADUTILS_H_

#include <cstdint>
#include <functional>

enum class nsresult : uint32_t;

namespace mozilla::dom::quota {

/**
 * Add a temporary thread observer and listen for the "AfterProcessNextEvent"
 * notification. Once the notification is received, remove the temporary thread
 * observer and call aCallback.
 * In practice, this calls aCallback immediately after the current thread is
 * done with running and releasing recently popped event from thread's event
 * queue.
 * If called multiple times, all the callbacks will be executed, in the
 * order in which RunAfterProcessingCurrentEvent() was called.
 * Use this method if you need to dispatch the same or some other runnable to
 * another thread in a way which prevents any race conditions (for example
 * unpredictable releases of objects).
 * This method should be used only in existing code which can't be easily
 * converted to use MozPromise which doesn't have the problem with
 * unpredictable releases of objects, see:
 * https://searchfox.org/mozilla-central/rev/4582d908c17fbf7924f5699609fe4a12c28ddc4a/xpcom/threads/MozPromise.h#866
 *
 * Note: Calling this method from a thread pool is not supported since thread
 * pools don't fire the "AfterProcessNextEvent" notification. The method has
 * a diagnostic assertion for that so any calls like that will be caught
 * in builds with enabled diagnostic assertions. The callback will never
 * get executed in other builds, such as release builds. The limitation can
 * be removed completely when thread pool implementation gets support for firing
 * the "AfterProcessNextEvent".
 */
nsresult RunAfterProcessingCurrentEvent(std::function<void()>&& aCallback);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_THREADUTILS_H_
