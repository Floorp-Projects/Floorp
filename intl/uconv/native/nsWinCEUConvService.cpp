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
 * The Original Code is WinCEUConvAdapter for Windows CE
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <windows.h>

#include "nsNativeUConvService.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharRepresentable.h"
#include "nsIPlatformCharset.h"
#include "nsIServiceManager.h"

class WinCEUConvAdapter : public nsIUnicodeDecoder,
                        public nsIUnicodeEncoder,
                        public nsICharRepresentable
{
public:

    WinCEUConvAdapter();
    virtual ~WinCEUConvAdapter();

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
    
};

NS_IMPL_ISUPPORTS3(WinCEUConvAdapter,
                   nsIUnicodeDecoder,
                   nsIUnicodeEncoder,
                   nsICharRepresentable)

WinCEUConvAdapter::WinCEUConvAdapter()
{
}

WinCEUConvAdapter::~WinCEUConvAdapter()
{
}

nsresult
WinCEUConvAdapter::Init(const char* from, const char* to)
{
    return NS_OK;
}

NS_IMETHODIMP
WinCEUConvAdapter::Convert(const char * aSrc, 
                         PRInt32 * aSrcLength, 
                         PRUnichar * aDest, 
                         PRInt32 * aDestLength)
{
    int count = MultiByteToWideChar(CP_ACP,
                                    0,
                                    aSrc,
                                    *aSrcLength,
                                    aDest,
                                    *aDestLength);
    *aDestLength = count;
    *aSrcLength  = count;
    return NS_OK;
}
    
NS_IMETHODIMP
WinCEUConvAdapter::GetMaxLength(const char * aSrc, 
                              PRInt32 aSrcLength, 
                              PRInt32 * aDestLength)
{
    int count = MultiByteToWideChar(CP_ACP,
                                    0,
                                    aSrc,
                                    aSrcLength,
                                    NULL,
                                    NULL);
    
    *aDestLength = count;
    return NS_OK;

}
NS_IMETHODIMP
WinCEUConvAdapter::Reset()
{
    return NS_OK;
}
    
// Encoder methods:
    
NS_IMETHODIMP
WinCEUConvAdapter::Convert(const PRUnichar * aSrc, 
                         PRInt32 * aSrcLength, 
                         char * aDest, 
                         PRInt32 * aDestLength)
{
    char * defaultChar = "?";
    int count = WideCharToMultiByte(CP_ACP,
                                    WC_COMPOSITECHECK | WC_SEPCHARS | WC_DEFAULTCHAR,
                                    aSrc,
                                    *aSrcLength,
                                    aDest,
                                    *aDestLength,
                                    defaultChar,
                                    NULL);

    *aSrcLength = count;
    *aDestLength = count;

    return NS_OK;
}
    
    
NS_IMETHODIMP
WinCEUConvAdapter::Finish(char * aDest, PRInt32 * aDestLength)
{
    return NS_OK;
}
    
NS_IMETHODIMP
WinCEUConvAdapter::GetMaxLength(const PRUnichar * aSrc, 
                            PRInt32 aSrcLength, 
                            PRInt32 * aDestLength)
{
    int count = WideCharToMultiByte(CP_ACP,
                                    0,
                                    aSrc,
                                    aSrcLength,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);

    *aDestLength = count;
    return NS_OK;
}
    
// defined by the Decoder:  NS_IMETHOD Reset();
    
NS_IMETHODIMP
WinCEUConvAdapter::SetOutputErrorBehavior(PRInt32 aBehavior, 
                                      nsIUnicharEncoder * aEncoder, 
                                      PRUnichar aChar)
{
    return NS_OK;
}
    
NS_IMETHODIMP
WinCEUConvAdapter::FillInfo(PRUint32* aInfo)
{
    return NS_OK;
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

    WinCEUConvAdapter* ucl = new WinCEUConvAdapter();
    if (!ucl)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = ucl->Init(from, to);

    if (NS_SUCCEEDED(rv)) {
        NS_ADDREF(*aResult = (nsISupports*)(nsIUnicharEncoder*)ucl);
    }

    return rv;
}
