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

#ifndef nsUnicodeToTSCII_h___
#define nsUnicodeToTSCII_h___

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharRepresentable.h"

//----------------------------------------------------------------------
// Class nsUnicodeToTSCII [declaration]

class nsUnicodeToTSCII : public nsIUnicodeEncoder, public nsICharRepresentable
{

NS_DECL_ISUPPORTS

public:
  nsUnicodeToTSCII() { mBuffer = 0; }
  virtual ~nsUnicodeToTSCII() {}

  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength,
                     char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength,
                          PRInt32 * aDestLength);

  NS_IMETHOD Reset();

  NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior,
                                    nsIUnicharEncoder * aEncoder, 
                                    PRUnichar aChar);

  NS_IMETHOD FillInfo(PRUint32* aInfo);

private:
  PRUint32 mBuffer; // buffer for character(s) to be combined with the following
                    // character. Up to 4 single byte characters can be 
                    // stored. 
};

#define CHAR_BUFFER_SIZE 2048

//----------------------------------------------------------------------
// Class nsUnicodeToTamilTTF [declaration]

class nsUnicodeToTamilTTF : public nsUnicodeToTSCII
{
  NS_DECL_ISUPPORTS_INHERITED

public:
  nsUnicodeToTamilTTF() : nsUnicodeToTSCII() {}
  virtual ~nsUnicodeToTamilTTF() {}

  NS_IMETHOD Convert      (const PRUnichar * aSrc, PRInt32 * aSrcLength,
                           char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD GetMaxLength (const PRUnichar * aSrc, PRInt32  aSrcLength,
                           PRInt32 * aDestLength);

  NS_IMETHOD SetOutputErrorBehavior (PRInt32 aBehavior, 
                                     nsIUnicharEncoder *aEncoder, 
                                     PRUnichar aChar);

private:
  char mStaticBuffer[CHAR_BUFFER_SIZE];
  PRInt32 mErrBehavior;
  PRUnichar mErrChar;
  nsCOMPtr<nsIUnicharEncoder> mErrEncoder;
};

#endif /* nsUnicodeToTSCII_h___ */
