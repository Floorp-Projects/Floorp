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
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#include "UMacUnicode.h"
#include "nsString.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

CPlatformUCSConversion *CPlatformUCSConversion::mgInstance = nsnull; 

CPlatformUCSConversion::CPlatformUCSConversion() :
    mCharsetSel(kPlatformCharsetSel_FileName)
{
   mEncoder = nsnull;
   mDecoder = nsnull;
}


CPlatformUCSConversion*
CPlatformUCSConversion::GetInstance()
{
    if (!mgInstance)
        mgInstance = new CPlatformUCSConversion;
        
    return mgInstance;
}


NS_IMETHODIMP
CPlatformUCSConversion::SetCharsetSelector(nsPlatformCharsetSel aSel)
{
    if (mCharsetSel != aSel) {
        mCharsetSel = aSel;
        mPlatformCharset.Truncate(0);
        mEncoder = nsnull;
        mDecoder = nsnull;
    }
    return NS_OK;
}


NS_IMETHODIMP 
CPlatformUCSConversion::PreparePlatformCharset()
{
   nsresult res = NS_OK;

   if (mPlatformCharset.Length() == 0)
   { 
     NS_WITH_SERVICE(nsIPlatformCharset, pcharset, NS_PLATFORMCHARSET_CONTRACTID, &res);
     if (!(NS_SUCCEEDED(res) && pcharset)) {
       NS_WARNING("cannot get platform charset");
     }
     if(NS_SUCCEEDED(res) && pcharset) {
        res = pcharset->GetCharset(mCharsetSel, mPlatformCharset);
     } 
   }
   return res;
}


NS_IMETHODIMP 
CPlatformUCSConversion::PrepareEncoder()
{
   nsresult res = NS_OK;

   if(! mEncoder)
   {
       res = PreparePlatformCharset();
       if(NS_SUCCEEDED(res)) {
           NS_WITH_SERVICE(nsICharsetConverterManager,
                ucmgr, NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && ucmgr), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && ucmgr) 
               res = ucmgr->GetUnicodeEncoder( &mPlatformCharset, getter_AddRefs(mEncoder));
           NS_ASSERTION((NS_SUCCEEDED(res) && mEncoder), 
                   "cannot find the unicode encoder");
       }
   }
   return res;
}


NS_IMETHODIMP 
CPlatformUCSConversion::PrepareDecoder()
{
   nsresult res = NS_OK;

   if(! mDecoder)
   {
       res = PreparePlatformCharset();
       if(NS_SUCCEEDED(res)) {
           NS_WITH_SERVICE(nsICharsetConverterManager,
                ucmgr, NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && ucmgr), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && ucmgr) 
               res = ucmgr->GetUnicodeDecoder( &mPlatformCharset, getter_AddRefs(mDecoder));
           NS_ASSERTION((NS_SUCCEEDED(res) && mDecoder), 
                   "cannot find the unicode decoder");
       }
   }
   return res;
}


NS_IMETHODIMP 
CPlatformUCSConversion::UCSToPlatform(const nsAReadableString& aIn, nsAWritableCString& aOut)
{
    nsresult res;

    aOut.Truncate(0);
    res = PrepareEncoder();
    if(NS_SUCCEEDED(res)) 
    {        
        nsReadingIterator<PRUnichar> done_reading;
        aIn.EndReading(done_reading);

        // for each chunk of |aIn|...
        PRUint32 fragmentLength = 0;
        nsReadingIterator<PRUnichar> iter;
        for (aIn.BeginReading(iter); iter != done_reading && NS_SUCCEEDED(res); iter.advance(PRInt32(fragmentLength)))
        {
            fragmentLength = PRUint32(iter.size_forward());
            PRInt32 inLength = fragmentLength;
            PRInt32 outLength;

            res = mEncoder->GetMaxLength(iter.get(), fragmentLength, &outLength);
            if (NS_SUCCEEDED(res))
            {
                char *outBuf = (char*)nsMemory::Alloc(outLength);
                if (outBuf)
                {
                    res = mEncoder->Convert(iter.get(), &inLength, outBuf,  &outLength);
                    if (NS_SUCCEEDED(res))
                        aOut.Append(outBuf, outLength);
                    nsMemory::Free(outBuf);
                }
                else
                    res = NS_ERROR_OUT_OF_MEMORY;
            }
        }
   }
   return res;
}


NS_IMETHODIMP
CPlatformUCSConversion::UCSToPlatform(const nsAReadableString& aIn, Str255& aOut)
{
    nsresult res;
    nsCAutoString cStr;
    
    res = UCSToPlatform(aIn, cStr);
    if (NS_SUCCEEDED(res))
    {
        PRUint32 outLength = cStr.Length();
        if (outLength > 255)
            outLength = 255;
        memcpy(&aOut[1], cStr.GetBuffer(), outLength);
        aOut[0] = outLength;
    }
    return res;
}


NS_IMETHODIMP 
CPlatformUCSConversion::PlatformToUCS(const nsAReadableCString& aIn, nsAWritableString& aOut)
{
   nsresult res;

    aOut.Truncate(0);
    res = PrepareDecoder();
    if (NS_SUCCEEDED(res)) 
    {
        nsReadingIterator<char> done_reading;
        aIn.EndReading(done_reading);

        // for each chunk of |aIn|...
        PRUint32 fragmentLength = 0;
        nsReadingIterator<char> iter;
        for (aIn.BeginReading(iter); iter != done_reading && NS_SUCCEEDED(res); iter.advance(PRInt32(fragmentLength)))
        {
            fragmentLength = PRUint32(iter.size_forward());
            PRInt32 inLength = fragmentLength;
            PRInt32 outLength;

            res = mDecoder->GetMaxLength(iter.get(), inLength, &outLength);
            if (NS_SUCCEEDED(res))
            {
               PRUnichar *outBuf = (PRUnichar*)nsMemory::Alloc(outLength * sizeof(PRUnichar));
               if (outBuf)
               {
                  res = mDecoder->Convert(iter.get(), &inLength, outBuf,  &outLength);
                  if(NS_SUCCEEDED(res))
                     aOut.Append(outBuf, outLength);
                  nsMemory::Free(outBuf);
               }
               else
                  res = NS_ERROR_OUT_OF_MEMORY;
            }
        }
   }
   return res;
}

NS_IMETHODIMP 
CPlatformUCSConversion::PlatformToUCS(const Str255& aIn, nsAWritableString& aOut)
{
    char charBuf[256];
    
    memcpy(charBuf, &aIn[1], aIn[0]);
    charBuf[aIn[0]] = '\0';
    return PlatformToUCS(nsLiteralCString(charBuf), aOut);
}
