/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIFactory.h"
#include "nsString.h"
#include "nsISaveAsCharset.h"


#define MASK_FALLBACK(a) (nsISaveAsCharset::mask_Fallback & (a))
#define MASK_ENTITY(a) (nsISaveAsCharset::mask_Entity & (a))
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

protected:

  NS_IMETHOD DoCharsetConversion(const PRUnichar *inString, char **outString);

  NS_IMETHOD DoConversionFallBack(PRUnichar inCharacter, char *outString, PRInt32 bufferLength);

  // do the fallback, reallocate the buffer if necessary
  // need to pass destination buffer info (size, current position and estimation of rest of the conversion)
  NS_IMETHOD HandleFallBack(PRUnichar character, char **outString, PRInt32 *bufferLength, 
                            PRInt32 *currentPos, PRInt32 estimatedLength);


  PRUint32 mAttribute;                    // conversion attribute
  PRUint32 mEntityVersion;                // see nsIEntityConverter
  nsIUnicodeEncoder *mEncoder;            // encoder (convert from unicode)
  nsIEntityConverter *mEntityConverter;
};


nsresult NS_NewSaveAsCharset(nsISupports **inst);

