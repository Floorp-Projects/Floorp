/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSpellCheckUtils_h__
#define nsSpellCheckUtils_h__

#include "nsString.h"
#include "nsCRT.h"
#include "nsICharsetConverterManager.h"

class nsISpellChecker;
class nsITextServicesDocument;
class nsIWordBreaker;

class CharBuffer
{
public:

  PRInt32 mCapacity;
  char *mData;
  PRInt32 mDataLength;

  CharBuffer() : mCapacity(0), mData(0), mDataLength(0) {}

  ~CharBuffer()
  {
    if (mData)
      delete []mData;

    mData       = 0;
    mCapacity   = 0;
    mDataLength = 0;
  }

  nsresult AssureCapacity(PRInt32 aLength)
  {
    if (aLength >  mCapacity)
    {
      if (mData)
        delete []mData;

      mData = new char[aLength];

      if (!mData)
        return NS_ERROR_OUT_OF_MEMORY;

      mCapacity = aLength;
    }

    return NS_OK;
  }
};

/** implementation of a text services object.
 *
 */
class nsSpellCheckUtils 
{
public:
  static nsresult ReadStringIntoBuffer(nsIUnicodeEncoder* aUnicodeEncoder,
                                       const PRUnichar*   aStr, 
                                       CharBuffer*        aBuf);

  static nsresult ReadStringIntoBuffer(nsIUnicodeEncoder* aUnicodeEncoder,
                                       const nsString*    aStr, 
                                       CharBuffer*        aBuf);


  static nsresult CreateUnicodeConverters(const PRUnichar*    aCharset,
                                          nsIUnicodeEncoder** aUnicodeEncoder,
                                          nsIUnicodeDecoder** aUnicodeDecoder);

  static nsresult LoadTextBlockIntoBuffer(nsITextServicesDocument* aTxtSvcDoc,
                                          nsISpellChecker* aSpellChecker,
                                          CharBuffer&          aCharBuf,
                                          nsString&            aText, 
                                          PRUint32&            aOffset);

  // helper
  static nsresult GetWordBreaker(nsIWordBreaker** aResult);

#ifdef NS_DEBUG
  static nsresult DumpWords(nsIWordBreaker*  aWordBreaker, 
                            const PRUnichar* aText, 
                            const PRUint32&  aTextLen);
#define DUMPWORDS(_wb, _txt, _txtLen) nsSpellCheckUtils::DumpWords(_wb, _txt, _txtLen);
#else
#define DUMPWORDS(_wb, _txt, _txtLen)
#endif

private:

  /** The default constructor.
   */
  nsSpellCheckUtils() {}

  /** The default destructor.
   */
  virtual ~nsSpellCheckUtils() {}

};

#endif // nsSpellCheckUtils_h__
