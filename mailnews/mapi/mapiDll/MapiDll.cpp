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
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *           Krishna Mohan Khandrika (kkhandrika@netscape.com)
 *           Rajiv Dayal (rdayal@netscape.com)
 *           David Bienvenu <bienvenu@nventure.com>
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
#include <tchar.h>
#include <mapidefs.h>
#include <mapi.h>
#include "msgMapi.h"
#include "msgMapiMain.h"

#define MAX_RECIPS  100
#define MAX_FILES   100


#define           MAX_NAME_LEN    256
#define           MAX_PW_LEN      256
#define           MAX_MSGINFO_LEN 512
#define           MAX_POINTERS    32

const CLSID CLSID_CMapiImp = {0x29f458be, 0x8866, 0x11d5,
                              {0xa3, 0xdd, 0x0, 0xb0, 0xd0, 0xf3, 0xba, 0xa7}};
const IID IID_nsIMapi = {0x6EDCD38E,0x8861,0x11d5,
                        {0xA3,0xDD,0x00,0xB0,0xD0,0xF3,0xBA,0xA7}};

DWORD tId = 0;

#define   MAPI_MESSAGE_TYPE     0
#define   MAPI_RECIPIENT_TYPE   1
 
typedef struct {
  LPVOID    lpMem;
  UCHAR     memType;
} memTrackerType;


// this can't be right.
memTrackerType    memArray[MAX_POINTERS];

//
// For remembering memory...how ironic.
//
void
SetPointerArray(LPVOID ptr, BYTE type)
{
int i;
  
  for (i=0; i<MAX_POINTERS; i++)
  {
    if (memArray[i].lpMem == NULL)
    {
      memArray[i].lpMem = ptr;
      memArray[i].memType = type;
      break;
    }
  }
}


BOOL WINAPI DllMain(HINSTANCE aInstance, DWORD aReason, LPVOID aReserved)
{
    switch (aReason)
    {
        case DLL_PROCESS_ATTACH : tId = TlsAlloc();
                                  if (tId == 0xFFFFFFFF)
                                      return FALSE;
                                  break;

        case DLL_PROCESS_DETACH : TlsFree(tId);
                                  break;
    }
    return TRUE;
}

BOOL InitMozillaReference(nsIMapi **aRetValue)
{
    // Check whether this thread has a valid Interface
    // by looking into thread-specific-data variable

    *aRetValue = (nsIMapi *)TlsGetValue(tId);

    // Check whether the pointer actually resolves to
    // a valid method call; otherwise mozilla is not running

    if ((*aRetValue) && (*aRetValue)->IsValid() == S_OK)
         return TRUE;

    HRESULT hRes = ::CoInitialize(nsnull) ;

    hRes = ::CoCreateInstance(CLSID_CMapiImp, NULL, CLSCTX_LOCAL_SERVER,
                                         IID_nsIMapi, (LPVOID *)aRetValue);

    if (hRes == S_OK && (*aRetValue)->Initialize() == S_OK)
        if (TlsSetValue(tId, (LPVOID)(*aRetValue)))
            return TRUE;

    // Either CoCreate or TlsSetValue failed; so return FALSE

    if ((*aRetValue))
        (*aRetValue)->Release();

    ::CoUninitialize();
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////
// The MAPILogon function begins a Simple MAPI session, loading the default message ////
// store and address book providers                            ////
////////////////////////////////////////////////////////////////////////////////////////

ULONG FAR PASCAL MAPILogon(ULONG aUIParam, LPTSTR aProfileName,
                            LPTSTR aPassword, FLAGS aFlags,
                            ULONG aReserved, LPLHANDLE aSession)
{
    HRESULT hr = 0;
    ULONG nSessionId = 0;
    nsIMapi *pNsMapi = NULL;

    if (!InitMozillaReference(&pNsMapi))
        return MAPI_E_FAILURE;

    if (!(aFlags & MAPI_UNICODE))
    {
        // Need to convert the parameters to Unicode.

        char *pUserName = (char *) aProfileName;
        char *pPassWord = (char *) aPassword;

        TCHAR ProfileName[MAX_NAME_LEN] = {0};
        TCHAR PassWord[MAX_PW_LEN] = {0};

        if (pUserName != NULL)
        {
            if (!MultiByteToWideChar(CP_ACP, 0, pUserName, -1, ProfileName,
                                                            MAX_NAME_LEN))
                return MAPI_E_FAILURE;
        }

        if (pPassWord != NULL)
        {
            if (!MultiByteToWideChar(CP_ACP, 0, pPassWord, -1, PassWord,
                                                            MAX_NAME_LEN))
                return MAPI_E_FAILURE;
        }

        hr = pNsMapi->Login(aUIParam, ProfileName, PassWord, aFlags,
                                                        &nSessionId);
    }
    else
        hr = pNsMapi->Login(aUIParam, aProfileName, aPassword,
                                                aFlags, &nSessionId);
    if (hr == S_OK)
        (*aSession) = (LHANDLE) nSessionId;
    else
        return nSessionId;

    return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPILogoff (LHANDLE aSession, ULONG aUIParam,
                                            FLAGS aFlags, ULONG aReserved)
{
    nsIMapi *pNsMapi = (nsIMapi *)TlsGetValue(tId);
    if (pNsMapi != NULL)
    {
        if (pNsMapi->Logoff((ULONG) aSession) == S_OK)
            pNsMapi->Release();
        pNsMapi = NULL;
    }

    TlsSetValue(tId, NULL);

    ::CoUninitialize();

    return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPISendMail (LHANDLE lhSession, ULONG ulUIParam, nsMapiMessage *lpMessage,
                FLAGS flFlags, ULONG ulReserved )
{
    HRESULT hr = 0;
    BOOL bTempSession = FALSE ;
    nsIMapi *pNsMapi = NULL;

    if (!InitMozillaReference(&pNsMapi))
        return MAPI_E_FAILURE;

    if (lpMessage->nRecipCount > MAX_RECIPS)
        return MAPI_E_TOO_MANY_RECIPIENTS ;

    if (lpMessage->nFileCount > MAX_FILES)
        return MAPI_E_TOO_MANY_FILES ;

    if ( (!(flFlags & MAPI_DIALOG)) && (lpMessage->lpRecips == NULL) )
        return MAPI_E_UNKNOWN_RECIPIENT ;

    if (!lhSession || pNsMapi->IsValidSession(lhSession) != S_OK)
    {
        FLAGS LoginFlag ;
        if ( (flFlags & MAPI_LOGON_UI) && (flFlags & MAPI_NEW_SESSION) )
            LoginFlag = MAPI_LOGON_UI | MAPI_NEW_SESSION ;
        else if (flFlags & MAPI_LOGON_UI) 
            LoginFlag = MAPI_LOGON_UI ;

        hr = MAPILogon (ulUIParam, (LPTSTR) NULL, (LPTSTR) NULL, LoginFlag, 0, &lhSession) ;
        if (hr != SUCCESS_SUCCESS)
            return MAPI_E_LOGIN_FAILURE ;
        bTempSession = TRUE ;
    }

    // we need to deal with null data passed in by MAPI clients, specially when MAPI_DIALOG is set.
    // The MS COM type lib code generated by MIDL for the MS COM interfaces checks for these parameters
    // to be non null, although null is a valid value for them here. 
    nsMapiRecipDesc * lpRecips ;
    nsMapiFileDesc * lpFiles ;

    nsMapiMessage Message ;
    memset (&Message, 0, sizeof (nsMapiMessage) ) ;
    nsMapiRecipDesc Recipient ;
    memset (&Recipient, 0, sizeof (nsMapiRecipDesc) );
    nsMapiFileDesc Files ;
    memset (&Files, 0, sizeof (nsMapiFileDesc) ) ;

    if(!lpMessage)
    {
       lpMessage = &Message ;
    }
    if(!lpMessage->lpRecips)
    {
        lpRecips = &Recipient ;
    }
    else
        lpRecips = lpMessage->lpRecips ;
    if(!lpMessage->lpFiles)
    {
        lpFiles = &Files ;
    }
    else
        lpFiles = lpMessage->lpFiles ;

    hr = pNsMapi->SendMail (lhSession, lpMessage, 
                            (short) lpMessage->nRecipCount, lpRecips,
                            (short) lpMessage->nFileCount, lpFiles,
                            flFlags, ulReserved);

    // we are seeing a problem when using Word, although we return success from the MAPI support
    // MS COM interface in mozilla, we are getting this error here. This is a temporary hack !!
    if (hr == 0x800703e6)
        hr = SUCCESS_SUCCESS;
    
    if (bTempSession)
        MAPILogoff (lhSession, ulUIParam, 0,0) ;

    return hr ; 
}


ULONG FAR PASCAL MAPISendDocuments(ULONG ulUIParam, LPTSTR lpszDelimChar, LPTSTR lpszFilePaths,
                                LPTSTR lpszFileNames, ULONG ulReserved)
{
    LHANDLE lhSession ;
    nsIMapi *pNsMapi = NULL;

    if (!InitMozillaReference(&pNsMapi))
        return MAPI_E_FAILURE;

    unsigned long result = MAPILogon (ulUIParam, (LPTSTR) NULL, (LPTSTR) NULL, MAPI_LOGON_UI, 0, &lhSession) ;
    if (result != SUCCESS_SUCCESS)
        return MAPI_E_LOGIN_FAILURE ;

    HRESULT hr;

    hr = pNsMapi->SendDocuments(lhSession, (LPTSTR) lpszDelimChar, (LPTSTR) lpszFilePaths, 
                                    (LPTSTR) lpszFileNames, ulReserved) ;

    MAPILogoff (lhSession, ulUIParam, 0,0) ;

    return hr ;
}

ULONG FAR PASCAL MAPIFindNext(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszMessageType,
                              LPTSTR lpszSeedMessageID, FLAGS flFlags, ULONG ulReserved,
                              unsigned char lpszMessageID[64])
{
  nsIMapi *pNsMapi = NULL;

  if (!InitMozillaReference(&pNsMapi))
    return MAPI_E_FAILURE;

  if (lhSession == 0)
    return(MAPI_E_INVALID_SESSION);

  if (!lpszMessageType)
    lpszMessageType = L"";

  if (!lpszSeedMessageID)
    lpszSeedMessageID = L"";

  return pNsMapi->FindNext(lhSession, ulUIParam, lpszMessageType,
                              lpszSeedMessageID, flFlags, ulReserved,
                              lpszMessageID) ;


}


ULONG FAR PASCAL MAPIReadMail(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszMessageID,
                              FLAGS flFlags, ULONG ulReserved, nsMapiMessage **lppMessage)
{
  nsIMapi *pNsMapi = NULL;

  if (!InitMozillaReference(&pNsMapi))
    return MAPI_E_FAILURE;

  if (lhSession == 0)
    return(MAPI_E_INVALID_SESSION);

  return pNsMapi->ReadMail(lhSession, ulUIParam,
                              lpszMessageID, flFlags, ulReserved,
                              lppMessage) ;

}

ULONG FAR PASCAL MAPISaveMail(LHANDLE lhSession, ULONG ulUIParam, lpnsMapiMessage lpMessage,
                              FLAGS flFlags, ULONG ulReserved, LPTSTR lpszMessageID)
{
    nsIMapi *pNsMapi = NULL;

  if (lhSession == 0)
    return(MAPI_E_INVALID_SESSION);

  if (!InitMozillaReference(&pNsMapi))
      return MAPI_E_FAILURE;

  return MAPI_E_FAILURE;
}

ULONG FAR PASCAL MAPIDeleteMail(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszMessageID,
                                FLAGS flFlags, ULONG ulReserved)
{
  nsIMapi *pNsMapi = NULL;

  if (lhSession == 0)
    return(MAPI_E_INVALID_SESSION);

  if (!InitMozillaReference(&pNsMapi))
      return MAPI_E_FAILURE;

  return pNsMapi->DeleteMail(lhSession, ulUIParam,
                              lpszMessageID, flFlags, ulReserved) ;
}

ULONG FAR PASCAL MAPIAddress(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszCaption,
                             ULONG nEditFields, LPTSTR lpszLabels, ULONG nRecips,
                             lpMapiRecipDesc lpRecips, FLAGS flFlags,
                             ULONG ulReserved, LPULONG lpnNewRecips,
                             lpMapiRecipDesc FAR *lppNewRecips)
{
    return MAPI_E_FAILURE;
}

ULONG FAR PASCAL MAPIDetails(LHANDLE lhSession, ULONG ulUIParam, lpMapiRecipDesc lpRecip,
                             FLAGS flFlags, ULONG ulReserved)
{
    return MAPI_E_FAILURE;
}

ULONG FAR PASCAL MAPIResolveName(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszName,
                                 FLAGS flFlags, ULONG ulReserved, lpMapiRecipDesc FAR *lppRecip)
{
    return MAPI_E_FAILURE;
}

void FreeMAPIRecipient(lpMapiRecipDesc pv);
void FreeMAPIMessage(lpMapiMessage pv);

ULONG FAR PASCAL MAPIFreeBuffer(LPVOID pv)
{
  int   i;

  if (!pv)
  	return(S_OK);

  for (i=0; i<MAX_POINTERS; i++)
  {
    if (pv == memArray[i].lpMem)
    {
      if (memArray[i].memType == MAPI_MESSAGE_TYPE)
      {
        FreeMAPIMessage((MapiMessage *)pv);
        memArray[i].lpMem = NULL;
      }
      else if (memArray[i].memType == MAPI_RECIPIENT_TYPE)
      {
        FreeMAPIRecipient((MapiRecipDesc *)pv);
        memArray[i].lpMem = NULL;
      }
    }
  }

  pv = NULL;
  return(S_OK);
}

ULONG FAR PASCAL GetMapiDllVersion()
{
    return 94;
}

void
FreeMAPIFile(lpMapiFileDesc pv)
{
  if (!pv)
    return;

  if (pv->lpszPathName != NULL)   
    free(pv->lpszPathName);

  if (pv->lpszFileName != NULL)   
    free(pv->lpszFileName);
}


void
FreeMAPIMessage(lpMapiMessage pv)
{
  ULONG i;

  if (!pv)
    return;

  if (pv->lpszSubject != NULL)
    free(pv->lpszSubject);

  if (pv->lpszNoteText)
      free(pv->lpszNoteText);
  
  if (pv->lpszMessageType)
    free(pv->lpszMessageType);
  
  if (pv->lpszDateReceived)
    free(pv->lpszDateReceived);
  
  if (pv->lpszConversationID)
    free(pv->lpszConversationID);
  
  if (pv->lpOriginator)
    FreeMAPIRecipient(pv->lpOriginator);
  
  for (i=0; i<pv->nRecipCount; i++)
  {
    if (&(pv->lpRecips[i]) != NULL)
    {
      FreeMAPIRecipient(&(pv->lpRecips[i]));
    }
  }

  if (pv->lpRecips != NULL)
  {
    free(pv->lpRecips);
  }

  for (i=0; i<pv->nFileCount; i++)
  {
    if (&(pv->lpFiles[i]) != NULL)
    {
      FreeMAPIFile(&(pv->lpFiles[i]));
    }
  }

  if (pv->lpFiles != NULL)
  {
    free(pv->lpFiles);
  }
  
  free(pv);
  pv = NULL;
}

void
FreeMAPIRecipient(lpMapiRecipDesc pv)
{
  if (!pv)
    return;

  if (pv->lpszName != NULL)   
    free(pv->lpszName);

  if (pv->lpszAddress != NULL)
    free(pv->lpszAddress);

  if (pv->lpEntryID != NULL)
    free(pv->lpEntryID);  
}




