/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptRuntime_h__
#define nsIScriptRuntime_h__

#include "nsIScriptContext.h"

#define NS_ISCRIPTRUNTIME_IID \
{ 0x86c6b54a, 0xf067, 0x4878, \
  { 0xb2, 0x77, 0x4e, 0xee, 0x95, 0xda, 0x8f, 0x76 } }

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
  virtual already_AddRefed<nsIScriptContext>
  CreateContext(bool aGCOnDestruction,
                nsIScriptGlobalObject* aGlobalObject) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptRuntime, NS_ISCRIPTRUNTIME_IID)

/* helper functions */
nsresult NS_GetJSRuntime(nsIScriptRuntime** aLanguage);

nsresult NS_GetScriptRuntime(const nsAString &aLanguageName,
                             nsIScriptRuntime **aRuntime);

nsresult NS_GetScriptRuntimeByID(uint32_t aLanguageID,
                                 nsIScriptRuntime **aRuntime);

#endif // nsIScriptRuntime_h__
