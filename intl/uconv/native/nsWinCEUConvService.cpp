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

#include "nsUCSupport.h"
#include "nsUTF8ToUnicode.h"
#include "nsUnicodeToUTF8.h"

#ifdef ALERT_DBG
void DisplayLastError(const char * msg)
{
  int flags = MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND;
    int error = GetLastError();
    switch (error)
    {
    case ERROR_NO_UNICODE_TRANSLATION:
      MessageBox(0, "ERROR_NO_UNICODE_TRANSLATION", msg, flags);
      break;
    case ERROR_INVALID_PARAMETER:
      MessageBox(0, "ERROR_INVALID_PARAMETER", msg, flags);
      break;
    case ERROR_INVALID_FLAGS:
      MessageBox(0, "ERROR_INVALID_FLAGS", msg, flags);
      break;
    case ERROR_INSUFFICIENT_BUFFER:
      MessageBox(0, "ERROR_INSUFFICIENT_BUFFER", msg, flags);
      break;
    default:
      MessageBox(0, "other...", msg, flags);
    }
}
#endif


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
  
  virtual void SetInputErrorBehavior(PRInt32 aBehavior);
  virtual PRUnichar GetCharacterForUnMapped();

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
  
  PRUint32 mCodepage;
};

NS_IMPL_ISUPPORTS3(WinCEUConvAdapter,
                   nsIUnicodeDecoder,
                   nsIUnicodeEncoder,
                   nsICharRepresentable)

WinCEUConvAdapter::WinCEUConvAdapter()
{
  mCodepage = -1;
}

WinCEUConvAdapter::~WinCEUConvAdapter()
{
}

typedef struct CharsetCodePageMap {
  char      charset[32];
  PRUint16  codepage;
} CsCpMap;


static CsCpMap theCsCPMap[] = {
  {"Big5",    950},
  {"EUC-JP", 51932},     
  {"EUC-KR", 949},  
  {"GB2312", 936},  
  {"HZ-GB-2312    ", 52936},     
  {"IBM850", 850},   
  {"IBM852", 852},   
  {"IBM855", 855},   
  {"IBM857", 857},   
  {"IBM862", 862},   
  {"IBM866", 866},   
  {"IBM869", 869},   
  {"ISO-2022-JP", 50220},     
  {"ISO-2022-KR", 50225},     
  {"ISO-8859-15", 28605},     
  {"ISO-8859-1", 28591},     
  {"ISO-8859-2", 28592},     
  {"ISO-8859-3", 28593},     
  {"ISO-8859-4", 28594},     
  {"ISO-8859-5", 28595},     
  {"ISO-8859-6", 28596},     
  {"ISO-8859-7", 28597},     
  {"ISO-8859-8", 28598},     
  {"ISO-8859-8-I", 1255},      
  {"ISO-8859-9", 28599},     
  {"Shift_JIS", 932},       
  {"TIS-620", 874},       
  {"UTF-16", 1200},      
  {"UTF-7", 65000},     
  {"UTF-8", 65001},     
  {"gb_2312-80", 936},       
  {"ks_c_5601-1987", 949},       
  {"us-ascii", 20127},     
  {"windows-1250", 1250},      
  {"windows-1251", 1251},      
  {"windows-1252", 1252},      
  {"windows-1253", 1253},      
  {"windows-1254", 1254},      
  {"windows-1255", 1255},      
  {"windows-1256", 1256},      
  {"windows-1257", 1257},      
  {"windows-1258", 1258},      
  {"windows-874", 874},       
  {"windows-936", 936},       
  {"x-mac-arabic", 10004},     
  {"x-mac-ce", 10029},     
  {"x-mac-cyrillic", 10007},     
  {"x-mac-greek", 10006},     
  {"x-mac-hebrew", 10005},     
  {"x-mac-icelandi", 10079},     
  {"x-x-big5", 950},       
  {nsnull, 0}
};


nsresult
WinCEUConvAdapter::Init(const char* from, const char* to)
{
  const char* cpstring = nsnull;
  
  if (!strcmp(from, "UCS-2"))
  {
    cpstring = to;
  }
  else
  {
    cpstring = from;
  }
  
  int i = 0;
  while (1)
  {
    if (theCsCPMap[i].charset[0] == nsnull)
      break;
    
    if (!strcmp(theCsCPMap[i].charset, cpstring))
    {
      mCodepage = theCsCPMap[i].codepage;
      break;
    }
    i++;
  }
  
  if (mCodepage == -1)
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

NS_IMETHODIMP
WinCEUConvAdapter::Convert(const char * aSrc, 
                           PRInt32 * aSrcLength, 
                           PRUnichar * aDest, 
                           PRInt32 * aDestLength)
{
  if (mCodepage == -1)
    return NS_ERROR_FAILURE;
  
  int count = MultiByteToWideChar(mCodepage,
                                  0,
                                  aSrc,
                                  *aSrcLength,
                                  aDest,
                                  *aDestLength);
  
  if (count == 0 && GetLastError() == ERROR_INVALID_PARAMETER)
  {
    // fall back on the current system Windows "ANSI" code page
    count = MultiByteToWideChar(CP_ACP,
                                0,
                                aSrc,
                                *aSrcLength,
                                aDest,
                                *aDestLength);
  }
  
#ifdef ALERT_DBG
  if (count == 0)
    DisplayLastError("MultiByteToWideChar");
#endif
  
  *aDestLength = count;
  *aSrcLength  = count;
  return NS_OK;
}

NS_IMETHODIMP
WinCEUConvAdapter::GetMaxLength(const char * aSrc, 
                                PRInt32 aSrcLength, 
                                PRInt32 * aDestLength)
{
  if (mCodepage == -1 || aSrc == nsnull )
    return NS_ERROR_FAILURE;
  
  int count = MultiByteToWideChar(mCodepage,
                                  MB_PRECOMPOSED,
                                  aSrc,
                                  aSrcLength,
                                  NULL,
                                  NULL);
  
  if (count == 0 && GetLastError() == ERROR_INVALID_PARAMETER)
  {
    // fall back on the current system Windows "ANSI" code page
    
    count = MultiByteToWideChar(CP_ACP,
                                MB_PRECOMPOSED,
                                aSrc,
                                aSrcLength,
                                NULL,
                                NULL);
  }
  
#ifdef ALERT_DBG  
  if (count == 0)
    DisplayLastError("MultiByteToWideChar (0)");
#endif
  
  *aDestLength = count;
  return NS_OK;
}

NS_IMETHODIMP
WinCEUConvAdapter::Reset()
{
  return NS_OK;
}

void
WinCEUConvAdapter::SetInputErrorBehavior(PRInt32 aBehavior)
{
}

PRUnichar
WinCEUConvAdapter::GetCharacterForUnMapped()
{
  return PRUnichar(0xfffd); // Unicode REPLACEMENT CHARACTER
}

// Encoder methods:

NS_IMETHODIMP
WinCEUConvAdapter::Convert(const PRUnichar * aSrc, 
                           PRInt32 * aSrcLength, 
                           char * aDest, 
                           PRInt32 * aDestLength)
{
  if (mCodepage == -1)
    return NS_ERROR_FAILURE;
  
  char * defaultChar = "?";
  int count = WideCharToMultiByte(mCodepage,
                                  0,
                                  aSrc,
                                  *aSrcLength,
                                  aDest,
                                  *aDestLength,
                                  defaultChar,
                                  NULL);
  
#ifdef ALERT_DBG
  if (count == 0)
    DisplayLastError("WideCharToMultiByte");
#endif
  
  *aSrcLength = count;
  *aDestLength = count;
  
  return NS_OK;
}


NS_IMETHODIMP
WinCEUConvAdapter::Finish(char * aDest, PRInt32 * aDestLength)
{
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP
WinCEUConvAdapter::GetMaxLength(const PRUnichar * aSrc, 
                                PRInt32 aSrcLength, 
                                PRInt32 * aDestLength)
{
  if (mCodepage == -1)
    return NS_ERROR_FAILURE;
  
  int count = WideCharToMultiByte(mCodepage,
                                  0,
                                  aSrc,
                                  aSrcLength,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL);
#ifdef ALERT_DBG
  if (count == 0)
    DisplayLastError("WideCharToMultiByte (0)");
#endif
  
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
  
  
  
  if (!strcmp(from, "UCS-2") && 
      !strcmp(to,   "UTF-8") )
  {
    nsUnicodeToUTF8 * inst = new nsUnicodeToUTF8();
    inst->AddRef();
    *aResult = inst;
    return NS_OK;
  }
  
  if (!strcmp(from, "UTF-8") &&
      !strcmp(to,   "UCS-2") )
  {
    nsUTF8ToUnicode * inst = new nsUTF8ToUnicode();
    inst->AddRef();
    *aResult = (nsIUnicodeDecoder*) inst;
    return NS_OK;
  }
  
  WinCEUConvAdapter* ucl = new WinCEUConvAdapter();
  if (!ucl)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = ucl->Init(from, to);
  
  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(*aResult = (nsISupports*)(nsIUnicharEncoder*)ucl);
  }
  
  return rv;
}
