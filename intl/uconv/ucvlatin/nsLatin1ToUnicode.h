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

#ifndef nsLatin1ToUnicode_h___
#define nsLatin1ToUnicode_h___

#include "nsIUnicodeDecoder.h"

//----------------------------------------------------------------------
// Class nsLatin1ToUnicodeFactory [declaration]

//----------------------------------------------------------------------
// Class nsLatin1ToUnicode [declaration]

/**
 * A character set converter from Latin1 to Unicode.
 *
 * This particular converter does not use the general single-byte converter 
 * helper object. That is because someone may want to optimise this converter 
 * to the fullest, as it is one of the most heavily used.
 *
 * Multithreading: not an issue, the object has one instance per user thread.
 * As a plus, it is also stateless!
 * 
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsLatin1ToUnicode : public nsIUnicodeDecoder
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsLatin1ToUnicode();

  /**
   * Class destructor.
   */
  ~nsLatin1ToUnicode();

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecoder [declaration]

  NS_IMETHOD Convert(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength,const char * aSrc, PRInt32 aSrcOffset, 
      PRInt32 * aSrcLength);
  NS_IMETHOD Finish(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength);
  NS_IMETHOD Length(const char * aSrc, PRInt32 aSrcOffset, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetInputErrorBehavior(PRInt32 aBehavior);
};

#endif /* nsLatin1ToUnicode_h___ */
