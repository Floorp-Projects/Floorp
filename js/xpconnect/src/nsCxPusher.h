/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#ifndef nsCxPusher_h
#define nsCxPusher_h

#include "jsapi.h"
#include "mozilla/Util.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
class EventTarget;
}
}

class nsIScriptContext;

namespace mozilla {

/**
 * Fundamental cx pushing class. All other cx pushing classes are implemented
 * in terms of this class.
 */
class MOZ_STACK_CLASS AutoCxPusher
{
public:
  AutoCxPusher(JSContext *aCx, bool aAllowNull = false);
  // XPCShell uses an nsCxPusher, which contains an AutoCxPusher.
  NS_EXPORT ~AutoCxPusher();

  nsIScriptContext* GetScriptContext() { return mScx; }

private:
  mozilla::Maybe<JSAutoRequest> mAutoRequest;
  mozilla::Maybe<JSAutoCompartment> mAutoCompartment;
  nsCOMPtr<nsIScriptContext> mScx;
  bool mScriptIsRunning;
#ifdef DEBUG
  JSContext* mPushedContext;
  unsigned mCompartmentDepthOnEntry;
#endif
};

} /* namespace mozilla */

/**
 * Legacy cx pushing class.
 *
 * This class provides a rather wonky interface, with the following quirks:
 *   * The constructor is a no-op, and callers must explicitly call one of
 *     the Push() methods.
 *   * Null must be pushed with PushNull().
 *   * The cx pusher can be reused multiple times with RePush().
 *
 * This class implements this interface in terms of the much simpler
 * AutoCxPusher class below.
 */
class MOZ_STACK_CLASS nsCxPusher
{
public:
  // This destructor doesn't actually do anything, but it implicitly depends on
  // the Maybe<AutoCxPusher> destructor, which in turn depends on the
  // ~AutoCxPusher destructor. If we stick with the default destructor, the
  // caller needs to be able to link against the AutoCxPusher destructor, which
  // isn't possible with externally-linked consumers like xpcshell. Hoist this
  // work into nsCxPusher.cpp and use NS_EXPORT to make it all work right.
  NS_EXPORT ~nsCxPusher();

  // Returns false if something erroneous happened.
  bool Push(mozilla::dom::EventTarget *aCurrentTarget);
  // If nothing has been pushed to stack, this works like Push.
  // Otherwise if context will change, Pop and Push will be called.
  bool RePush(mozilla::dom::EventTarget *aCurrentTarget);
  // If a null JSContext is passed to Push(), that will cause no
  // push to happen and false to be returned.
  NS_EXPORT_(void) Push(JSContext *cx);
  // Explicitly push a null JSContext on the the stack
  void PushNull();

  // Pop() will be a no-op if Push() or PushNull() fail
  NS_EXPORT_(void) Pop();

  nsIScriptContext* GetCurrentScriptContext() {
    return mPusher.empty() ? nullptr : mPusher.ref().GetScriptContext();
  }

private:
  mozilla::Maybe<mozilla::AutoCxPusher> mPusher;
};

namespace mozilla {

/**
 * Use AutoJSContext when you need a JS context on the stack but don't have one
 * passed as a parameter. AutoJSContext will take care of finding the most
 * appropriate JS context and release it when leaving the stack.
 */
class MOZ_STACK_CLASS AutoJSContext {
public:
  AutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

protected:
  AutoJSContext(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

private:
  // We need this Init() method because we can't use delegating constructor for
  // the moment. It is a C++11 feature and we do not require C++11 to be
  // supported to be able to compile Gecko.
  void Init(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  JSContext* mCx;
  Maybe<AutoCxPusher> mPusher;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * AutoSafeJSContext is similar to AutoJSContext but will only return the safe
 * JS context. That means it will never call ::GetCurrentJSContext().
 */
class MOZ_STACK_CLASS AutoSafeJSContext : public AutoJSContext {
public:
  AutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
};

/**
 * Use AutoPushJSContext when you want to use a specific JSContext that may or
 * may not be already on the stack. This differs from nsCxPusher in that it only
 * pushes in the case that the given cx is not the active cx on the JSContext
 * stack, which avoids an expensive JS_SaveFrameChain in the common case.
 *
 * Most consumers of this should probably just use AutoJSContext. But the goal
 * here is to preserve the existing behavior while ensure proper cx-stack
 * semantics in edge cases where the context being used doesn't match the active
 * context.
 *
 * NB: This will not push a null cx even if aCx is null. Make sure you know what
 * you're doing.
 */
class MOZ_STACK_CLASS AutoPushJSContext {
  Maybe<AutoCxPusher> mPusher;
  JSContext* mCx;

public:
  AutoPushJSContext(JSContext* aCx);
  operator JSContext*() { return mCx; }
};

} // namespace mozilla

#endif /* nsCxPusher_h */
