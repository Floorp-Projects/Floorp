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

#ifndef nsUCvLatinSupport_h___
#define nsUCvLatinSupport_h___

#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsIUnicodeDecodeHelper.h"

//----------------------------------------------------------------------
// Class nsEncoderSupport [declaration]

/**
 * Support class for the Unicode encoders. 
 *
 * The class source files for this class are in /ucvlatin/nsUCvLatinSupport. 
 * However, because these objects requires non-xpcom subclassing, local copies
 * will be made into the other directories using them. Just don't forget to 
 * keep in sync with the master copy!
 * 
 * This class implements:
 * - nsISupports
 * - the buffer management
 * - error handling procedure(s)
 *
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsEncoderSupport : public nsIUnicodeEncoder
{
  NS_DECL_ISUPPORTS

protected:

  /**
   * Internal buffer for partial conversions.
   */
  char *    mBuffer;
  PRInt32   mBufferCapacity;
  char *    mBufferStart;
  char *    mBufferEnd;

  /**
   * Error handling stuff
   */
  PRInt32   mErrBehavior;
  nsIUnicharEncoder * mErrEncoder;
  PRUnichar mErrChar;

  /**
   * Convert method but *without* the buffer management stuff and *with* 
   * error handling stuff.
   */
  NS_IMETHOD ConvertNoBuff(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);

  /**
   * Convert method but *without* the buffer management stuff and *without*
   * error handling stuff.
   */
  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength) = 0;

  /**
   * Finish method but *without* the buffer management stuff.
   */
  NS_IMETHOD FinishNoBuff(char * aDest, PRInt32 * aDestLength);

  /**
   * Copy as much as possible from the internal buffer to the destination.
   */
  nsresult FlushBuffer(char ** aDest, const char * aDestEnd);

public:

  /**
   * Class constructor.
   */
  nsEncoderSupport();

  /**
   * Class destructor.
   */
  virtual ~nsEncoderSupport();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncoder [declaration]

  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
      nsIUnicharEncoder * aEncoder, PRUnichar aChar);
};

//----------------------------------------------------------------------
// Class nsTableEncoderSupport [declaration]

/**
 * Support class for a single-table-driven Unicode encoder.
 * 
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsTableEncoderSupport : public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsTableEncoderSupport(uShiftTable * aShiftTable, 
      uMappingTable  * aMappingTable);

  /**
   * Class destructor.
   */
  virtual ~nsTableEncoderSupport();

protected:

  nsIUnicodeEncodeHelper    * mHelper;      // encoder helper object
  uShiftTable               * mShiftTable;
  uMappingTable             * mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
};

//----------------------------------------------------------------------
// Class nsTablesEncoderSupport [declaration]

/**
 * Support class for a multi-table-driven Unicode encoder.
 * 
 * @created         11/Mar/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsTablesEncoderSupport : public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsTablesEncoderSupport(PRInt32 aTableCount, uShiftTable ** aShiftTable,
      uMappingTable  ** aMappingTable);

  /**
   * Class destructor.
   */
  virtual ~nsTablesEncoderSupport();

protected:

  nsIUnicodeEncodeHelper    * mHelper;      // encoder helper object
  PRInt32                   mTableCount;
  uShiftTable               ** mShiftTable;
  uMappingTable             ** mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
};

//----------------------------------------------------------------------
// Class nsDecoderSupport [declaration]

/**
 * Support class for the Unicode decoders. 
 *
 * This class implements:
 * - nsISupports
 * - the buffer management
 *
 * @created         15/Mar/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsDecoderSupport : public nsIUnicodeDecoder
{
  NS_DECL_ISUPPORTS

protected:

  /**
   * Internal buffer for partial conversions.
   */
  char *    mBuffer;
  PRInt32   mBufferCapacity;
  PRInt32   mBufferLength;

  /**
   * Convert method but *without* the buffer management stuff.
   */
  NS_IMETHOD ConvertNoBuff(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength) = 0;

  void FillBuffer(const char ** aSrc, PRInt32 aSrcLength);
  void DoubleBuffer();

public:

  /**
   * Class constructor.
   */
  nsDecoderSupport();

  /**
   * Class destructor.
   */
  virtual ~nsDecoderSupport();

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength) = 0;

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecoder [declaration]

  NS_IMETHOD Convert(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength, const char * aSrc, PRInt32 aSrcOffset, 
      PRInt32 * aSrcLength);
  NS_IMETHOD Finish(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength);
  NS_IMETHOD Length(const char * aSrc, PRInt32 aSrcOffset, 
      PRInt32 aSrcLength, PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetInputErrorBehavior(PRInt32 aBehavior);
};

//----------------------------------------------------------------------
// Class nsTableDecoderSupport [declaration]

/**
 * Support class for a single-table-driven Unicode decoder.
 * 
 * @created         15/Mar/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsTableDecoderSupport : public nsDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsTableDecoderSupport(uShiftTable * aShiftTable, 
      uMappingTable  * aMappingTable);

  /**
   * Class destructor.
   */
  virtual ~nsTableDecoderSupport();

protected:

  nsIUnicodeDecodeHelper    * mHelper;      // decoder helper object
  uShiftTable               * mShiftTable;
  uMappingTable             * mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuff(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
};

#endif /* nsUCvLatinSupport_h___ */
