/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCvJaSupport_h___
#define nsUCvJaSupport_h___

#include "nsCOMPtr.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "uconvutil.h"
#include "mozilla/Mutex.h"

#define ONE_BYTE_TABLE_SIZE 256

inline bool WillOverrun(PRUnichar* aDest, PRUnichar* aDestEnd, uint32_t aLength)
{
  NS_ASSERTION(aDest <= aDestEnd, "Pointer overrun even before check");
  return (uint32_t(aDestEnd - aDest) < aLength);
}
#define CHECK_OVERRUN(dest, destEnd, length) (WillOverrun(dest, destEnd, length))

#ifdef DEBUG
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
#ifdef DEBUG
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

  virtual void SetInputErrorBehavior(int32_t aBehavior);
  virtual PRUnichar GetCharacterForUnMapped();

protected:
  int32_t   mErrBehavior;
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
  int32_t   mBufferCapacity;
  int32_t   mBufferLength;

  uint32_t  mMaxLengthFactor;
  
  /**
   * Convert method but *without* the buffer management stuff.
   */
  NS_IMETHOD ConvertNoBuff(const char * aSrc, int32_t * aSrcLength, 
      PRUnichar * aDest, int32_t * aDestLength) = 0;

  void FillBuffer(const char ** aSrc, int32_t aSrcLength);

public:

  /**
   * Class constructor.
   */
  nsBufferDecoderSupport(uint32_t aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsBufferDecoderSupport();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecoder [declaration]

  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength, 
      PRUnichar * aDest, int32_t * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD GetMaxLength(const char *aSrc,
                          int32_t aSrcLength,
                          int32_t* aDestLength);
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
      uMappingTable * aMappingTable, uint32_t aMaxLengthFactor);

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

  NS_IMETHOD ConvertNoBuff(const char * aSrc, int32_t * aSrcLength, 
      PRUnichar * aDest, int32_t * aDestLength);
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
  nsMultiTableDecoderSupport(int32_t aTableCount, const uRange * aRangeArray, 
                             uScanClassID * aScanClassArray,
                             uMappingTable ** aMappingTable,
                             uint32_t aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsMultiTableDecoderSupport();

protected:

  int32_t                   mTableCount;
  const uRange              * mRangeArray;
  uScanClassID              * mScanClassArray;
  uMappingTable             ** mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsBufferDecoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuff(const char * aSrc, int32_t * aSrcLength, 
      PRUnichar * aDest, int32_t * aDestLength);
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

  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength, 
      PRUnichar * aDest, int32_t * aDestLength);
  NS_IMETHOD GetMaxLength(const char * aSrc, int32_t aSrcLength, 
      int32_t * aDestLength);
  NS_IMETHOD Reset();
};

//----------------------------------------------------------------------
// Class nsBasicEncoder [declaration]

class nsBasicEncoder : public nsIUnicodeEncoder
#ifdef DEBUG
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
  int32_t   mBufferCapacity;
  char *    mBufferStart;
  char *    mBufferEnd;

  /**
   * Error handling stuff
   */
  int32_t   mErrBehavior;
  nsCOMPtr<nsIUnicharEncoder> mErrEncoder;
  PRUnichar mErrChar;
  uint32_t  mMaxLengthFactor;

  /**
   * Convert method but *without* the buffer management stuff and *with* 
   * error handling stuff.
   */
  NS_IMETHOD ConvertNoBuff(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);

  /**
   * Convert method but *without* the buffer management stuff and *without*
   * error handling stuff.
   */
  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength) = 0;

  /**
   * Finish method but *without* the buffer management stuff.
   */
  NS_IMETHOD FinishNoBuff(char * aDest, int32_t * aDestLength);

  /**
   * Copy as much as possible from the internal buffer to the destination.
   */
  nsresult FlushBuffer(char ** aDest, const char * aDestEnd);

public:

  /**
   * Class constructor.
   */
  nsEncoderSupport(uint32_t aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsEncoderSupport();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncoder [declaration]

  NS_IMETHOD Convert(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
  NS_IMETHOD Finish(char * aDest, int32_t * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetOutputErrorBehavior(int32_t aBehavior, 
      nsIUnicharEncoder * aEncoder, PRUnichar aChar);
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, 
                          int32_t aSrcLength, 
                          int32_t * aDestLength);
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
                        uint32_t aMaxLengthFactor);

  nsTableEncoderSupport(uScanClassID  aScanClass,
                        uMappingTable  * aMappingTable,
                        uint32_t aMaxLengthFactor);

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

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
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
  nsMultiTableEncoderSupport(int32_t aTableCount,
                             uScanClassID * aScanClassArray,
                             uShiftOutTable ** aShiftOutTable,
                             uMappingTable  ** aMappingTable,
                             uint32_t aMaxLengthFactor);

  /**
   * Class destructor.
   */
  virtual ~nsMultiTableEncoderSupport();

protected:

  int32_t                   mTableCount;
  uScanClassID              * mScanClassArray;
  uShiftOutTable            ** mShiftOutTable;
  uMappingTable             ** mMappingTable;

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
};

                        
#endif /* nsUCvJaSupport_h___ */
