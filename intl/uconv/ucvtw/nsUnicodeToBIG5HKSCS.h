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
 * Gavin Ho
 */

#ifndef nsUnicodeToBIG5HKSCS_h___
#define nsUnicodeToBIG5HKSCS_h___

#include "nsUCvTWSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToBIG5HKSCS [declaration]

/**
 * A character set converter from Unicode to BIG5-HKSCS.
 *
 * @created         02/Jul/2000
 * @author  Gavin Ho, Hong Kong Professional Services, Compaq Computer (Hong Kong) Ltd.
 */
class nsUnicodeToBIG5HKSCS : public nsMultiTableEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToBIG5HKSCS();

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength,
      PRInt32 * aDestLength);
};

#endif /* nsUnicodeToBIG5HKSCS_h___ */
