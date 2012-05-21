/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsDOMJSUtils_h__
#define nsDOMJSUtils_h__

#include "jsapi.h"
#include "nsIScriptContext.h"

class nsIJSArgArray;

// seems like overkill for just this 1 function - but let's see what else
// falls out first.
inline nsIScriptContext *
GetScriptContextFromJSContext(JSContext *cx)
{
  if (!(::JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS)) {
    return nsnull;
  }

  nsCOMPtr<nsIScriptContext> scx =
    do_QueryInterface(static_cast<nsISupports *>
                                 (::JS_GetContextPrivate(cx)));

  // This will return a pointer to something that's about to be
  // released, but that's ok here.
  return scx;
}

inline nsIScriptContextPrincipal*
GetScriptContextPrincipalFromJSContext(JSContext *cx)
{
  if (!(::JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS)) {
    return nsnull;
  }

  nsCOMPtr<nsIScriptContextPrincipal> scx =
    do_QueryInterface(static_cast<nsISupports *>
                                 (::JS_GetContextPrivate(cx)));

  // This will return a pointer to something that's about to be
  // released, but that's ok here.
  return scx;
}

// A factory function for turning a jsval argv into an nsIArray
// but also supports an effecient way of extracting the original argv.
// Bug 312003 describes why this must be "void *", but argv will be cast to
// jsval* and the args are found at:
//    ((jsval*)aArgv)[0], ..., ((jsval*)aArgv)[aArgc - 1]
// The resulting object will take a copy of the array, and ensure each
// element is rooted.
// Optionally, aArgv may be NULL, in which case the array is allocated and
// rooted, but all items remain NULL.  This presumably means the caller will
// then QI us for nsIJSArgArray, and set our array elements.
nsresult NS_CreateJSArgv(JSContext *aContext, PRUint32 aArgc, void *aArgv,
                         nsIJSArgArray **aArray);

#endif // nsDOMJSUtils_h__
