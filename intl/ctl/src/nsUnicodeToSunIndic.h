/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ucvhi : nsUnicodeToSunIndic.h
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by SUN are Copyright (C) 2000 SUN
 * Microsystems, Inc. All Rights Reserved.
 * 
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */
#ifndef nsUnicodeToSunIndic_h___
#define nsUnicodeToSunIndic_h___

#include "nspr.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIModule.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharRepresentable.h"

#include "nsILE.h"

//----------------------------------------------------------------------
// Class nsUnicodeToSunIndic [declaration]

class nsUnicodeToSunIndic : public nsIUnicodeEncoder, public nsICharRepresentable
{
NS_DECL_ISUPPORTS

public:
  nsUnicodeToSunIndic();
  virtual ~nsUnicodeToSunIndic();

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
  PRUint8 mState;
  PRInt32 mByteOff;
  PRInt32 mCharOff;

  nsCOMPtr<nsILE> mCtlObj;

  PRInt32 mErrBehavior;
  PRUnichar mErrChar;
  nsIUnicharEncoder* mErrEncoder;
};
#endif /* !nsUnicodeToSunIndic_h___ */
