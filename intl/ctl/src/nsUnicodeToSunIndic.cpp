/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ucvhi : nsUnicodeToSunIndic.h
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
 * The Original Code is mozilla.org code. The Initial Developer of the Original Code is Sun Microsystems, Inc.  Portions created by SUN are Copyright (C) 2000 SUN Microsystems, Inc. All Rights Reserved.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 *   Jungshik Shin (jshin@mailmaps.org)
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
#include "nsCtlCIID.h"
#include "nsILE.h"
#include "nsULE.h"
#include "nsUnicodeToSunIndic.h"

NS_IMPL_ISUPPORTS2(nsUnicodeToSunIndic, nsIUnicodeEncoder, nsICharRepresentable)

NS_IMETHODIMP
nsUnicodeToSunIndic::SetOutputErrorBehavior(PRInt32           aBehavior,
                                            nsIUnicharEncoder *aEncoder,
                                            PRUnichar         aChar)
{
  if (aBehavior == kOnError_CallBack && aEncoder == nsnull)
     return NS_ERROR_NULL_POINTER;
  NS_IF_RELEASE(mErrEncoder);
  mErrEncoder = aEncoder;
  NS_IF_ADDREF(mErrEncoder);

  mErrBehavior = aBehavior;
  mErrChar = aChar;
  return NS_OK;
}

// constructor and destructor
nsUnicodeToSunIndic::nsUnicodeToSunIndic()
{
  static   NS_DEFINE_CID(kLECID, NS_ULE_CID);
  nsresult rv;

  mCtlObj = do_CreateInstance(kLECID, &rv);
  if (NS_FAILED(rv)) {

#ifdef DEBUG_prabhath
    // No other error handling needed here since we
    // handle absence of mCtlObj in Convert
    printf("ERROR: Cannot create instance of component " NS_ULE_PROGID " [%x].\n", rv);
#endif

    NS_WARNING("Indian Text Shaping Will Not Be Supported\n");
    mCtlObj = nsnull;
  }
}

nsUnicodeToSunIndic::~nsUnicodeToSunIndic()
{
  // Maybe convert nsILE to a service
  //NS_IF_RELEASE(mCtlObj);
}

/*
 * This method converts the unicode to this font index.
 * Note: ConversionBufferFullException is not handled
 *       since this class is only used for character display.
 */
NS_IMETHODIMP nsUnicodeToSunIndic::Convert(const PRUnichar* input,
                                           PRInt32*         aSrcLength,
                                           char*            output,
                                           PRInt32*         aDestLength)
{
  PRSize outLen;

  if (mCtlObj == nsnull) {
#ifdef DEBUG_prabhath
  printf("Debug/Test Case of No Hindi pango shaper Object\n");
  // Comment out mCtlObj == nsnull for test purposes
  printf("ERROR: No Hindi Text Layout Implementation");
#endif

    NS_WARNING("cannot get default converter for Hindi");
    return NS_ERROR_FAILURE;
  }

  mCharOff = mByteOff = 0;
  mCtlObj->GetPresentationForm(input, *aSrcLength, "sun.unicode.india-0",
                               &output[mByteOff], &outLen, PR_TRUE);
  *aDestLength = outLen;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToSunIndic::Finish(char *output, PRInt32 *aDestLength)
{
  // Finish does'nt have to do much as Convert already flushes
  // to output buffer
  mByteOff = mCharOff = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToSunIndic::Reset()
{
  mByteOff = mCharOff = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToSunIndic::GetMaxLength(const PRUnichar * aSrc,
                                                PRInt32 aSrcLength,
                                                PRInt32 * aDestLength)
{
  *aDestLength = (aSrcLength + 1) *  2; // Each Hindi character can generate
                                        // atmost two presentation forms
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToSunIndic::FillInfo(PRUint32* aInfo)
{
  PRUint16 i;

  // 00-0x7f
  for (i = 0;i <= 0x7f; i++)
    SET_REPRESENTABLE(aInfo, i);

  // \u904, \u90a, \u93b, \u94e, \u94f are Undefined
  for (i = 0x0901; i <= 0x0903; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x0905; i <= 0x0939; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x093c; i <= 0x094d; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x0950; i <= 0x0954; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x0958; i <= 0x0970; i++)
    SET_REPRESENTABLE(aInfo, i);
 
  // ZWJ and ZWNJ support & coverage need to be added.
  return NS_OK;
}
