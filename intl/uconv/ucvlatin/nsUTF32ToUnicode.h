/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Contributor(s): Jungshik Shin <jshin@mailaps.org>
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

#ifndef nsUTF32ToUnicode_h___
#define nsUTF32ToUnicode_h___

//----------------------------------------------------------------------
// Class nsUTF32ToUnicode [declaration]  

/**
 * A character set converter from UTF32 to Unicode.
 * The base class for UTF32BE/UTF32LE to Unicode converters.
 * @created         08/Dec/2002
 * @author  Jungshik Shin
 */

class nsUTF32ToUnicode : public nsBasicDecoderSupport
{

public:

  /**
   * Class constructor.
   */
  nsUTF32ToUnicode();

protected:

  // the number of additional bytes to read to complete an incomplete UTF-32 4byte seq.
  PRUint16 mState;  
  // buffer for an incomplete UTF-32 sequence. 
  PRUint8  mBufferInc[4];

  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
                          PRInt32 * aDestLength);

  NS_IMETHOD Reset();

};

//----------------------------------------------------------------------
// Class nsUTF32BEToUnicode [declaration]  

/**
 * A character set converter from UTF32BE to Unicode.
 * A subclass of UTF32ToUnicode.
 * @created         08/Dec/2002
 * @author  Jungshik Shin
 */

class nsUTF32BEToUnicode : public nsUTF32ToUnicode
{
public:


  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength, 
                     PRUnichar * aDest, PRInt32 * aDestLength);


};

//----------------------------------------------------------------------
// Class nsUTF32LEToUnicode [declaration]  

/**
 * A character set converter from UTF32LE to Unicode.
 * A subclass of UTF32ToUnicode.
 * @created         08/Dec/2002
 * @author  Jungshik Shin
 */

class nsUTF32LEToUnicode : public nsUTF32ToUnicode
{
public:


  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength, 
                     PRUnichar * aDest, PRInt32 * aDestLength);

};

#endif /* nsUTF32ToUnicode_h___ */

