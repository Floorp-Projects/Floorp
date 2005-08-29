/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * XXX : for the table included below. check how to credit...
 * Copyright (C) 1999 Red Hat Software
 * Author: Owen Taylor <otaylor@redhat.com>
 * 
 * Copyright (C) 2002 NECTEC
 * Author: Theppitak Karoonboonyanan <thep@links.nectec.or.th>
 *
 * Copyright (c) 1996-2000 by Sun Microsystems, Inc.
 * Author: Chookij Vanatham <Chookij.Vanatham@Eng.Sun.COM>
 *
 * Contributor(s): 
 *   Jungshik Shin <jshin@mailaps.org> 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsMemory.h"
#include "nsUnicodeToThaiTTF.h"

// tis620-2 to Thai TTF 'custom' font encoding. Actually, most positions
// are regular code points for Thai and general punctuation marks around
// U+2000. Only about one and half dozens of PUA code points  are used.
// Copied from Pango Thai shaper (thai-xft.c)
  
const static PRUnichar gTIS620ToUnicode[] = {
    0xf700, 0xf701, 0xf702, 0xf703, 0xf704, 0x2026, 0xf705, 0xf706,
    0xf707, 0xf708, 0xf709, 0xf70a, 0xf70b, 0xf70c, 0xf70d, 0xf70e,
    0xf70f, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0xf710, 0xf711, 0xf712, 0xf713, 0xf714, 0xf715, 0xf716, 0xf717,
    0x00a0, 0x0e01, 0x0e02, 0x0e03, 0x0e04, 0x0e05, 0x0e06, 0x0e07,
    0x0e08, 0x0e09, 0x0e0a, 0x0e0b, 0x0e0c, 0x0e0d, 0x0e0e, 0x0e0f,
    0x0e10, 0x0e11, 0x0e12, 0x0e13, 0x0e14, 0x0e15, 0x0e16, 0x0e17,
    0x0e18, 0x0e19, 0x0e1a, 0x0e1b, 0x0e1c, 0x0e1d, 0x0e1e, 0x0e1f,
    0x0e20, 0x0e21, 0x0e22, 0x0e23, 0x0e24, 0x0e25, 0x0e26, 0x0e27,
    0x0e28, 0x0e29, 0x0e2a, 0x0e2b, 0x0e2c, 0x0e2d, 0x0e2e, 0x0e2f,
    0x0e30, 0x0e31, 0x0e32, 0x0e33, 0x0e34, 0x0e35, 0x0e36, 0x0e37,
    0x0e38, 0x0e39, 0x0e3a,      0,      0,      0,      0, 0x0e3f,
    0x0e40, 0x0e41, 0x0e42, 0x0e43, 0x0e44, 0x0e45, 0x0e46, 0x0e47,
    0x0e48, 0x0e49, 0x0e4a, 0x0e4b, 0x0e4c, 0x0e4d, 0x0e4e, 0x0e4f,
    0x0e50, 0x0e51, 0x0e52, 0x0e53, 0x0e54, 0x0e55, 0x0e56, 0x0e57,
    0x0e58, 0x0e59, 0x0e5a, 0x0e5b, 0xf718, 0xf719, 0xf71a,      0
};

//----------------------------------------------------------------------
// Class nsUnicodeToThaiTTF [implementation]
  
NS_IMPL_ISUPPORTS_INHERITED0(nsUnicodeToThaiTTF, nsUnicodeToTIS620)


NS_IMETHODIMP 
nsUnicodeToThaiTTF::Convert(const PRUnichar * aSrc, 
                            PRInt32 * aSrcLength, char * aDest, 
                            PRInt32 * aDestLength)
{

  PRInt32 medLen;
  char *med;

  GetMaxLength(aSrc, *aSrcLength, &medLen);
  NS_ASSERTION(medLen <= *aDestLength, "Insufficient buffer size");

  //intermediate encoder(TIS620) is a single byte converter taking half
  //the space taken by ThaiTTF encoder.
  medLen /= 2;
  if (medLen > CHAR_BUFFER_SIZE)
  {
    med = (char *) nsMemory::Alloc(*aDestLength);
    if (!med)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  else 
    med = mStaticBuffer;

  nsresult rv = nsUnicodeToTIS620::Convert(aSrc, aSrcLength, med, &medLen);

  if (NS_FAILED(rv)) {
    if (med != mStaticBuffer)
      nsMemory::Free(med);
    return rv;
  }

  PRInt32 i, j;

  for (i = 0, j = 0; i < medLen; i++)
  {
    PRUnichar ucs2 = (med[i] & 0x80) ? gTIS620ToUnicode[med[i] & 0x7f] : med[i];
    aDest[j++] = PRUint8((ucs2 & 0xff00) >> 8);
    aDest[j++] = PRUint8(ucs2 & 0x00ff);
  }

  *aDestLength = j;

  if (med != mStaticBuffer)
    nsMemory::Free(med);

  return NS_OK;
}

NS_IMETHODIMP
nsUnicodeToThaiTTF::GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, PRInt32 * aDestLength)
{
  // Each Thai character can generate at most two presentation forms,
  // but we're 'extending' them to 16bit shorts, which accounts for 
  // additional factor of 2.
  *aDestLength = (aSrcLength + 1) *  4; 
  
  return NS_OK;
}

NS_IMETHODIMP 
nsUnicodeToThaiTTF::SetOutputErrorBehavior(PRInt32 aBehavior, 
                                           nsIUnicharEncoder *aEncoder, 
                                           PRUnichar aChar)
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
