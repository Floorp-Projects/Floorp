/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsDOMScriptObjectFactory.h"
#include "xpcexception.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsJSEnvironment.h"
#include "nsJSEventListener.h"
#include "nsGlobalWindow.h"
#include "nsIJSContextStack.h"
#include "nsISupportsPrimitives.h"
#include "nsDOMException.h"
#include "nsCRT.h"
#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

nsDOMScriptObjectFactory::nsDOMScriptObjectFactory() :
  mLoadedAllLanguages(PR_FALSE)
{
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");

  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  }

  nsCOMPtr<nsIExceptionService> xs =
    do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);

  if (xs) {
    xs->RegisterExceptionProvider(this, NS_ERROR_MODULE_DOM);
    xs->RegisterExceptionProvider(this, NS_ERROR_MODULE_DOM_RANGE);
#ifdef MOZ_SVG
    xs->RegisterExceptionProvider(this, NS_ERROR_MODULE_SVG);
#endif
    xs->RegisterExceptionProvider(this, NS_ERROR_MODULE_DOM_XPATH);
    xs->RegisterExceptionProvider(this, NS_ERROR_MODULE_XPCONNECT);
  }
  // And pre-create the javascript language.
  NS_CreateJSRuntime(getter_AddRefs(mLanguageArray[NS_STID_INDEX(nsIProgrammingLanguage::JAVASCRIPT)]));
}

NS_INTERFACE_MAP_BEGIN(nsDOMScriptObjectFactory)
  NS_INTERFACE_MAP_ENTRY(nsIDOMScriptObjectFactory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIExceptionProvider)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMScriptObjectFactory)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMScriptObjectFactory)
NS_IMPL_RELEASE(nsDOMScriptObjectFactory)

/**
 * Notes about language registration (for language other than js):
 * - All language are expected to register (at least) 2 contract IDs
 *    @mozilla.org/script-language;1?id=%d
 *  using the language ID as defined in nsIProgrammingLanguage, and
 *    @mozilla.org/script-language;1?script-type=%s
 *  using the "mime-type" of the script language
 *
 *  Theoretically, a language could register multiple script-type
 *  names, although this is discouraged - each language should have one,
 *  canonical name.
 *
 *  The most common case is that languages are looked up by ID.  For this
 *  reason, we keep an array of languages indexed by this ID - the registry
 *  is only looked the first request for a language ID.
 *  
 *  The registry is looked up and getService called for each query by name.
 *  (As services are cached by CID, multiple contractIDs will still work
 *  correctly)
 **/

NS_IMETHODIMP
nsDOMScriptObjectFactory::GetScriptRuntime(const nsAString &aLanguageName,
                                           nsIScriptRuntime **aLanguage)
{
  // Note that many callers have optimized detection for JS (along with
  // supporting various alternate names for JS), so don't call this.
  // One exception is for the new "script-type" attribute on a node - and
  // there is no need to support backwards compatible names.
  // As JS is the default language, this is still rarely called for JS -
  // only when a node explicitly sets JS - so that is done last.
  nsCAutoString contractid(NS_LITERAL_CSTRING(
                          "@mozilla.org/script-language;1?script-type="));
  // Arbitrarily use utf8 encoding should the name have extended chars
  AppendUTF16toUTF8(aLanguageName, contractid);
  nsresult rv;
  nsCOMPtr<nsIScriptRuntime> lang =
        do_GetService(contractid.get(), &rv);

  if (NS_FAILED(rv)) {
    if (aLanguageName.Equals(NS_LITERAL_STRING("application/javascript")))
      return GetScriptRuntimeByID(nsIProgrammingLanguage::JAVASCRIPT, aLanguage);
    // Not JS and nothing else we know about.
    NS_WARNING("No script language registered for this mime-type");
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }
  // And stash it away in our array for fast lookup by ID.
  PRUint32 lang_ndx = NS_STID_INDEX(lang->GetScriptTypeID());
  if (mLanguageArray[lang_ndx] == nsnull) {
    mLanguageArray[lang_ndx] = lang;
  } else {
    // All languages are services - we should have an identical object!
    NS_ASSERTION(mLanguageArray[lang_ndx] == lang,
                 "Got a different language for this ID???");
  }
  *aLanguage = lang;
  NS_IF_ADDREF(*aLanguage);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::GetScriptRuntimeByID(PRUint32 aLanguageID, 
                                               nsIScriptRuntime **aLanguage)
{
  if (!NS_STID_VALID(aLanguageID)) {
    NS_WARNING("Unknown script language");
    return NS_ERROR_UNEXPECTED;
  }
  *aLanguage = mLanguageArray[NS_STID_INDEX(aLanguageID)];
  if (!*aLanguage) {
    nsCAutoString contractid(NS_LITERAL_CSTRING(
                        "@mozilla.org/script-language;1?id="));
    char langIdStr[25]; // space for an int.
    sprintf(langIdStr, "%d", aLanguageID);
    contractid += langIdStr;
    nsresult rv;
    nsCOMPtr<nsIScriptRuntime> lang = do_GetService(contractid.get(), &rv);

    if (NS_FAILED(rv)) {
      NS_ERROR("Failed to get the script language");
      return rv;
    }

    // Stash it away in our array for fast lookup by ID.
    mLanguageArray[NS_STID_INDEX(aLanguageID)] = lang;
    *aLanguage = lang;
  }
  NS_IF_ADDREF(*aLanguage);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::GetIDForScriptType(const nsAString &aLanguageName,
                                             PRUint32 *aScriptTypeID)
{
  nsCOMPtr<nsIScriptRuntime> languageRuntime;
  nsresult rv;
  rv = GetScriptRuntime(aLanguageName, getter_AddRefs(languageRuntime));
  if (NS_FAILED(rv))
    return rv;

  *aScriptTypeID = languageRuntime->GetScriptTypeID();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::NewScriptGlobalObject(PRBool aIsChrome,
                                                nsIScriptGlobalObject **aGlobal)
{
  return NS_NewScriptGlobalObject(aIsChrome, aGlobal);
}

NS_IMETHODIMP_(nsISupports *)
nsDOMScriptObjectFactory::GetClassInfoInstance(nsDOMClassInfoID aID)
{
  return nsDOMClassInfo::GetClassInfoInstance(aID);
}

NS_IMETHODIMP_(nsISupports *)
nsDOMScriptObjectFactory::GetExternalClassInfoInstance(const nsAString& aName)
{
  extern nsScriptNameSpaceManager *gNameSpaceManager;

  NS_ENSURE_TRUE(gNameSpaceManager, nsnull);

  const nsGlobalNameStruct *globalStruct;
  gNameSpaceManager->LookupName(aName, &globalStruct);
  if (globalStruct) {
    if (globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfoCreator) {
      nsresult rv;
      nsCOMPtr<nsIDOMCIExtension> creator(do_CreateInstance(globalStruct->mCID, &rv));
      NS_ENSURE_SUCCESS(rv, nsnull);

      rv = creator->RegisterDOMCI(NS_ConvertUTF16toUTF8(aName).get(), this);
      NS_ENSURE_SUCCESS(rv, nsnull);

      rv = gNameSpaceManager->LookupName(aName, &globalStruct);
      NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && globalStruct, nsnull);

      NS_ASSERTION(globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo,
                   "The classinfo data for this class didn't get registered.");
    }
    if (globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      return nsDOMClassInfo::GetClassInfoInstance(globalStruct->mData);
    }
  }
  return nsnull;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::Observe(nsISupports *aSubject,
                                  const char *aTopic,
                                  const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
#ifdef MOZ_XUL
    // Flush the XUL cache since it holds JS roots, and we're about to
    // start the final GC.
    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();

    if (cache)
      cache->Flush();
#endif

    nsCOMPtr<nsIThreadJSContextStack> stack =
      do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      JSContext *cx = nsnull;

      stack->GetSafeJSContext(&cx);

      if (cx) {
        // Do one final GC to clean things up before shutdown.

        ::JS_GC(cx);
      }
    }

    nsGlobalWindow::ShutDown();
    nsDOMClassInfo::ShutDown();

    PRUint32 i;
    NS_STID_FOR_INDEX(i) {
      if (mLanguageArray[i] != nsnull) {
        mLanguageArray[i]->ShutDown();
        mLanguageArray[i] = nsnull;
      }
    }

    nsCOMPtr<nsIExceptionService> xs =
      do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);

    if (xs) {
      xs->UnregisterExceptionProvider(this, NS_ERROR_MODULE_DOM);
      xs->UnregisterExceptionProvider(this, NS_ERROR_MODULE_DOM_RANGE);
#ifdef MOZ_SVG
      xs->UnregisterExceptionProvider(this, NS_ERROR_MODULE_SVG);
#endif
      xs->UnregisterExceptionProvider(this, NS_ERROR_MODULE_DOM_XPATH);
      xs->UnregisterExceptionProvider(this, NS_ERROR_MODULE_XPCONNECT);
    }
  }

  return NS_OK;
}

static nsresult
CreateXPConnectException(nsresult aResult, nsIException *aDefaultException,
                         nsIException **_retval)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIXPCException> exception(
      do_CreateInstance("@mozilla.org/js/xpc/Exception;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = exception->Initialize(nsnull, aResult, nsnull, nsnull, nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = exception);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::GetException(nsresult result,
				       nsIException *aDefaultException,
				       nsIException **_retval)
{
  switch (NS_ERROR_GET_MODULE(result))
  {
    case NS_ERROR_MODULE_DOM_RANGE:
      return NS_NewRangeException(result, aDefaultException, _retval);
#ifdef MOZ_SVG
    case NS_ERROR_MODULE_SVG:
      return NS_NewSVGException(result, aDefaultException, _retval);
#endif
    case NS_ERROR_MODULE_DOM_XPATH:
      return NS_NewXPathException(result, aDefaultException, _retval);
    case NS_ERROR_MODULE_XPCONNECT:
      return CreateXPConnectException(result, aDefaultException, _retval);
    default:
      return NS_NewDOMException(result, aDefaultException, _retval);
  }
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::RegisterDOMClassInfo(const char *aName,
					       nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
					       const nsIID *aProtoChainInterface,
					       const nsIID **aInterfaces,
					       PRUint32 aScriptableFlags,
					       PRBool aHasClassInterface,
					       const nsCID *aConstructorCID)
{
  extern nsScriptNameSpaceManager *gNameSpaceManager;

  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return gNameSpaceManager->RegisterDOMCIData(aName,
                                              aConstructorFptr,
                                              aProtoChainInterface,
                                              aInterfaces,
                                              aScriptableFlags,
                                              aHasClassInterface,
                                              aConstructorCID);
}

/* static */ nsresult
nsDOMScriptObjectFactory::Startup()
{
  nsJSRuntime::Startup();
  // nsDOMScriptObjectFactory is a service - assuming that reinitialzing
  // xpcom also recreates all services, then everything else should
  // reinitialize correctly.
  return NS_OK;
}

// Factories
nsresult NS_GetScriptRuntime(const nsAString &aLanguageName,
                             nsIScriptRuntime **aLanguage)
{
  nsresult rv;
  *aLanguage = nsnull;
  nsCOMPtr<nsIDOMScriptObjectFactory> factory = \
        do_GetService(kDOMScriptObjectFactoryCID, &rv);
  if (NS_FAILED(rv))
    return rv;
  return factory->GetScriptRuntime(aLanguageName, aLanguage);
}

nsresult NS_GetScriptRuntimeByID(PRUint32 aScriptTypeID,
                                 nsIScriptRuntime **aLanguage)
{
  nsresult rv;
  *aLanguage = nsnull;
  nsCOMPtr<nsIDOMScriptObjectFactory> factory = \
        do_GetService(kDOMScriptObjectFactoryCID, &rv);
  if (NS_FAILED(rv))
    return rv;
  return factory->GetScriptRuntimeByID(aScriptTypeID, aLanguage);
}
