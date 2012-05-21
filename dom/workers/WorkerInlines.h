/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

BEGIN_WORKERS_NAMESPACE

inline
void
SetJSPrivateSafeish(JSObject* aObj, PrivatizableBase* aBase)
{
  JS_SetPrivate(aObj, aBase);
}

template <class Derived>
inline
Derived*
GetJSPrivateSafeish(JSObject* aObj)
{
  return static_cast<Derived*>(
    static_cast<PrivatizableBase*>(JS_GetPrivate(aObj)));
}

END_WORKERS_NAMESPACE
