/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for managing the script settings object stack defined in webapps */

#ifndef mozilla_dom_ScriptSettings_h
#define mozilla_dom_ScriptSettings_h

#include "nsCxPusher.h"
#include "MainThreadUtils.h"

#include "mozilla/Maybe.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

/*
 * A class that represents a new script entry point.
 */
class AutoEntryScript {
public:
  AutoEntryScript(nsIGlobalObject* aGlobalObject,
                  bool aIsMainThread = NS_IsMainThread(),
                  // Note: aCx is mandatory off-main-thread.
                  JSContext* aCx = nullptr);

private:
  nsCxPusher mCxPusher;
  mozilla::Maybe<JSAutoCompartment> mAc; // This can de-Maybe-fy when mCxPusher
                                         // goes away.
};

/*
 * A class that can be used to force a particular incumbent script on the stack.
 */
class AutoIncumbentScript {
public:
  AutoIncumbentScript(nsIGlobalObject* aGlobalObject);
};

/*
 * A class used for C++ to indicate that existing entry and incumbent scripts
 * should not apply to anything in scope, and that callees should act as if
 * they were invoked "from C++".
 */
class AutoSystemCaller {
public:
  AutoSystemCaller(bool aIsMainThread = NS_IsMainThread());
private:
  nsCxPusher mCxPusher;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScriptSettings_h
