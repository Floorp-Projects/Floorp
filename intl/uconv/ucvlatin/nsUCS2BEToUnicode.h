/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUCS2BEToUnicode_h___
#define nsUCS2BEToUnicode_h___

#include "nsISupports.h"
#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsUCS2BEToUnicode [declaration]

/**
 * A character set converter from UCS2BE to Unicode.
 *
 * @created         18/Mar/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsUCS2BEToUnicode : public nsTableDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUCS2BEToUnicode();

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
nsresult NEW_UTF16BEToUnicode(nsISupports **Result);
nsresult NEW_UTF16LEToUnicode(nsISupports **Result);

//============== above code is obsolete ==============================

class nsUTF16BEToUnicode : public nsBasicDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUTF16BEToUnicode() { Reset();};

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength,
      PRUnichar * aDest, PRInt32 * aDestLength); 
  NS_IMETHOD Reset();

protected:
  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
protected:
  PRUint8 mState;
  PRUint8 mData;
};

class nsUTF16LEToUnicode : public nsBasicDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUTF16LEToUnicode() { Reset();};

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength,
      PRUnichar * aDest, PRInt32 * aDestLength); 
  NS_IMETHOD Reset();

protected:
  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
protected:
  PRUint8 mState;
  PRUint8 mData;
};

#endif /* nsUCS2BEToUnicode_h___ */
