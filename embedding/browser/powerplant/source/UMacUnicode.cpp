/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "UMacUnicode.h"

#include <TextCommon.h>
#include <Script.h>

#include "nsString.h"

static TextEncoding getSystemEncoding()
{
    OSStatus err;
    TextEncoding theEncoding;
    
    err = ::UpgradeScriptInfoToTextEncoding(smSystemScript, kTextLanguageDontCare,
        kTextRegionDontCare, NULL, &theEncoding);
    
    if (err != noErr)
        theEncoding = kTextEncodingMacRoman;
    
    return theEncoding;
}

CPlatformUCSConversion *CPlatformUCSConversion::mgInstance = nsnull; 
UnicodeToTextInfo CPlatformUCSConversion::sEncoderInfo = nsnull;
TextToUnicodeInfo CPlatformUCSConversion::sDecoderInfo = nsnull;

CPlatformUCSConversion::CPlatformUCSConversion()
{
}


CPlatformUCSConversion*
CPlatformUCSConversion::GetInstance()
{
    if (!mgInstance)
        mgInstance = new CPlatformUCSConversion;
        
    return mgInstance;
}


NS_IMETHODIMP 
CPlatformUCSConversion::PrepareEncoder()
{
    nsresult rv = NS_OK;
    if (!sEncoderInfo) {
        OSStatus err;
        err = ::CreateUnicodeToTextInfoByEncoding(getSystemEncoding(), &sEncoderInfo);
        if (err)
            rv = NS_ERROR_FAILURE;
    }
    return rv;
}


NS_IMETHODIMP 
CPlatformUCSConversion::PrepareDecoder()
{
    nsresult rv = NS_OK;
    if (!sDecoderInfo) {
        OSStatus err;
        err = ::CreateTextToUnicodeInfoByEncoding(getSystemEncoding(), &sDecoderInfo);
        if (err)
            rv = NS_ERROR_FAILURE;
    }
    return rv;
}


NS_IMETHODIMP 
CPlatformUCSConversion::UCSToPlatform(const nsAString& aIn, nsACString& aOut)
{
    nsresult rv = PrepareEncoder();
    if (NS_FAILED(rv)) return rv;
    
    OSStatus err = noErr;
    char stackBuffer[512];

    aOut.Truncate(0);
    nsReadingIterator<PRUnichar> done_reading;
    aIn.EndReading(done_reading);

    // for each chunk of |aIn|...
    PRUint32 fragmentLength = 0;
    nsReadingIterator<PRUnichar> iter;
    for (aIn.BeginReading(iter); iter != done_reading && err == noErr; iter.advance(PRInt32(fragmentLength)))
    {
        fragmentLength = PRUint32(iter.size_forward());        
        UInt32 bytesLeft = fragmentLength * sizeof(UniChar);
        nsReadingIterator<PRUnichar> sub_iter(iter);
        
        do {
            UInt32 bytesRead = 0, bytesWritten = 0;
            err = ::ConvertFromUnicodeToText(sEncoderInfo,
                                             bytesLeft,
                                             (const UniChar*)sub_iter.get(),
                                             kUnicodeUseFallbacksMask | kUnicodeLooseMappingsMask,
                                             0, nsnull, nsnull, nsnull,
                                             sizeof(stackBuffer),
                                             &bytesRead,
                                             &bytesWritten,
                                             stackBuffer);
            if (err == kTECUsedFallbacksStatus)
                err = noErr;
            else if (err == kTECOutputBufferFullStatus) {
                bytesLeft -= bytesRead;
                sub_iter.advance(bytesRead / sizeof(UniChar));
            }
            aOut.Append(stackBuffer, bytesWritten);
        }
        while (err == kTECOutputBufferFullStatus);
    }
    return (err == noErr) ? NS_OK : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CPlatformUCSConversion::UCSToPlatform(const nsAString& aIn, Str255& aOut)
{
    nsresult res;
    nsCAutoString cStr;
    
    res = UCSToPlatform(aIn, cStr);
    if (NS_SUCCEEDED(res))
    {
        PRUint32 outLength = cStr.Length();
        if (outLength > 255)
            outLength = 255;
        memcpy(&aOut[1], cStr.get(), outLength);
        aOut[0] = outLength;
    }
    return res;
}


NS_IMETHODIMP 
CPlatformUCSConversion::PlatformToUCS(const nsACString& aIn, nsAString& aOut)
{
    nsresult rv = PrepareDecoder();
    if (NS_FAILED(rv)) return rv;
    
    OSStatus err = noErr;
    UniChar stackBuffer[512];

    aOut.Truncate(0);
    nsReadingIterator<char> done_reading;
    aIn.EndReading(done_reading);

    // for each chunk of |aIn|...
    PRUint32 fragmentLength = 0;
    nsReadingIterator<char> iter;
    for (aIn.BeginReading(iter); iter != done_reading && err == noErr; iter.advance(PRInt32(fragmentLength)))
    {
        fragmentLength = PRUint32(iter.size_forward());        
        UInt32 bytesLeft = fragmentLength;
        nsReadingIterator<char> sub_iter(iter);
        
        do {
            UInt32 bytesRead = 0, bytesWritten = 0;
            err = ::ConvertFromTextToUnicode(sDecoderInfo,
                                             bytesLeft,
                                             sub_iter.get(),
                                             kUnicodeUseFallbacksMask | kUnicodeLooseMappingsMask,
                                             0, nsnull, nsnull, nsnull,
                                             sizeof(stackBuffer),
                                             &bytesRead,
                                             &bytesWritten,
                                             stackBuffer);
            if (err == kTECUsedFallbacksStatus)
                err = noErr;
            else if (err == kTECOutputBufferFullStatus) {
                bytesLeft -= bytesRead;
                sub_iter.advance(bytesRead);
            }
            aOut.Append((PRUnichar *)stackBuffer, bytesWritten / sizeof(PRUnichar));
        }
        while (err == kTECOutputBufferFullStatus);
    }
    return (err == noErr) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
CPlatformUCSConversion::PlatformToUCS(const Str255& aIn, nsAString& aOut)
{
    char charBuf[256];
    
    memcpy(charBuf, &aIn[1], aIn[0]);
    charBuf[aIn[0]] = '\0';
    return PlatformToUCS(nsDependentCString(charBuf, aIn[0]), aOut);
}
