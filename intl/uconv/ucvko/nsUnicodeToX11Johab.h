/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsUnicodeToX11Johab_h___
#define nsUnicodeToX11Johab_h___

#include "nsUCvKOSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToX11Johab [declaration]

class nsUnicodeToX11Johab : public nsIUnicodeEncoder, public nsICharRepresentable
{

NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsUnicodeToX11Johab();
  ~nsUnicodeToX11Johab();

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
private:
  PRUint8 state;
  PRUint8 l;
  PRUint8 v;
  PRUint8 t;
  PRInt32 byteOff;
  PRInt32 charOff;
  void composeHangul(char* output);
};

#endif /* nsUnicodeToX11Johab_h___ */
