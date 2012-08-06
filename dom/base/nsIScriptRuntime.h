/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptRuntime_h__
#define nsIScriptRuntime_h__

#include "nsIScriptContext.h"

#define NS_ISCRIPTRUNTIME_IID \
{ 0xfa30d7a8, 0x7f0a, 0x437a, \
  { 0xa1, 0x0c, 0xc2, 0xbe, 0xa3, 0xdb, 0x4f, 0x4b } }

/**
 * A singleton language environment for an application.  Responsible for
 * initializing and cleaning up the global language environment, and a factory
 * for language contexts
 */
class nsIScriptRuntime : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTRUNTIME_IID)

  /* Factory for a new context for this language */
  virtual already_AddRefed<nsIScriptContext> CreateContext() = 0;
  
  /* Memory managment for script objects returned from various
   * nsIScriptContext methods.  These are identical to those in
   * nsIScriptContext, but are useful when a script context is not known.
   */
  virtual nsresult DropScriptObject(void *object) = 0;
  virtual nsresult HoldScriptObject(void *object) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptRuntime, NS_ISCRIPTRUNTIME_IID)

/* helper functions */
nsresult NS_GetJSRuntime(nsIScriptRuntime** aLanguage);

nsresult NS_GetScriptRuntime(const nsAString &aLanguageName,
                             nsIScriptRuntime **aRuntime);

nsresult NS_GetScriptRuntimeByID(PRUint32 aLanguageID,
                                 nsIScriptRuntime **aRuntime);

#endif // nsIScriptRuntime_h__
