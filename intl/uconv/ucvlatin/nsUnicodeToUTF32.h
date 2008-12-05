/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jungshik Shin <jshin@mailaps.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsUnicodeToUTF32_h___
#define nsUnicodeToUTF32_h___

//----------------------------------------------------------------------
// Class nsUnicodeToUTF32 [declaration]  

/**
 * A character set converter from UTF32 to Unicode.
 * The base class for UTF32BE/UTF32LE to Unicode converters.
 * @created         08/Dec/2002
 * @author  Jungshik Shin
 */

class nsUnicodeToUTF32 : public nsIUnicodeEncoder
{
   NS_DECL_ISUPPORTS

public:

 /**
   * Class constructor.
   */
  nsUnicodeToUTF32() {mHighSurrogate = 0;}
  virtual ~nsUnicodeToUTF32() {}

protected:
  PRUnichar  mHighSurrogate;

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
                          PRInt32 * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsIUnicodeEncoder class [declaration]

  NS_IMETHOD Reset() {mHighSurrogate = 0; return NS_OK;}
  NS_IMETHOD FillInfo(PRUint32* aInfo);
  NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
                                    nsIUnicharEncoder * aEncoder, 
                                    PRUnichar aChar) 
                                    {return NS_OK;}

};

//----------------------------------------------------------------------
// Class nsUnicodeToUTF32BE [declaration]  

/**
 * A character set converter from Unicode to UTF32BE.
 * A subclass of UnicodeToUTF32.
 * @created         08/Dec/2002
 * @author  Jungshik Shin
 */

class nsUnicodeToUTF32BE : public nsUnicodeToUTF32
{
public:

  //--------------------------------------------------------------------
  // Subclassing of nsIUnicodeEncoder class [declaration]

  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
                     char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);


};

//----------------------------------------------------------------------
// Class nsUnicodeToUTF32LE [declaration]  

/**
 * A character set converter from Unicode to UTF32LE.
 * A subclass of UnicodeToUTF32.
 * @created         08/Dec/2002
 * @author  Jungshik Shin
 */

class nsUnicodeToUTF32LE : public nsUnicodeToUTF32
{
public:

  //--------------------------------------------------------------------
  // Subclassing of nsIUnicodeEncoder class [declaration]
  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
                     char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);

};

#endif /* nsUnicodeToUTF32_h___ */

