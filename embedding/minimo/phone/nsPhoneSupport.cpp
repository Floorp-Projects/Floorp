/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Phone Support for Minimo
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>
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

#ifdef WINCE
#include "windows.h"
#include "phone.h"
#include "sms.h"
#endif

#include "nsString.h"
#include "nsIPhoneSupport.h"
#include "nsIGenericFactory.h"

void errormsg(DWORD dw, LPTSTR lpszFunction) 
{ 
  TCHAR szBuf[80]; 
  LPVOID lpMsgBuf;
  
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                dw,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0, NULL );
  
  wsprintf(szBuf, 
           "%s failed with error %d: %s", 
           lpszFunction, dw, lpMsgBuf); 
  
  MessageBox(NULL, szBuf, "Error", MB_OK); 
  
  LocalFree(lpMsgBuf);
}

class nsPhoneSupport : public nsIPhoneSupport
{
public:
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPHONESUPPORT

  nsPhoneSupport() {};
  ~nsPhoneSupport(){};

};

NS_IMPL_ISUPPORTS1(nsPhoneSupport, nsIPhoneSupport)



static char* normalizePhoneNumber(const char* inTelephoneNumber)
{
  return strdup(inTelephoneNumber);
}

NS_IMETHODIMP
nsPhoneSupport::MakeCall(const char *telephoneNumber, const char *telephoneDescription)
{
#ifdef WINCE
  long result = -1;
  char* normalizedNumber = normalizePhoneNumber(telephoneNumber);

  if (!normalizedNumber)
    return NS_ERROR_FAILURE;
  
  typedef LONG (*__PhoneMakeCall)(PHONEMAKECALLINFO *ppmci);
  
  HMODULE hPhoneDLL = LoadLibrary("phone.dll"); 
  if(hPhoneDLL)
  {
    __PhoneMakeCall MakeCall = (__PhoneMakeCall) GetProcAddress( hPhoneDLL,
                                                                 "PhoneMakeCall");
    if(MakeCall)
    {
      PHONEMAKECALLINFO callInfo;
      callInfo.cbSize          = sizeof(PHONEMAKECALLINFO);
      callInfo.dwFlags         = PMCF_PROMPTBEFORECALLING;
      callInfo.pszDestAddress  = NS_ConvertUTF8toUTF16(normalizedNumber).get();
      callInfo.pszAppName      = nsnull;
      callInfo.pszCalledParty  = NS_ConvertUTF8toUTF16(telephoneDescription).get();
      callInfo.pszComment      = nsnull; 
      
      result = MakeCall(&callInfo);
    }
    FreeLibrary(hPhoneDLL);
  } 

  free(normalizedNumber);

  return (result == 0) ? NS_OK : NS_ERROR_FAILURE;

#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP 
nsPhoneSupport::SendSMS(const char *smsDest, const char *smsMessage)
{
#ifdef WINCE

  // wince -- this doesn't work yet.

  char* normalizedNumber = normalizePhoneNumber(smsDest);

  if (!normalizedNumber)
    return NS_ERROR_FAILURE;

  typedef HRESULT (*__SmsOpen)(const LPCTSTR ptsMessageProtocol,
                               const DWORD dwMessageModes,
                               SMS_HANDLE* const psmshHandle,
                               HANDLE* const phMessageAvailableEvent);
  
  typedef HRESULT (*__SmsSendMessage)(const SMS_HANDLE smshHandle,
                                      const SMS_ADDRESS * const psmsaSMSCAddress,
                                      const SMS_ADDRESS * const psmsaDestinationAddress,
                                      const SYSTEMTIME * const pstValidityPeriod,
                                      const BYTE * const pbData,
                                      const DWORD dwDataSize,
                                      const BYTE * const pbProviderSpecificData,
                                      const DWORD dwProviderSpecificDataSize,
                                      const SMS_DATA_ENCODING smsdeDataEncoding,
                                      const DWORD dwOptions,
                                      SMS_MESSAGE_ID * psmsmidMessageID);

  typedef HRESULT (*__SmsClose)(const SMS_HANDLE smshHandle);


  HMODULE hSmsDLL = LoadLibrary("sms.dll"); 
  if(hSmsDLL)
  {
    __SmsOpen        Open  = (__SmsOpen) GetProcAddress(hSmsDLL, "SmsOpen");
    __SmsSendMessage Send  = (__SmsSendMessage) GetProcAddress(hSmsDLL, "SmsSendMessage");
    __SmsClose       Close = (__SmsClose) GetProcAddress(hSmsDLL, "SmsClose");
      

    SMS_HANDLE smshHandle;
    SMS_ADDRESS smsaDestination;
    TEXT_PROVIDER_SPECIFIC_DATA tpsd;
    SMS_MESSAGE_ID smsmidMessageID = 0;
    
    // try to open an SMS Handle
    HRESULT hr = SMS_E_INVALIDPROTOCOL;
    hr = Open(SMS_MSGTYPE_TEXT, SMS_MODE_SEND, &smshHandle, NULL);
    
    if (hr == SMS_E_INVALIDPROTOCOL)
      return NS_ERROR_NOT_AVAILABLE;
    
    if (hr != ERROR_SUCCESS)
      return NS_ERROR_FAILURE;
    
    // Create the destination address
    memset (&smsaDestination, 0, sizeof (smsaDestination));
    smsaDestination.smsatAddressType = SMSAT_INTERNATIONAL;
    lstrcpy(smsaDestination.ptsAddress, normalizedNumber);
    
    free(normalizedNumber);
    
    // Set up provider specific data
    tpsd.dwMessageOptions = PS_MESSAGE_OPTION_NONE;
    tpsd.psMessageClass   = PS_MESSAGE_CLASS0;
    tpsd.psReplaceOption  = PSRO_NONE;
    
    // Send the message, indicating success or failure
    hr = Send(smshHandle,
              NULL, 
              &smsaDestination,
              NULL,
              (PBYTE) smsMessage, 
              strlen(smsMessage)+1, 
              (PBYTE) &tpsd, 
              sizeof(TEXT_PROVIDER_SPECIFIC_DATA), /*12*/ 
              SMSDE_OPTIMAL, 
              SMS_OPTION_DELIVERY_NONE, 
              &smsmidMessageID);
    
    Close (smshHandle);
    
    if (hr == ERROR_SUCCESS)
      return NS_OK;

    FreeLibrary(hSmsDLL);
  }
  return NS_ERROR_FAILURE;

#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}


//------------------------------------------------------------------------------
//  XPCOM REGISTRATION BELOW
//------------------------------------------------------------------------------

#define nsPhoneSupport_CID \
{ 0x2a08c9e4, 0xf853, 0x4f02, \
  {0x88, 0xd8, 0xd6, 0x2f, 0x27, 0xca, 0x06, 0x85} }

#define nsPhoneSupport_ContractID "@mozilla.org/phone/support;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPhoneSupport)

static const nsModuleComponentInfo components[] =
{
  { "Phone Support", 
    nsPhoneSupport_CID, 
    nsPhoneSupport_ContractID,
    nsPhoneSupportConstructor,
    nsnull,
    nsnull
  }  
};

NS_IMPL_NSGETMODULE(nsPhoneSupportModule, components)
