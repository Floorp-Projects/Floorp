/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 /**
 * A character set converter from Unicode to GBK (no ascii).
 * 
 *
 * @created     Jan/24/2001
 * @author      Brian Stell bstell@netscape.com
 * Revision History
 *
 */

#include "nsUnicodeToGBKNoAscii.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGBK [implementation]

NS_IMETHODIMP nsUnicodeToGBKNoAscii::FillInfo(PRUint32 *aInfo)
{
  nsresult rv = nsUnicodeToGBK::FillInfo(aInfo); // call the super class
  if(NS_SUCCEEDED(rv))
  {
    // mark the first 128 bits as 0. 4 x 32 bits = 128 bits
    aInfo[0] = aInfo[1] = aInfo[2] = aInfo[3] = 0;
  }
  return rv;
}

