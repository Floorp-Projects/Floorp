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

#ifndef nsIUnicodeEncoder_h___
#define nsIUnicodeEncoder_h___

#include "nscore.h"
#include "nsISupports.h"

// Interface ID for our Unicode Encoder interface
// {2B2CA3D0-A4C9-11d2-8AA1-00600811A836}
NS_DECLARE_ID(kIUnicodeEncoderIID,
  0x2b2ca3d0, 0xa4c9, 0x11d2, 0x8a, 0xa1, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

/**
 * Interface for a Converter from Unicode into a Charset.
 *
 * XXX Think, write and doc the error handling.
 * XXX Write error value macros
 *
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsIUnicodeEncoder : public nsISupports
{
public:

  /**
   * Converts the data from Unicode to a Charset.
   *
   * About the byte ordering:
   * - The input stream is Unicode, having the byte order which is internal
   * for the machine on which the converter is running on.
   * - For output, if the converter cares (that depends of the charset, for 
   * example a singlebyte will ignore the byte ordering) it should assume 
   * network order. If necessary and requested, we can add a method 
   * SetOutputByteOrder() so that the reverse order can be used, too. That 
   * method would have as default the assumed network order.
   *
   * Unless there is not enough output space, this method must consume all the
   * available input data! We don't have partial input for the Unicode charset.
   *
   * @param aSrc        [IN] the source data buffer
   * @param aSrcOffset  [IN] the offset in the source data buffer
   * @param aSrcLength  [IN/OUT] the length of source data buffer; after
   *                    conversion will contain the number of Unicode 
   *                    characters read
   * @param aDest       [OUT] the destination data buffer
   * @param aDestOffset [IN] the offset in the destination data buffer
   * @param aDestLength [IN/OUT] the length of the destination data buffer;
   *                    after conversion will contain the number of bytes
   *                    written
   * @return            XXX doc me
   */
  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 aSrcOffset, 
      PRInt32 * aSrcLength, char * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength) = 0;

  /**
   * Finishes the conversion. The converter has the possibility to write some 
   * extra data and flush its final state.
   *
   * @param aDest       [OUT] the destination data buffer
   * @param aDestOffset [IN] the offset in the destination data buffer
   * @param aDestLength [IN/OUT] the length of destination data buffer; after
   *                    conversion it will contain the number of bytes written
   * @return            XXX doc me
   */
  NS_IMETHOD Finish(char * aDest, PRInt32 aDestOffset, PRInt32 * aDestLength)
      = 0;

  /**
   * Returns a quick estimation of the size of the buffer needed to hold the
   * converted data. Remember: this is an estimation and not necessarily 
   * correct. Its purpose is to help the caller allocate the destination 
   * buffer.
   *
   * @param aSrc        [IN] the source data buffer
   * @param aSrcOffset  [IN] the offset in the source data buffer
   * @param aSrcLength  [IN] the length of source data buffer
   * @param aDestLength [OUT] the needed size of the destination buffer
   * @return            XXX doc me
   */
  NS_IMETHOD Length(const PRUnichar * aSrc, PRInt32 aSrcOffset, 
      PRInt32 aSrcLength, PRInt32 * aDestLength) = 0;
  /**
   * Resets the charset converter so it may be recycled for a completely 
   * different and urelated buffer of data.
   */
  NS_IMETHOD Reset() = 0;
};

#endif /* nsIUnicodeEncoder_h___ */
