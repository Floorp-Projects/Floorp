/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsMacGreekToUnicode_h___
#define nsMacGreekToUnicode_h___

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsMacGreekToUnicode [declaration]

/**
 * A character set converter from MacGreek to Unicode.
 *
 * @created         05/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsMacGreekToUnicode : public nsTableDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsMacGreekToUnicode();

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
};

#endif /* nsMacGreekToUnicode_h___ */
