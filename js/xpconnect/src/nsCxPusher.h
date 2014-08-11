/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCxPusher_h
#define nsCxPusher_h

#include "jsapi.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"

class nsIScriptContext;

namespace mozilla {

/**
 * Fundamental cx pushing class. All other cx pushing classes are implemented
 * in terms of this class.
 */
class MOZ_STACK_CLASS AutoCxPusher
{
public:
  explicit AutoCxPusher(JSContext *aCx, bool aAllowNull = false);
  ~AutoCxPusher();

  nsIScriptContext* GetScriptContext() { return mScx; }

  // Returns true if this AutoCxPusher performed the push that is currently at
  // the top of the cx stack.
  bool IsStackTop() const;

private:
  mozilla::Maybe<JSAutoRequest> mAutoRequest;
  mozilla::Maybe<JSAutoCompartment> mAutoCompartment;
  nsCOMPtr<nsIScriptContext> mScx;
  uint32_t mStackDepthAfterPush;
#ifdef DEBUG
  JSContext* mPushedContext;
  unsigned mCompartmentDepthOnEntry;
#endif
};

/**
 * Use AutoJSContext when you need a JS context on the stack but don't have one
 * passed as a parameter. AutoJSContext will take care of finding the most
 * appropriate JS context and release it when leaving the stack.
 */
class MOZ_STACK_CLASS AutoJSContext {
public:
  explicit AutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

protected:
  explicit AutoJSContext(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  // We need this Init() method because we can't use delegating constructor for
  // the moment. It is a C++11 feature and we do not require C++11 to be
  // supported to be able to compile Gecko.
  void Init(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  JSContext* mCx;
  Maybe<AutoCxPusher> mPusher;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * Use ThreadsafeAutoJSContext when you want an AutoJSContext but might be
 * running on a worker thread.
 */
class MOZ_STACK_CLASS ThreadsafeAutoJSContext {
public:
  explicit ThreadsafeAutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

private:
  JSContext* mCx; // Used on workers.  Null means mainthread.
  Maybe<JSAutoRequest> mRequest; // Used on workers.
  Maybe<AutoJSContext> mAutoJSContext; // Used on main thread.
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * AutoSafeJSContext is similar to AutoJSContext but will only return the safe
 * JS context. That means it will never call ::GetCurrentJSContext().
 */
class MOZ_STACK_CLASS AutoSafeJSContext : public AutoJSContext {
public:
  explicit AutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
private:
  JSAutoCompartment mAc;
};

/**
 * Like AutoSafeJSContext but can be used safely on worker threads.
 */
class MOZ_STACK_CLASS ThreadsafeAutoSafeJSContext {
public:
  explicit ThreadsafeAutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

private:
  JSContext* mCx; // Used on workers.  Null means mainthread.
  Maybe<JSAutoRequest> mRequest; // Used on workers.
  Maybe<AutoSafeJSContext> mAutoSafeJSContext; // Used on main thread.
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} // namespace mozilla

#endif /* nsCxPusher_h */
