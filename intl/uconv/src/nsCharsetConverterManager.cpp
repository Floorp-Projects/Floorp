/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsICharsetAlias.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsICharsetConverterManager.h"
#include "nsEncoderDecoderUtils.h"
#include "nsIStringBundle.h"
#include "nsILocaleService.h"
#include "nsUConvDll.h"
#include "prmem.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsStringEnumerator.h"
#include "nsThreadUtils.h"
#include "nsIProxyObjectManager.h"

#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"

// just for CONTRACTIDs
#include "nsCharsetConverterManager.h"

#ifdef MOZ_USE_NATIVE_UCONV
#include "nsNativeUConvService.h"
#endif

// Pattern of cached, commonly used, single byte decoder
#define NS_1BYTE_CODER_PATTERN "ISO-8859"
#define NS_1BYTE_CODER_PATTERN_LEN 8

// Class nsCharsetConverterManager [implementation]

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCharsetConverterManager,
                              nsICharsetConverterManager)

nsCharsetConverterManager::nsCharsetConverterManager() 
  :mDataBundle(NULL), mTitleBundle(NULL)
{
#ifdef MOZ_USE_NATIVE_UCONV
  mNativeUC = do_GetService(NS_NATIVE_UCONV_SERVICE_CONTRACT_ID);
#endif
}

nsCharsetConverterManager::~nsCharsetConverterManager() 
{
  NS_IF_RELEASE(mDataBundle);
  NS_IF_RELEASE(mTitleBundle);
}

nsresult nsCharsetConverterManager::Init()
{
  if (!mDecoderHash.Init())
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

nsresult nsCharsetConverterManager::RegisterConverterManagerData()
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  RegisterConverterCategory(catman, NS_TITLE_BUNDLE_CATEGORY,
                            "chrome://global/locale/charsetTitles.properties");
  RegisterConverterCategory(catman, NS_DATA_BUNDLE_CATEGORY,
                            "resource://gre/res/charsetData.properties");

  return NS_OK;
}

nsresult
nsCharsetConverterManager::RegisterConverterCategory(nsICategoryManager* catman,
                                                     const char* aCategory,
                                                     const char* aURL)
{
  return catman->AddCategoryEntry(aCategory, aURL, "",
                                  PR_TRUE, PR_TRUE, nsnull);
}

nsresult nsCharsetConverterManager::LoadExtensibleBundle(
                                    const char* aCategory, 
                                    nsIStringBundle ** aResult)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIStringBundleService> sbServ = 
           do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  return sbServ->CreateExtensibleBundle(aCategory, aResult);
}

nsresult nsCharsetConverterManager::GetBundleValue(nsIStringBundle * aBundle, 
                                                   const char * aName, 
                                                   const nsAFlatString& aProp, 
                                                   PRUnichar ** aResult)
{
  nsAutoString key; 

  key.AssignWithConversion(aName);
  ToLowerCase(key); // we lowercase the main comparison key
  key.Append(aProp);

  return aBundle->GetStringFromName(key.get(), aResult);
}

nsresult nsCharsetConverterManager::GetBundleValue(nsIStringBundle * aBundle, 
                                                   const char * aName, 
                                                   const nsAFlatString& aProp, 
                                                   nsAString& aResult)
{
  nsresult rv = NS_OK;

  nsXPIDLString value;
  rv = GetBundleValue(aBundle, aName, aProp, getter_Copies(value));
  if (NS_FAILED(rv))
    return rv;

  aResult = value;

  return NS_OK;
}


//----------------------------------------------------------------------------//----------------------------------------------------------------------------
// Interface nsICharsetConverterManager [implementation]

NS_IMETHODIMP
nsCharsetConverterManager::GetUnicodeEncoder(const char * aDest, 
                                             nsIUnicodeEncoder ** aResult)
{
  // resolve the charset first
  nsCAutoString charset;
  
  // fully qualify to possibly avoid vtable call
  nsCharsetConverterManager::GetCharsetAlias(aDest, charset);

  return nsCharsetConverterManager::GetUnicodeEncoderRaw(charset.get(),
                                                         aResult);
}


NS_IMETHODIMP
nsCharsetConverterManager::GetUnicodeEncoderRaw(const char * aDest, 
                                                nsIUnicodeEncoder ** aResult)
{
  *aResult= nsnull;
  nsCOMPtr<nsIUnicodeEncoder> encoder;

#ifdef MOZ_USE_NATIVE_UCONV
  if (mNativeUC) {
    nsCOMPtr<nsISupports> supports;
    mNativeUC->GetNativeConverter("UCS-2", 
                                  aDest,
                                  getter_AddRefs(supports));

    encoder = do_QueryInterface(supports);

    if (encoder) {
      NS_ADDREF(*aResult = encoder);
      return NS_OK;
    }
  }
#endif  
  nsresult rv = NS_OK;

  nsCAutoString
    contractid(NS_LITERAL_CSTRING(NS_UNICODEENCODER_CONTRACTID_BASE) +
               nsDependentCString(aDest));

  // Always create an instance since encoders hold state.
  encoder = do_CreateInstance(contractid.get(), &rv);

  if (NS_FAILED(rv))
    rv = NS_ERROR_UCONV_NOCONV;
  else
  {
    *aResult = encoder.get();
    NS_ADDREF(*aResult);
  }
  return rv;
}

NS_IMETHODIMP
nsCharsetConverterManager::GetUnicodeDecoder(const char * aSrc, 
                                             nsIUnicodeDecoder ** aResult)
{
  // resolve the charset first
  nsCAutoString charset;
  
  // fully qualify to possibly avoid vtable call
  nsCharsetConverterManager::GetCharsetAlias(aSrc, charset);

  return nsCharsetConverterManager::GetUnicodeDecoderRaw(charset.get(),
                                                         aResult);
}

NS_IMETHODIMP
nsCharsetConverterManager::GetUnicodeDecoderRaw(const char * aSrc, 
                                                nsIUnicodeDecoder ** aResult)
{
  *aResult= nsnull;
  nsCOMPtr<nsIUnicodeDecoder> decoder;

#ifdef MOZ_USE_NATIVE_UCONV
  if (mNativeUC) {
    nsCOMPtr<nsISupports> supports;
    mNativeUC->GetNativeConverter(aSrc,
                                  "UCS-2", 
                                  getter_AddRefs(supports));
    
    decoder = do_QueryInterface(supports);

    if (decoder) {
      NS_ADDREF(*aResult = decoder);
      return NS_OK;
    }
  }
#endif
  nsresult rv = NS_OK;

  NS_NAMED_LITERAL_CSTRING(contractbase, NS_UNICODEDECODER_CONTRACTID_BASE);
  nsDependentCString src(aSrc);
  
  if (!strncmp(aSrc, NS_1BYTE_CODER_PATTERN, NS_1BYTE_CODER_PATTERN_LEN))
  {
    // Single byte decoders don't hold state. Optimize by using a service, and
    // cache it in our hash to avoid repeated trips through the service manager.
    if (!mDecoderHash.Get(aSrc, getter_AddRefs(decoder))) {
      decoder = do_GetService(PromiseFlatCString(contractbase + src).get(),
                              &rv);
      if (NS_SUCCEEDED(rv))
        mDecoderHash.Put(aSrc, decoder);
    }
  }
  else
  {
    decoder = do_CreateInstance(PromiseFlatCString(contractbase + src).get(),
                                &rv);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UCONV_NOCONV);

  decoder.forget(aResult);
  return rv;
}

nsresult 
nsCharsetConverterManager::GetList(const nsACString& aCategory,
                                   const nsACString& aPrefix,
                                   nsIUTF8StringEnumerator** aResult)
{
  if (aResult == NULL) 
    return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  nsresult rv;
  nsCAutoString alias;

  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCStringArray* array = new nsCStringArray;
  if (!array)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  catman->EnumerateCategory(PromiseFlatCString(aCategory).get(), 
                            getter_AddRefs(enumerator));

  PRBool hasMore;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    if (NS_FAILED(enumerator->GetNext(getter_AddRefs(supports))))
      continue;
    
    nsCOMPtr<nsISupportsCString> supStr = do_QueryInterface(supports);
    if (!supStr)
      continue;

    nsCAutoString fullName(aPrefix);
    
    nsCAutoString name;
    if (NS_FAILED(supStr->GetData(name)))
      continue;

    fullName += name;
    rv = GetCharsetAlias(fullName.get(), alias);
    if (NS_FAILED(rv)) 
      continue;

    rv = array->AppendCString(alias);
  }
    
  return NS_NewAdoptingUTF8StringEnumerator(aResult, array);
}

// we should change the interface so that we can just pass back a enumerator!
NS_IMETHODIMP
nsCharsetConverterManager::GetDecoderList(nsIUTF8StringEnumerator ** aResult)
{
  return GetList(NS_LITERAL_CSTRING(NS_UNICODEDECODER_NAME),
                 EmptyCString(), aResult);
}

NS_IMETHODIMP
nsCharsetConverterManager::GetEncoderList(nsIUTF8StringEnumerator ** aResult)
{
  return GetList(NS_LITERAL_CSTRING(NS_UNICODEENCODER_NAME),
                 EmptyCString(), aResult);
}

NS_IMETHODIMP
nsCharsetConverterManager::GetCharsetDetectorList(nsIUTF8StringEnumerator** aResult)
{
  return GetList(NS_LITERAL_CSTRING("charset-detectors"),
                 NS_LITERAL_CSTRING("chardet."), aResult);
}

// XXX Improve the implementation of this method. Right now, it is build on 
// top of the nsCharsetAlias service. We can make the nsCharsetAlias
// better, with its own hash table (not the StringBundle anymore) and
// a nicer file format.
NS_IMETHODIMP
nsCharsetConverterManager::GetCharsetAlias(const char * aCharset, 
                                           nsACString& aResult)
{
  NS_PRECONDITION(aCharset, "null param");
  if (!aCharset)
    return NS_ERROR_NULL_POINTER;

  // We must not use the charset alias from a background thread
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsICharsetConverterManager> self;
    nsresult rv =
    NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                         NS_GET_IID(nsICharsetConverterManager),
                         this, NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                         getter_AddRefs(self));
    NS_ENSURE_SUCCESS(rv, rv);
    return self->GetCharsetAlias(aCharset, aResult);
  }

  // We try to obtain the preferred name for this charset from the charset 
  // aliases. If we don't get it from there, we just use the original string
  nsDependentCString charset(aCharset);
  nsCOMPtr<nsICharsetAlias> csAlias(do_GetService(NS_CHARSETALIAS_CONTRACTID));
  NS_ASSERTION(csAlias, "failed to get the CharsetAlias service");
  if (csAlias) {
    nsAutoString pref;
    nsresult rv = csAlias->GetPreferred(charset, aResult);
    if (NS_SUCCEEDED(rv)) {
      return (!aResult.IsEmpty()) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aResult = charset;
  return NS_OK;
}


NS_IMETHODIMP
nsCharsetConverterManager::GetCharsetTitle(const char * aCharset, 
                                           nsAString& aResult)
{
  if (aCharset == NULL) return NS_ERROR_NULL_POINTER;

  if (mTitleBundle == NULL) {
    nsresult rv = LoadExtensibleBundle(NS_TITLE_BUNDLE_CATEGORY, &mTitleBundle);
    if (NS_FAILED(rv))
      return rv;
  }

  return GetBundleValue(mTitleBundle, aCharset, NS_LITERAL_STRING(".title"), aResult);
}

NS_IMETHODIMP
nsCharsetConverterManager::GetCharsetData(const char * aCharset, 
                                          const PRUnichar * aProp,
                                          nsAString& aResult)
{
  if (aCharset == NULL)
    return NS_ERROR_NULL_POINTER;
  // aProp can be NULL

  if (mDataBundle == NULL) {
    nsresult rv = LoadExtensibleBundle(NS_DATA_BUNDLE_CATEGORY, &mDataBundle);
    if (NS_FAILED(rv))
      return rv;
  }

  return GetBundleValue(mDataBundle, aCharset, nsDependentString(aProp), aResult);
}

NS_IMETHODIMP
nsCharsetConverterManager::GetCharsetLangGroup(const char * aCharset, 
                                               nsIAtom** aResult)
{
  // resolve the charset first
  nsCAutoString charset;

  nsresult rv = GetCharsetAlias(aCharset, charset);
  if (NS_FAILED(rv))
    return rv;

  // fully qualify to possibly avoid vtable call
  return nsCharsetConverterManager::GetCharsetLangGroupRaw(charset.get(),
                                                           aResult);
}

NS_IMETHODIMP
nsCharsetConverterManager::GetCharsetLangGroupRaw(const char * aCharset, 
                                                  nsIAtom** aResult)
{

  *aResult = nsnull;
  if (aCharset == NULL)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;

  if (mDataBundle == NULL) {
    rv = LoadExtensibleBundle(NS_DATA_BUNDLE_CATEGORY, &mDataBundle);
    if (NS_FAILED(rv))
      return rv;
  }

  nsAutoString langGroup;
  rv = GetBundleValue(mDataBundle, aCharset, NS_LITERAL_STRING(".LangGroup"), langGroup);

  if (NS_SUCCEEDED(rv))
    *aResult = NS_NewAtom(langGroup);

  return rv;
}
