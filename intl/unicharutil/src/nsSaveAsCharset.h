/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSaveAsCharset_h__
#define nsSaveAsCharset_h__

#include "nsIFactory.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsICharsetConverterManager.h"
#include "nsISaveAsCharset.h"


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
  virtual ~nsSaveAsCharset();

	//
	// nsISupports
	//
	NS_DECL_ISUPPORTS

	//
	// nsIEntityConverter
	//
  NS_IMETHOD Init(const char *charset, PRUint32 attr, PRUint32 entityVersion);

  NS_IMETHOD Convert(const PRUnichar *inString, char **_retval);

  NS_IMETHODIMP GetCharset(char * *aCharset);

protected:

  NS_IMETHOD DoCharsetConversion(const PRUnichar *inString, char **outString);

  NS_IMETHOD DoConversionFallBack(PRUint32 inUCS4, char *outString, PRInt32 bufferLength);

  // do the fallback, reallocate the buffer if necessary
  // need to pass destination buffer info (size, current position and estimation of rest of the conversion)
  NS_IMETHOD HandleFallBack(PRUint32 character, char **outString, PRInt32 *bufferLength, 
                            PRInt32 *currentPos, PRInt32 estimatedLength);

  nsresult SetupUnicodeEncoder(const char* charset);

  nsresult SetupCharsetList(const char *charsetList);

  const char * GetNextCharset();

  PRUint32 mAttribute;                    // conversion attribute
  PRUint32 mEntityVersion;                // see nsIEntityConverter
  nsCOMPtr<nsIUnicodeEncoder> mEncoder;   // encoder (convert from unicode)
  nsCOMPtr<nsIEntityConverter> mEntityConverter;
  nsTArray<nsCString> mCharsetList;
  PRInt32        mCharsetListIndex;
};

#endif
