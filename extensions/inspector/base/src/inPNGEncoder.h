/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __inPNGEncoder_h__
#define __inPNGEncoder_h__

#include "inIPNGEncoder.h"
#include "inIBitmap.h"

#include "nsCOMPtr.h"

class inPNGEncoder : public inIPNGEncoder 
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INIPNGENCODER

  inPNGEncoder();
  virtual ~inPNGEncoder();

protected:
  
  /*
   * Since inIBitmap stores pixel bytes in BGR order, and the png encoder 
   * needs them in RGB format, this will reverse them prior to encoding.
   */
  
  static void ReverseRGB(PRUint32 aWidth, PRUint32 aHeight, PRUint8* aBits);
};

#endif // __inPNGEncoder_h__
