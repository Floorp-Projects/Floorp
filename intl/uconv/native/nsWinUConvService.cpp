/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#include <windows.h>

#include "nsNativeUConvService.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharRepresentable.h"
#include "nsIPlatformCharset.h"
#include "nsIServiceManager.h"

#include "pldhash.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsURLProperties.h"

extern nsURLProperties* gCodePageInfo;


// maps charset names to windows codepages

static PLDHashTable* gCharsetToCPMap = nsnull;

struct CodePageMapEntry : public PLDHashEntryHdr
{
    const char* mCharset;
    UINT mCodePage;
};

    
static const struct PLDHashTableOps charsetMapOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashStringKey,
    PL_DHashMatchStringKey,
    PL_DHashMoveEntryStub,
    PL_DHashFreeStringKey,
    PL_DHashFinalizeStub,
    nsnull
};

BOOL CALLBACK AddCodePageEntry(LPTSTR aCodePageString)
{
    CodePageMapEntry* entry =
        NS_STATIC_CAST(CodePageMapEntry*,
                       PL_DHashTableOperate(gCharsetToCPMap, aCodePageString, PL_DHASH_ADD));

    // always continue
    if (!entry) return TRUE;
    
    // add an entry
    PRInt32 error;
    entry->mCodePage = nsCAutoString(aCodePageString).ToInteger(&error);
    // XXX handle error

    nsAutoString charset;
    gCodePageInfo->Get(NS_LITERAL_STRING("acp.") +
                       NS_ConvertASCIItoUCS2(aCodePageString), charset);
    
    entry->mCharset = ToNewCString(charset);

    printf("AddCodePageEntry(%d -> %s)\n", entry->mCodePage, entry->mCharset);

#if 1
    // now try GetCPInfoEx

    CPINFOEX cpinfo;
    
    // now have to look this up with GetCPInfoEx()
    GetCPInfoEx(entry->mCodePage, 0, &cpinfo);
    // XXX handle error
    printf(" --> %s\n", cpinfo.CodePageName);

#endif
    return TRUE;
}

static
void InitCharsetToCPMap() {
    NS_ASSERTION(!gCharsetToCPMap, "Double-init of charset table");

    gCharsetToCPMap = new PLDHashTable;
    PL_DHashTableInit(gCharsetToCPMap, &charsetMapOps,
                      nsnull, sizeof(CodePageMapEntry), 10);
    EnumSystemCodePages(AddCodePageEntry, CP_INSTALLED); // CP_SUPPORTED
}

static void
FreeCharsetToCPMap() {
    // free up codepage strings
}

static UINT CharsetToCodePage(const char* aCharset)
{
    CodePageMapEntry* entry =
        NS_STATIC_CAST(CodePageMapEntry*,
                       PL_DHashTableOperate(gCharsetToCPMap, aCharset,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(entry))
        return 0;

    return entry->mCodePage;
}

class WinUConvAdapter : public nsIUnicodeDecoder,
                        public nsIUnicodeEncoder,
                        public nsICharRepresentable
{
public:

    WinUConvAdapter();
    virtual ~WinUConvAdapter();

    nsresult Init(const char* from, const char* to);

    NS_DECL_ISUPPORTS;
    
    // Decoder methods:
    
    NS_IMETHOD Convert(const char * aSrc, 
                       PRInt32 * aSrcLength, 
                       PRUnichar * aDest, 
                       PRInt32 * aDestLength);
    
    NS_IMETHOD GetMaxLength(const char * aSrc, 
                            PRInt32 aSrcLength, 
                            PRInt32 * aDestLength);
    NS_IMETHOD Reset();
    
    // Encoder methods:
    
    NS_IMETHOD Convert(const PRUnichar * aSrc, 
                       PRInt32 * aSrcLength, 
                       char * aDest, 
                       PRInt32 * aDestLength);
    
    
    NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);
    
    NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, 
                            PRInt32 aSrcLength, 
                            PRInt32 * aDestLength);
    
    // defined by the Decoder:  NS_IMETHOD Reset();
    
    NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
                                      nsIUnicharEncoder * aEncoder, 
                                      PRUnichar aChar);
    
    NS_IMETHOD FillInfo(PRUint32* aInfo);
    
private:
    static PRUint32 gRefCnt;
};

PRUint32 WinUConvAdapter::gRefCnt = 0;

NS_IMPL_ISUPPORTS3(WinUConvAdapter,
                   nsIUnicodeDecoder,
                   nsIUnicodeEncoder,
                   nsICharRepresentable)

WinUConvAdapter::WinUConvAdapter()
{
    if (++gRefCnt == 1) {
        nsCOMPtr<nsIPlatformCharset> platformCharset =
            do_GetService(NS_PLATFORMCHARSET_CONTRACTID);
        InitCharsetToCPMap();
    }
}

WinUConvAdapter::~WinUConvAdapter()
{
    if (--gRefCnt == 0) {
        FreeCharsetToCPMap();
    }
}

nsresult
WinUConvAdapter::Init(const char* from, const char* to)
{
    UINT fromCP = CharsetToCodePage(from);
    UINT toCP = CharsetToCodePage(to);

    printf("WinUConvAdapter::Init(%s -> %d, %s -> %d)\n",
           from, fromCP,
           to, toCP);

    return NS_OK;
}

NS_IMETHODIMP
WinUConvAdapter::Convert(const char * aSrc, 
                         PRInt32 * aSrcLength, 
                         PRUnichar * aDest, 
                         PRInt32 * aDestLength)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
    
NS_IMETHODIMP
WinUConvAdapter::GetMaxLength(const char * aSrc, 
                              PRInt32 aSrcLength, 
                              PRInt32 * aDestLength)
{
    return NS_ERROR_NOT_IMPLEMENTED;

}
NS_IMETHODIMP
WinUConvAdapter::Reset()
{

    return NS_ERROR_NOT_IMPLEMENTED;
}
    
// Encoder methods:
    
NS_IMETHODIMP
WinUConvAdapter::Convert(const PRUnichar * aSrc, 
                         PRInt32 * aSrcLength, 
                         char * aDest, 
                         PRInt32 * aDestLength)
{

    return NS_ERROR_NOT_IMPLEMENTED;
}
    
    
NS_IMETHODIMP
WinUConvAdapter::Finish(char * aDest, PRInt32 * aDestLength)
{

    return NS_ERROR_NOT_IMPLEMENTED;
}
    
NS_IMETHODIMP
WinUConvAdapter::GetMaxLength(const PRUnichar * aSrc, 
                            PRInt32 aSrcLength, 
                            PRInt32 * aDestLength)
{

    return NS_ERROR_NOT_IMPLEMENTED;
}
    
// defined by the Decoder:  NS_IMETHOD Reset();
    
NS_IMETHODIMP
WinUConvAdapter::SetOutputErrorBehavior(PRInt32 aBehavior, 
                                      nsIUnicharEncoder * aEncoder, 
                                      PRUnichar aChar)
{

    return NS_ERROR_NOT_IMPLEMENTED;
}
    
NS_IMETHODIMP
WinUConvAdapter::FillInfo(PRUint32* aInfo)
{

    return NS_ERROR_NOT_IMPLEMENTED;
}
    
// NativeUConvService

NS_IMPL_ISUPPORTS1(NativeUConvService, 
                   nsINativeUConvService);

NS_IMETHODIMP 
NativeUConvService::GetNativeConverter(const char* from,
                                       const char* to,
                                       nsISupports** aResult) 
{
    *aResult = nsnull;

    WinUConvAdapter* ucl = new WinUConvAdapter();
    if (!ucl)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = ucl->Init(from, to);

    if (NS_SUCCEEDED(rv)) {
        NS_ADDREF(*aResult = (nsISupports*)(nsIUnicharEncoder*)ucl);
    }

    return rv;
}
