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

#ifndef nsUnicodeToJohabNoAscii_h___
#define nsUnicodeToJohabNoAscii_h___

#include "nsUCvKOSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToJohabNoAscii [declaration]

/**
 * A character set converter from Unicode to Johab Sun Font encoding
 *  ksc5601_1992-3  (Johab without US-ASCII)
 */
class nsUnicodeToJohabNoAscii : public nsMultiTableEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToJohabNoAscii();

  NS_IMETHOD GetMaxLength( const PRUnichar* aSrc, PRInt32 aSrcLength, 
                           PRInt32* aDestLength);
};

#endif /* nsUnicodeToJohabNoAscii_h___ */
