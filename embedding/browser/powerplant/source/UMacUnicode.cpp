/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com>
 */

#include "UMacUnicode.h"
#include "nsString.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIServiceManager.h"
#define NS_IMPL_IDS
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsICharsetConverterManager.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

static nsIUnicodeEncoder     *gUnicodeEncoder;
static nsIUnicodeDecoder     *gUnicodeDecoder;

static void GetFileSystemCharset(nsString & fileSystemCharset);


void UMacUnicode::ReleaseUnit()
{
   NS_IF_RELEASE(gUnicodeEncoder);
   NS_IF_RELEASE(gUnicodeDecoder);
}


void UMacUnicode::StringToStr255(const nsString& aText, Str255& aStr255)
{
	char buffer[256];
	nsresult rv = NS_OK;
	
	// get file system charset and create a unicode encoder
	if (nsnull == gUnicodeEncoder) {
		nsAutoString fileSystemCharset;
		GetFileSystemCharset(fileSystemCharset);

		NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &rv); 
		if (NS_SUCCEEDED(rv)) {
			rv = ccm->GetUnicodeEncoder(&fileSystemCharset, &gUnicodeEncoder);
		}
	}

	// converts from unicode to the file system charset
	if (NS_SUCCEEDED(rv)) {
		PRInt32 inLength = aText.Length();
		PRInt32 outLength = 255;
		rv = gUnicodeEncoder->Convert(aText.GetUnicode(), &inLength, (char *) &aStr255[1], &outLength);
		if (NS_SUCCEEDED(rv))
			aStr255[0] = outLength;
	}

	if (NS_FAILED(rv)) {
//		NS_ASSERTION(0, "error: charset covnersion");
		aText.ToCString(buffer, 255);
		PRInt32 len = nsCRT::strlen(buffer);
		memcpy(&aStr255[1], buffer, len);
		aStr255[0] = len;
	}
}


void UMacUnicode::Str255ToString(const Str255& aStr255, nsString& aText)
{
	nsresult rv = NS_OK;
	
	// get file system charset and create a unicode encoder
	if (nsnull == gUnicodeDecoder) {
		nsAutoString fileSystemCharset;
		GetFileSystemCharset(fileSystemCharset);

		NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &rv); 
		if (NS_SUCCEEDED(rv)) {
			rv = ccm->GetUnicodeDecoder(&fileSystemCharset, &gUnicodeDecoder);
		}
	}
  
	// converts from the file system charset to unicode
	if (NS_SUCCEEDED(rv)) {
		PRUnichar buffer[512];
		PRInt32 inLength = aStr255[0];
		PRInt32 outLength = 512;
		rv = gUnicodeDecoder->Convert((char *) &aStr255[1], &inLength, buffer, &outLength);
		if (NS_SUCCEEDED(rv)) {
			aText.Assign(buffer, outLength);
		}
	}
	
	if (NS_FAILED(rv)) {
//		NS_ASSERTION(0, "error: charset covnersion");
		aText.AssignWithConversion((char *) &aStr255[1], aStr255[0]);
	}
}


static void GetFileSystemCharset(nsString & fileSystemCharset)
{
  static nsAutoString aCharset;
  nsresult rv;

  if (aCharset.Length() < 1) {
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
	  if (NS_SUCCEEDED(rv)) 
		  rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");
	  if (NS_FAILED(rv)) 
		  aCharset.AssignWithConversion("x-mac-roman");
  }
  fileSystemCharset = aCharset;
}
