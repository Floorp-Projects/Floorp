/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsUCvJaSupport_h___
#define nsUCvJaSupport_h___

#include "nsCOMPtr.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "uconvutil.h"
#include "mozilla/Mutex.h"

#define ONE_BYTE_TABLE_SIZE 256

#ifdef NS_DEBUG
// {7AFC9F0A-CFE1-44ea-A755-E3B86AB1226E}
#define NS_IBASICDECODER_IID \
{ 0x7afc9f0a, 0xcfe1, 0x44ea, { 0xa7, 0x55, 0xe3, 0xb8, 0x6a, 0xb1, 0x22, 0x6e } }

// {65968A7B-6467-4c4a-B50A-3E0C97A32F07}
#define NS_IBASICENCODER_IID \
{ 0x65968a7b, 0x6467, 0x4c4a, { 0xb5, 0xa, 0x3e, 0xc, 0x97, 0xa3, 0x2f, 0x7 } }

class nsIBasicDecoder : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBASICDECODER_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIBasicDecoder, NS_IBASICDECODER_IID)

class nsIBasicEncoder : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBASICENCODER_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIBasicEncoder, NS_IBASICENCODER_IID)

#endif

//----------------------------------------------------------------------
// Class nsBasicDecoderSupport [declaration]

/**
 * Support class for the Unicode decoders. 
 *
 * The class source files for this class are in /ucvlatin/nsUCvJaSupport. 
 * However, because these objects requires non-xpcom subclassing, local copies
 * will be made into the other directories using them. Just don't forget to 
 * keep in sync with the master copy!
 * 
 * This class implements:
 * - nsISupports
 * - nsIUnicodeDecoder
 *
 * @created         19/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsBasicDecoderSupport : public nsIUnicodeDecoder
#ifdef NS_DEBUG
                              ,public nsIBasicDecoder
#endif
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsBasicDecoderSupport();

  /**
   * Class destructor.
   */
  virtual ~nsBasicDecoderSupport();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecoder [declaration]

  virtual void SetInputErrorBehavior(PRInt32 aBehavior);
  virtual PRUnichar GetCharacterForUnMapped();

protected:
  PRInt32   mErrBehavior;
};

//----------------------------------------------------------------------
// Class nsBufferDecoderSupport [declaration]

/**
 * Support class for the Unicode decoders. 
 *
 * This class implements:
 * - the buffer management
 *
 * @created         15/Mar/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsBufferDecoderSupport : public nsBasicDecoderSupport
{
protected:

  /**
   * Internal buffer for partial conversions.
   */
  char *    mBuffer;
  PRInt32   mBufferCapacity;
  PRInt32   mBufferLength;

  PRUint32  mMaxLengthFactor;
  
  /**
   * Convert method but *without* the buffer management stuff.
   */
  NS_IMETHOD ConvertNoBuff(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength) = 0;

  void FillBuffer(const char ** aSrc, PRInt32 aSrcLength);

public:

  /**
   * Class constructor.
   */
  nsBufferDecoderSupport(PRUint32 aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsBufferDecoderSupport();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecoder [declaration]

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD GetMaxLength(const char *aSrc,
                          PRInt32 aSrcLength,
                          PRInt32* aDestLength);
};

//----------------------------------------------------------------------
// Class nsTableDecoderSupport [declaration]

/**
 * Support class for a single-table-driven Unicode decoder.
 * 
 * @created         15/Mar/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsTableDecoderSupport : public nsBufferDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsTableDecoderSupport(uScanClassID aScanClass, uShiftInTable * aShiftInTable,
      uMappingTable * aMappingTable, PRUint32 aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsTableDecoderSupport();

protected:

  uScanClassID              mScanClass;
  uShiftInTable             * mShiftInTable;
  uMappingTable             * mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsBufferDecoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuff(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
};

//----------------------------------------------------------------------
// Class nsMultiTableDecoderSupport [declaration]

/**
 * Support class for a multi-table-driven Unicode decoder.
 * 
 * @created         24/Mar/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsMultiTableDecoderSupport : public nsBufferDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsMultiTableDecoderSupport(PRInt32 aTableCount, const uRange * aRangeArray, 
                             uScanClassID * aScanClassArray,
                             uMappingTable ** aMappingTable,
                             PRUint32 aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsMultiTableDecoderSupport();

protected:

  PRInt32                   mTableCount;
  const uRange              * mRangeArray;
  uScanClassID              * mScanClassArray;
  uMappingTable             ** mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsBufferDecoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuff(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
};

//----------------------------------------------------------------------
// Class nsBufferDecoderSupport [declaration]

/**
 * Support class for a single-byte Unicode decoder.
 *
 * @created         19/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsOneByteDecoderSupport : public nsBasicDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsOneByteDecoderSupport(uMappingTable * aMappingTable);

  /**
   * Class destructor.
   */
  virtual ~nsOneByteDecoderSupport();

protected:

  uMappingTable             * mMappingTable;
  PRUnichar                 mFastTable[ONE_BYTE_TABLE_SIZE];
  bool                      mFastTableCreated;
  mozilla::Mutex            mFastTableMutex;

  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Reset();
};

//----------------------------------------------------------------------
// Class nsBasicEncoder [declaration]

class nsBasicEncoder : public nsIUnicodeEncoder
#ifdef NS_DEBUG
                       ,public nsIBasicEncoder
#endif
{
  NS_DECL_ISUPPORTS

public:
  /**
   * Class constructor.
   */
  nsBasicEncoder();

  /**
   * Class destructor.
   */
  virtual ~nsBasicEncoder();

};
//----------------------------------------------------------------------
// Class nsEncoderSupport [declaration]

/**
 * Support class for the Unicode encoders. 
 *
 * This class implements:
 * - nsISupports
 * - the buffer management
 * - error handling procedure(s)
 *
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsEncoderSupport :  public nsBasicEncoder
{

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
  nsCOMPtr<nsIUnicharEncoder> mErrEncoder;
  PRUnichar mErrChar;
  PRUint32  mMaxLengthFactor;

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
  nsEncoderSupport(PRUint32 aMaxLengthFactor);

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
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, 
                          PRInt32 aSrcLength, 
                          PRInt32 * aDestLength);
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
   * Class constructors.
   */
  nsTableEncoderSupport(uScanClassID  aScanClass,
                        uShiftOutTable * aShiftOutTable,
                        uMappingTable  * aMappingTable,
                        PRUint32 aMaxLengthFactor);

  nsTableEncoderSupport(uScanClassID  aScanClass,
                        uMappingTable  * aMappingTable,
                        PRUint32 aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsTableEncoderSupport();

protected:

  uScanClassID              mScanClass;
  uShiftOutTable            * mShiftOutTable;
  uMappingTable             * mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
};

//----------------------------------------------------------------------
// Class nsMultiTableEncoderSupport [declaration]

/**
 * Support class for a multi-table-driven Unicode encoder.
 * 
 * @created         11/Mar/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsMultiTableEncoderSupport : public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsMultiTableEncoderSupport(PRInt32 aTableCount,
                             uScanClassID * aScanClassArray,
                             uShiftOutTable ** aShiftOutTable,
                             uMappingTable  ** aMappingTable,
                             PRUint32 aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsMultiTableEncoderSupport();

protected:

  PRInt32                   mTableCount;
  uScanClassID              * mScanClassArray;
  uShiftOutTable            ** mShiftOutTable;
  uMappingTable             ** mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
};

                        
#endif /* nsUCvJaSupport_h___ */
