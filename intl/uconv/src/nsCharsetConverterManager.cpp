/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsICharsetConverterManager2.h"
#include "nsIStringBundle.h"
#include "nsILocaleService.h"
#include "nsUConvDll.h"
#include "prmem.h"
#include "nsCRT.h"

#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsICharsetDetector.h"

// just for CIDs
#include "nsIUnicodeDecodeHelper.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsCharsetConverterManager.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 
static NS_DEFINE_CID(kSupportsArrayCID, NS_SUPPORTSARRAY_CID); 

// Pattern of cached, commonly used, single byte decoder
#define NS_1BYTE_CODER_PATTERN "ISO-8859"
#define NS_1BYTE_CODER_PATTERN_LEN 8

// Class nsCharsetConverterManager [implementation]

NS_IMPL_THREADSAFE_ISUPPORTS2(nsCharsetConverterManager,
                              nsICharsetConverterManager, 
                              nsICharsetConverterManager2);

nsCharsetConverterManager::nsCharsetConverterManager() 
:mDataBundle(NULL), mTitleBundle(NULL)
{
}

nsCharsetConverterManager::~nsCharsetConverterManager() 
{
  NS_IF_RELEASE(mDataBundle);
  NS_IF_RELEASE(mTitleBundle);
}


nsresult nsCharsetConverterManager::RegisterConverterManagerData()
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  RegisterConverterCategory(catman, NS_TITLE_BUNDLE_CATEGORY,
                            "chrome://global/locale/charsetTitles.properties");
  RegisterConverterCategory(catman, NS_DATA_BUNDLE_CATEGORY,
                            "resource:/res/charsetData.properties");

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
  nsresult res = NS_OK;

  nsCOMPtr<nsIStringBundleService> sbServ = 
           do_GetService(kStringBundleServiceCID, &res);
  if (NS_FAILED(res)) return res;

  res = sbServ->CreateExtensibleBundle(aCategory, aResult);
  if (NS_FAILED(res)) return res;

  return res;
}

nsresult nsCharsetConverterManager::GetBundleValue(nsIStringBundle * aBundle, 
                                                   const nsIAtom * aName, 
                                                   const nsAFlatString& aProp, 
                                                   PRUnichar ** aResult)
{
  nsresult res = NS_OK;

  nsAutoString key;
  res = ((nsIAtom *) aName)->ToString(key);
  if (NS_FAILED(res)) return res;

  ToLowerCase(key); // we lowercase the main comparison key
  if (!aProp.IsEmpty()) key.Append(aProp.get()); // yes, this param may be NULL

  res = aBundle->GetStringFromName(key.get(), aResult);
  return res;
}

nsresult nsCharsetConverterManager::GetBundleValue(nsIStringBundle * aBundle, 
                                                   const nsIAtom * aName, 
                                                   const nsAFlatString& aProp, 
                                                   nsIAtom ** aResult)
{
  nsresult res = NS_OK;

  PRUnichar * value;
  res = GetBundleValue(aBundle, aName, aProp, &value);
  if (NS_FAILED(res)) return res;

  *aResult =  NS_NewAtom(value);
  PR_Free(value);

  return NS_OK;
}


//----------------------------------------------------------------------------
// Interface nsICharsetConverterManager [implementation]

NS_IMETHODIMP nsCharsetConverterManager::GetUnicodeEncoder(
                                         const nsString * aDest, 
                                         nsIUnicodeEncoder ** aResult)
{
  *aResult= nsnull;
  nsresult res = NS_OK;

  nsCAutoString contractid(
                    NS_LITERAL_CSTRING(NS_UNICODEENCODER_CONTRACTID_BASE) +
                    NS_LossyConvertUCS2toASCII(*aDest));

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  // Always create an instance since encoders hold state.
  encoder = do_CreateInstance(contractid.get(), &res);

  if (NS_FAILED(res))
    res = NS_ERROR_UCONV_NOCONV;
  else
  {
    *aResult = encoder.get();
    NS_ADDREF(*aResult);
  }
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetUnicodeDecoder(
                                         const nsString * aSrc, 
                                         nsIUnicodeDecoder ** aResult)
{
  *aResult= nsnull;
  nsresult res = NS_OK;;

  NS_NAMED_LITERAL_CSTRING(kUnicodeDecoderContractIDBase,
                           NS_UNICODEDECODER_CONTRACTID_BASE);

  nsCAutoString contractid(kUnicodeDecoderContractIDBase +
                           NS_LossyConvertUCS2toASCII(*aSrc));

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  if (!strncmp(contractid.get()+kUnicodeDecoderContractIDBase.Length(),
               NS_1BYTE_CODER_PATTERN,
               NS_1BYTE_CODER_PATTERN_LEN))
  {
    // Single byte decoders dont hold state. Optimize by using a service.
    decoder = do_GetService(contractid.get(), &res);
  }
  else
  {
    decoder = do_CreateInstance(contractid.get(), &res);
  }
  if(NS_FAILED(res))
    res = NS_ERROR_UCONV_NOCONV;
  else
  {
    *aResult = decoder.get();
    NS_ADDREF(*aResult);
  }
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetLangGroup(
                                         nsString * aCharset, 
                                         nsIAtom ** aResult)
{
  if (aCharset == NULL) return NS_ERROR_NULL_POINTER;
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  nsCOMPtr<nsIAtom> atom;
  nsresult res = GetCharsetAtom(aCharset->get(), getter_AddRefs(atom));
  if (NS_FAILED(res)) return res;

  res = GetCharsetLangGroup(atom, aResult);
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetUnicodeDecoder(
                                         const nsIAtom * aCharset, 
                                         nsIUnicodeDecoder ** aResult)
{
  if (aCharset == NULL) return NS_ERROR_NULL_POINTER;
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  // XXX use nsImmutableString
  nsAutoString name;
  NS_CONST_CAST(nsIAtom*, aCharset)->ToString(name);
  return GetUnicodeDecoder(&name, aResult);
}

NS_IMETHODIMP nsCharsetConverterManager::GetUnicodeEncoder(
                                         const nsIAtom * aCharset, 
                                         nsIUnicodeEncoder ** aResult)
{
  if (aCharset == NULL) return NS_ERROR_NULL_POINTER;
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  // XXX use nsImmutableString
  nsAutoString name;
  NS_CONST_CAST(nsIAtom*, aCharset)->ToString(name);
  return GetUnicodeEncoder(&name, aResult);
}

nsresult 
nsCharsetConverterManager::GetList(const nsACString& aCategory,
                                   const nsACString& aPrefix,
                                   nsISupportsArray** aResult)
{
  if (aResult == NULL) 
    return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  nsresult rv;
  nsCOMPtr<nsIAtom> atom;
 
  nsCOMPtr<nsISupportsArray> array = do_CreateInstance(kSupportsArrayCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
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
    rv = GetCharsetAtom2(fullName.get(), getter_AddRefs(atom));
    if (NS_FAILED(rv)) 
      continue;
    
    rv = array->AppendElement(atom);
  }
    
  NS_ADDREF(*aResult = array);
  return NS_OK;
}

// we should change the interface so that we can just pass back a enumerator!
NS_IMETHODIMP nsCharsetConverterManager::GetDecoderList(nsISupportsArray ** aResult)
{
  return GetList(NS_LITERAL_CSTRING(NS_UNICODEDECODER_NAME),
                 NS_LITERAL_CSTRING(""), aResult);
}

NS_IMETHODIMP nsCharsetConverterManager::GetEncoderList(
                                         nsISupportsArray ** aResult)
{
  return GetList(NS_LITERAL_CSTRING(NS_UNICODEENCODER_NAME),
                 NS_LITERAL_CSTRING(""), aResult);
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetDetectorList(
                                         nsISupportsArray ** aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  return GetList(NS_LITERAL_CSTRING(NS_CHARSET_DETECTOR_CATEGORY),
                 NS_LITERAL_CSTRING("chardet."), aResult);
}

// XXX Improve the implementation of this method. Right now, it is build on 
// top of two things: the nsCharsetAlias service and the Atom engine. We can 
// improve on both. First, make the nsCharsetAlias better, with its own hash 
// table (not the StringBundle anymore) and a nicer file format. Second, 
// reimplement the Atom engine for the specific Charset case - more optimal.
// Finally, unify the two for even better performance.
NS_IMETHODIMP nsCharsetConverterManager::GetCharsetAtom(
                                         const PRUnichar * aCharset, 
                                         nsIAtom ** aResult)
{
  NS_PRECONDITION(aCharset && aResult, "null param");
  if (!aCharset)
    return NS_ERROR_NULL_POINTER;

  // We try to obtain the preferred name for this charset from the charset 
  // aliases. If we don't get it from there, we just use the original string
  nsDependentString charset(aCharset);
  nsCOMPtr<nsICharsetAlias> csAlias( do_GetService(kCharsetAliasCID) );
  NS_ASSERTION(csAlias, "failed to get the CharsetAlias service");
  if (csAlias) {
    nsAutoString pref;
    nsresult res = csAlias->GetPreferred(charset, pref);
    if (NS_SUCCEEDED(res)) {
      *aResult = NS_NewAtom(pref);
      return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aResult = NS_NewAtom(charset);
  return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetAtom2(
                                         const char * aCharset, 
                                         nsIAtom ** aResult)
{
  nsAutoString str;
  str.AssignWithConversion(aCharset);
  return GetCharsetAtom(str.get(), aResult);
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetTitle(
                                         const nsIAtom * aCharset, 
                                         PRUnichar ** aResult)
{
  if (aCharset == NULL) return NS_ERROR_NULL_POINTER;
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  nsresult res = NS_OK;

  if (mTitleBundle == NULL) {
    res = LoadExtensibleBundle(NS_TITLE_BUNDLE_CATEGORY, &mTitleBundle);
    if (NS_FAILED(res)) return res;
  }

  res = GetBundleValue(mTitleBundle, aCharset, NS_LITERAL_STRING(".title"), aResult);
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetTitle2(
                                         const nsIAtom * aCharset, 
                                         nsString * aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;

  PRUnichar * title;
  res = GetCharsetTitle(aCharset, &title);
  if (NS_FAILED(res)) return res;

  aResult->Assign(title);
  PR_Free(title);
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetData(
                                         const nsIAtom * aCharset, 
                                         const PRUnichar * aProp,
                                         PRUnichar ** aResult)
{
  if (aCharset == NULL) return NS_ERROR_NULL_POINTER;
  // aProp can be NULL
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  nsresult res = NS_OK;

  if (mDataBundle == NULL) {
    res = LoadExtensibleBundle(NS_DATA_BUNDLE_CATEGORY, &mDataBundle);
    if (NS_FAILED(res)) return res;
  }

  res = GetBundleValue(mDataBundle, aCharset, nsDependentString(aProp), aResult);
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetData2(
                                         const nsIAtom * aCharset, 
                                         const PRUnichar * aProp,
                                         nsString * aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;

  PRUnichar * data;
  res = GetCharsetData(aCharset, aProp, &data);
  if (NS_FAILED(res)) return res;

  aResult->Assign(data);
  PR_Free(data);
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetCharsetLangGroup(
                                         const nsIAtom * aCharset, 
                                         nsIAtom ** aResult)
{
  if (aCharset == NULL) return NS_ERROR_NULL_POINTER;
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  *aResult = NULL;

  nsresult res = NS_OK;

  if (mDataBundle == NULL) {
    res = LoadExtensibleBundle(NS_DATA_BUNDLE_CATEGORY, &mDataBundle);
    if (NS_FAILED(res)) return res;
  }

  res = GetBundleValue(mDataBundle, aCharset, NS_LITERAL_STRING(".LangGroup"), aResult);
  return res;
}
