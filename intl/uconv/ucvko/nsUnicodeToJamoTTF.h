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
 * Portions created by the Initial Developer are Copyright (C) 1998, 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jungshik Shin <jshin@mailaps.org>.
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

#ifndef nsUnicodeToJamoTTF_h___
#define nsUnicodeToJamoTTF_h___

#include "nsUCSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToJamoTTF [declaration]

class nsUnicodeToJamoTTF : public nsIUnicodeEncoder, public nsICharRepresentable
{

  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsUnicodeToJamoTTF();
  virtual ~nsUnicodeToJamoTTF();

  NS_IMETHOD Convert(
      const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD Finish(
      char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD GetMaxLength(
      const PRUnichar * aSrc, PRInt32 aSrcLength,
      PRInt32 * aDestLength);

  NS_IMETHOD Reset();

  NS_IMETHOD SetOutputErrorBehavior(
      PRInt32 aBehavior,
      nsIUnicharEncoder * aEncoder, PRUnichar aChar);

  NS_IMETHOD FillInfo(PRUint32* aInfo);

protected:
  PRUnichar *mJamos;
  PRUnichar mJamosStatic[9];
  PRInt32  mJamoCount;
  PRInt32  mJamosMaxLength;
  PRInt32  mByteOff;

  PRInt32 mErrBehavior;
  PRUnichar mErrChar;
  nsCOMPtr<nsIUnicharEncoder> mErrEncoder;

private:
  NS_IMETHOD  composeHangul(char* output);
  int RenderAsPrecompSyllable (PRUnichar* aSrc, PRInt32* aSrcLength, 
                               char* aResult);
};

#endif /* nsUnicodeToJamoTTF_h___ */

