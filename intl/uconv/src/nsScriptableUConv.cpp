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
 *   Makoto Kato <m_kato@ga2.so-net.ne.jp >
 *   Ryoichi Furukawa <oliver@1000cp.com>
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

#include "nsAtomicRefcnt.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIScriptableUConv.h"
#include "nsScriptableUConv.h"
#include "nsIStringStream.h"
#include "nsCRT.h"
#include "nsComponentManagerUtils.h"

static PRInt32          gInstanceCount = 0;

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsScriptableUnicodeConverter, nsIScriptableUnicodeConverter)

nsScriptableUnicodeConverter::nsScriptableUnicodeConverter()
: mIsInternal(PR_FALSE)
{
  PR_ATOMIC_INCREMENT(&gInstanceCount);
}

nsScriptableUnicodeConverter::~nsScriptableUnicodeConverter()
{
  PR_ATOMIC_DECREMENT(&gInstanceCount);
}

nsresult
nsScriptableUnicodeConverter::ConvertFromUnicodeWithLength(const nsAString& aSrc,
                                                           PRInt32* aOutLen,
                                                           char **_retval)
{
  if (!mEncoder)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  PRInt32 inLength = aSrc.Length();
  const nsAFlatString& flatSrc = PromiseFlatString(aSrc);
  rv = mEncoder->GetMaxLength(flatSrc.get(), inLength, aOutLen);
  if (NS_SUCCEEDED(rv)) {
    *_retval = (char*) nsMemory::Alloc(*aOutLen+1);
    if (!*_retval)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = mEncoder->Convert(flatSrc.get(), &inLength, *_retval, aOutLen);
    if (NS_SUCCEEDED(rv))
    {
      (*_retval)[*aOutLen] = '\0';
      return NS_OK;
    }
    nsMemory::Free(*_retval);
  }
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

/* ACString ConvertFromUnicode (in AString src); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertFromUnicode(const nsAString& aSrc,
                                                 nsACString& _retval)
{
  PRInt32 len;
  char* str;
  nsresult rv = ConvertFromUnicodeWithLength(aSrc, &len, &str);
  if (NS_SUCCEEDED(rv)) {
    // No Adopt on nsACString :(
    _retval.Assign(str, len);
    nsMemory::Free(str);
  }
  return rv;
}

nsresult
nsScriptableUnicodeConverter::FinishWithLength(char **_retval, PRInt32* aLength)
{
  if (!mEncoder)
    return NS_ERROR_FAILURE;

  PRInt32 finLength = 32;

  *_retval = (char *) nsMemory::Alloc(finLength);
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = mEncoder->Finish(*_retval, &finLength);
  if (NS_SUCCEEDED(rv))
    *aLength = finLength;
  else
    nsMemory::Free(*_retval);

  return rv;

}

/* ACString Finish(); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::Finish(nsACString& _retval)
{
  PRInt32 len;
  char* str;
  nsresult rv = FinishWithLength(&str, &len);
  if (NS_SUCCEEDED(rv)) {
    // No Adopt on nsACString :(
    _retval.Assign(str, len);
    nsMemory::Free(str);
  }
  return rv;
}

/* AString ConvertToUnicode (in ACString src); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertToUnicode(const nsACString& aSrc, nsAString& _retval)
{
  nsACString::const_iterator i;
  aSrc.BeginReading(i);
  return ConvertFromByteArray(reinterpret_cast<const PRUint8*>(i.get()),
                              aSrc.Length(),
                              _retval);
}

/* AString convertFromByteArray([const,array,size_is(aCount)] in octet aData,
                                in unsigned long aCount);
 */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertFromByteArray(const PRUint8* aData,
                                                   PRUint32 aCount,
                                                   nsAString& _retval)
{
  if (!mDecoder)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  PRInt32 inLength = aCount;
  PRInt32 outLength;
  rv = mDecoder->GetMaxLength(reinterpret_cast<const char*>(aData),
                              inLength, &outLength);
  if (NS_SUCCEEDED(rv))
  {
    PRUnichar* buf = (PRUnichar*) nsMemory::Alloc((outLength+1)*sizeof(PRUnichar));
    if (!buf)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = mDecoder->Convert(reinterpret_cast<const char*>(aData),
                           &inLength, buf, &outLength);
    if (NS_SUCCEEDED(rv))
    {
      buf[outLength] = 0;
      _retval.Assign(buf, outLength);
    }
    nsMemory::Free(buf);
    return rv;
  }
  return NS_ERROR_FAILURE;

}

/* void convertToByteArray(in AString aString,
                          [optional] out unsigned long aLen,
                          [array, size_is(aLen),retval] out octet aData);
 */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertToByteArray(const nsAString& aString,
                                                 PRUint32* aLen,
                                                 PRUint8** _aData)
{
  char* data;
  PRInt32 len;
  nsresult rv = ConvertFromUnicodeWithLength(aString, &len, &data);
  if (NS_FAILED(rv))
    return rv;
  nsXPIDLCString str;
  str.Adopt(data, len); // NOTE: This uses the XPIDLCString as a byte array

  rv = FinishWithLength(&data, &len);
  if (NS_FAILED(rv))
    return rv;

  str.Append(data, len);
  nsMemory::Free(data);
  // NOTE: this being a byte array, it needs no null termination
  *_aData = reinterpret_cast<PRUint8*>
                            (nsMemory::Clone(str.get(), str.Length()));
  if (!*_aData)
    return NS_ERROR_OUT_OF_MEMORY;
  *aLen = str.Length();
  return NS_OK;
}

/* nsIInputStream convertToInputStream(in AString aString); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertToInputStream(const nsAString& aString,
                                                   nsIInputStream** _retval)
{
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> inputStream =
    do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  if (NS_FAILED(rv))
    return rv;

  PRUint8* data;
  PRUint32 dataLen;
  rv = ConvertToByteArray(aString, &dataLen, &data);
  if (NS_FAILED(rv))
    return rv;

  rv = inputStream->AdoptData(reinterpret_cast<char*>(data), dataLen);
  if (NS_FAILED(rv)) {
    nsMemory::Free(data);
    return rv;
  }

  NS_ADDREF(*_retval = inputStream);
  return rv;
}

/* attribute string charset; */
NS_IMETHODIMP
nsScriptableUnicodeConverter::GetCharset(char * *aCharset)
{
  *aCharset = ToNewCString(mCharset);
  if (!*aCharset)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsScriptableUnicodeConverter::SetCharset(const char * aCharset)
{
  mCharset.Assign(aCharset);
  return InitConverter();
}

NS_IMETHODIMP
nsScriptableUnicodeConverter::GetIsInternal(PRBool *aIsInternal)
{
  *aIsInternal = mIsInternal;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableUnicodeConverter::SetIsInternal(const PRBool aIsInternal)
{
  mIsInternal = aIsInternal;
  return NS_OK;
}

nsresult
nsScriptableUnicodeConverter::InitConverter()
{
  nsresult rv = NS_OK;
  mEncoder = NULL ;

  nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);

  if (NS_SUCCEEDED( rv) && (nsnull != ccm)) {
    // get charset atom due to getting unicode converter
    
    // get an unicode converter
    rv = ccm->GetUnicodeEncoder(mCharset.get(), getter_AddRefs(mEncoder));
    if(NS_SUCCEEDED(rv)) {
      rv = mEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, (PRUnichar)'?');
      if(NS_SUCCEEDED(rv)) {
        rv = mIsInternal ?
          ccm->GetUnicodeDecoderInternal(mCharset.get(),
                                         getter_AddRefs(mDecoder)) :
          ccm->GetUnicodeDecoder(mCharset.get(),
                                 getter_AddRefs(mDecoder));
      }
    }
  }

  return rv ;
}
