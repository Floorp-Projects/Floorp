/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptRuntime_h__
#define nsIScriptRuntime_h__

#include "nsIScriptContext.h"

#define NS_ISCRIPTRUNTIME_IID \
{ 0xb146580f, 0x55f7, 0x4d97, \
  { 0x8a, 0xbb, 0x4a, 0x50, 0xb0, 0xa8, 0x04, 0x97 } }

/**
 * A singleton language environment for an application.  Responsible for
 * initializing and cleaning up the global language environment, and a factory
 * for language contexts
 */
class nsIScriptRuntime : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTRUNTIME_IID)

  /* Parses a "version string" for the language into a bit-mask used by
   * the language implementation.  If the specified version is not supported
   * an error should be returned.  If the specified version is blank, a default
   * version should be assumed
   */
  virtual nsresult ParseVersion(const nsString &aVersionStr, PRUint32 *verFlags) = 0;
  
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
