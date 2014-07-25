/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSaveAsCharset_h__
#define nsSaveAsCharset_h__

#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsISaveAsCharset.h"
#include "nsCOMPtr.h"

#define MASK_FALLBACK(a) (nsISaveAsCharset::mask_Fallback & (a))
#define MASK_ENTITY(a) (nsISaveAsCharset::mask_Entity & (a))
#define MASK_CHARSET_FALLBACK(a) (nsISaveAsCharset::mask_CharsetFallback & (a))
#define ATTR_NO_FALLBACK(a) (nsISaveAsCharset::attr_FallbackNone == MASK_FALLBACK(a) && \
                             nsISaveAsCharset::attr_EntityAfterCharsetConv != MASK_ENTITY(a))

class nsIUnicodeEncoder;
class nsIEntityConverter;

class nsSaveAsCharset : public nsISaveAsCharset
{
public:
	
	//
	// implementation methods
	//
  nsSaveAsCharset();

	//
	// nsISupports
	//
	NS_DECL_ISUPPORTS

	//
	// nsIEntityConverter
	//
  NS_IMETHOD Init(const char *charset, uint32_t attr, uint32_t entityVersion);

  NS_IMETHOD Convert(const char16_t *inString, char **_retval);

  NS_IMETHODIMP GetCharset(char * *aCharset);

protected:

  virtual ~nsSaveAsCharset();

  NS_IMETHOD DoCharsetConversion(const char16_t *inString, char **outString);

  NS_IMETHOD DoConversionFallBack(uint32_t inUCS4, char *outString, int32_t bufferLength);

  // do the fallback, reallocate the buffer if necessary
  // need to pass destination buffer info (size, current position and estimation of rest of the conversion)
  NS_IMETHOD HandleFallBack(uint32_t character, char **outString, int32_t *bufferLength, 
                            int32_t *currentPos, int32_t estimatedLength);

  nsresult SetupUnicodeEncoder(const char* charset);

  nsresult SetupCharsetList(const char *charsetList);

  const char * GetNextCharset();

  uint32_t mAttribute;                    // conversion attribute
  uint32_t mEntityVersion;                // see nsIEntityConverter
  nsCOMPtr<nsIUnicodeEncoder> mEncoder;   // encoder (convert from unicode)
  nsCOMPtr<nsIEntityConverter> mEntityConverter;
  nsTArray<nsCString> mCharsetList;
  int32_t        mCharsetListIndex;
};

#endif
