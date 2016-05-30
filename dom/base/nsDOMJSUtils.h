/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMJSUtils_h__
#define nsDOMJSUtils_h__

#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIJSArgArray;

// seems like overkill for just this 1 function - but let's see what else
// falls out first.
inline nsIScriptContext *
GetScriptContextFromJSContext(JSContext *cx)
{
  if (!(JS::ContextOptionsRef(cx).privateIsNSISupports())) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptContext> scx =
    do_QueryInterface(static_cast<nsISupports *>
                                 (::JS_GetContextPrivate(cx)));

  // This will return a pointer to something that's about to be
  // released, but that's ok here.
  return scx;
}

// A factory function for turning a JS::Value argv into an nsIArray
// but also supports an effecient way of extracting the original argv.
// The resulting object will take a copy of the array, and ensure each
// element is rooted.
// Optionally, aArgv may be nullptr, in which case the array is allocated and
// rooted, but all items remain nullptr.  This presumably means the caller
// will then QI us for nsIJSArgArray, and set our array elements.
nsresult NS_CreateJSArgv(JSContext *aContext, uint32_t aArgc,
                         const JS::Value* aArgv, nsIJSArgArray **aArray);

#endif // nsDOMJSUtils_h__
