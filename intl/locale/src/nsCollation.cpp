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
 * The Original Code is mozilla.org code.
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

#include "nsIPlatformCharset.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCollation.h"
#include "nsCollationCID.h"
#include "nsUnicharUtilCIID.h"
#include "prmem.h"
#include "nsReadableUtils.h"

////////////////////////////////////////////////////////////////////////////////

NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);

NS_IMPL_ISUPPORTS1(nsCollationFactory, nsICollationFactory)

nsresult nsCollationFactory::CreateCollation(nsILocale* locale, nsICollation** instancePtr)
{
  // Create a collation interface instance.
  //
  nsICollation *inst;
  nsresult res;
  
  res = CallCreateInstance(kCollationCID, &inst);
  if (NS_FAILED(res)) {
    return res;
  }

  inst->Initialize(locale);
  *instancePtr = inst;

  return res;
}

////////////////////////////////////////////////////////////////////////////////

nsCollation::nsCollation()
{
  MOZ_COUNT_CTOR(nsCollation);
  nsresult res;
  mCaseConversion = do_GetService(NS_UNICHARUTIL_CONTRACTID, &res);
  NS_ASSERTION(NS_SUCCEEDED(res), "CreateInstance failed for kCaseConversionIID");
}

nsCollation::~nsCollation()
{
  MOZ_COUNT_DTOR(nsCollation);
}

nsresult nsCollation::NormalizeString(const nsAString& stringIn, nsAString& stringOut)
{
  if (!mCaseConversion) {
    stringOut = stringIn;
  }
  else {
    PRInt32 aLength = stringIn.Length();

    if (aLength <= 64) {
      PRUnichar conversionBuffer[64];
      mCaseConversion->ToLower(PromiseFlatString(stringIn).get(), conversionBuffer, aLength);
      stringOut.Assign(conversionBuffer, aLength);
    }
    else {
      PRUnichar* conversionBuffer;
      conversionBuffer = new PRUnichar[aLength];
      if (!conversionBuffer) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mCaseConversion->ToLower(PromiseFlatString(stringIn).get(), conversionBuffer, aLength);
      stringOut.Assign(conversionBuffer, aLength);
      delete [] conversionBuffer;
    }
  }
  return NS_OK;
}

nsresult nsCollation::SetCharset(const char* aCharset)
{
  NS_ENSURE_ARG_POINTER(aCharset);

  nsresult rv;
  nsCOMPtr <nsICharsetConverterManager> charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = charsetConverterManager->GetUnicodeEncoder(aCharset,
                                                    getter_AddRefs(mEncoder));
  }
  return rv;
}

nsresult nsCollation::UnicodeToChar(const nsAString& aSrc, char** dst)
{
  NS_ENSURE_ARG_POINTER(dst);

  nsresult res = NS_OK;
  if (!mEncoder)
    res = SetCharset("ISO-8859-1");

  if (NS_SUCCEEDED(res)) {
    const nsPromiseFlatString& src = PromiseFlatString(aSrc);
    const PRUnichar *unichars = src.get();
    PRInt32 unicharLength = src.Length();
    PRInt32 dstLength;
    res = mEncoder->GetMaxLength(unichars, unicharLength, &dstLength);
    if (NS_SUCCEEDED(res)) {
      PRInt32 bufLength = dstLength + 1 + 32; // extra 32 bytes for Finish() call
      *dst = (char *) PR_Malloc(bufLength);
      if (*dst) {
        **dst = '\0';
        res = mEncoder->Convert(unichars, &unicharLength, *dst, &dstLength);

        if (NS_SUCCEEDED(res) || (NS_ERROR_UENC_NOMAPPING == res)) {
          // Finishes the conversion. The converter has the possibility to write some 
          // extra data and flush its final state.
          PRInt32 finishLength = bufLength - dstLength; // remaining unused buffer length
          if (finishLength > 0) {
            res = mEncoder->Finish((*dst + dstLength), &finishLength);
            if (NS_SUCCEEDED(res)) {
              (*dst)[dstLength + finishLength] = '\0';
            }
          }
        }
        if (NS_FAILED(res)) {
          PR_Free(*dst);
          *dst = nsnull;
        }
      }
      else {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return res;
}



