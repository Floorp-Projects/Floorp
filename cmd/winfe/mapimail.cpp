/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 */
//
//  More MAPI Hooks for Communicator
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include "stdafx.h"

#include "rosetta.h"
#include "template.h"
#include "msgcom.h"
#include "wfemsg.h"
#include "compstd.h"
#include "compbar.h"
#include "compmisc.h"
#include "compfrm.h"
#include "prefapi.h"
#include "intl_csi.h"
#include "dlghtmrp.h"
#include "dlghtmmq.h"

// rhp - was breaking the optimized build!
//#include "edt.h"
//#include "edview.h"
//#include "postal.h"
//#include "apiaddr.h"
//#include "mailmisc.h"

extern "C" {
#include "xpgetstr.h"
extern int MK_MSG_MSG_COMPOSITION;
};

#include "mapimail.h"
#include "nscpmapi.h"
#include "mailpriv.h"
#include "nsstrseq.h"

MWContext             
*GetUsableContext(void)
{
  CGenericFrame     *pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();

  ASSERT(pFrame != NULL);
  if (pFrame == NULL)
  {
    return(NULL);
  }

  // Now return the context...
  return((MWContext *) pFrame->GetMainContext());
}

//
// This function will create a composition window and either do
// a blind send or pop up the compose window for the user to 
// complete the operation
//
// Return: appropriate MAPI return code...
//
// 
extern "C" LONG
DoFullMAPIMailOperation(MAPISendMailType      *sendMailPtr,
											  const char            *pInitialText,
                        BOOL                  winShowFlag)
{   
CGenericDoc             *pDocument;
LPSTR                   subject;
NSstringSeq             mailInfoSeq;
DWORD                   stringCount = 6;
DWORD                   i;
CString                 csDefault;

  // Get a context to use for this call...
  MWContext *pOldContext = GetUsableContext();
  if (!pOldContext)
  {
    return(MAPI_E_FAILURE);
  }

  // Don't allow a compose window to be created if the user hasn't 
  // specified an email address
  const char *real_addr = FE_UsersMailAddress();
  if (MISC_ValidateReturnAddress(pOldContext, real_addr) < 0)
  {
    return(MAPI_E_FAILURE);
  }

  // Check on HTML compose...only do this if the window is shown!
  XP_Bool           htmlCompose;
  PREF_GetBoolPref("mail.html_compose", &htmlCompose);
  if (!winShowFlag)
    htmlCompose = FALSE;

  //
  // Now, we must build the fields object...
  //
  mailInfoSeq = (NSstringSeq) &(sendMailPtr->dataBuf[0]);
  subject = NSStrSeqGet(mailInfoSeq, 0);

  // We should give it a subject to preven the prompt from coming
  // up...
  if ((!subject) || !(*subject))
  {
    if (sendMailPtr->MSG_nFileCount > 0)
    {
      subject = NSStrSeqGet(mailInfoSeq, 6 + (sendMailPtr->MSG_nRecipCount * 3) + 1);
    }
    else
    {
      csDefault.LoadString(IDS_COMPOSE_DEFAULTNOSUBJECT);
      subject = csDefault.GetBuffer(2);
    }
  }

  TRACE("MAPI: ProcessMAPISendMail() Subject   = [%s]\n", subject);
  TRACE("MAPI: ProcessMAPISendMail() Text Size = [%d]\n", strlen((const char *)pInitialText));
  TRACE("MAPI: ProcessMAPISendMail() # of Recipients  = [%d]\n", sendMailPtr->MSG_nRecipCount);


  char  toString[1024] = "";
  char  ccString[1024] = "";
  char  bccString[1024] = "";

  for (i=0; i<sendMailPtr->MSG_nRecipCount; i++)
  {
    LPSTR   ptr;
    UCHAR   tempString[256];

    ULONG addrType = atoi(NSStrSeqGet(mailInfoSeq, stringCount++));

    // figure which type of address this is?
    if (addrType == MAPI_CC)
      ptr = ccString;
    else if (addrType == MAPI_BCC)
      ptr = bccString;
    else
      ptr = toString;
      
    LPSTR namePtr = (LPSTR) NSStrSeqGet(mailInfoSeq, stringCount++);
    LPSTR emailPtr = (LPSTR) NSStrSeqGet(mailInfoSeq, stringCount++);
    if ( (lstrlen(emailPtr) > 5) && (*(emailPtr + 4) == ':') )
    {
      emailPtr += 5;
    }

    // Now build the temp string to tack on in the format
    // "Rich Pizzarro" <rhp@netscape.com>
    wsprintf((LPSTR) tempString, "\"%s\" <%s>", namePtr, emailPtr);

    // add a comma if not the first one
    if (ptr[0] != '\0')
      lstrcat(ptr, ",");

    // tack on string!
    lstrcat(ptr, (LPSTR) tempString);
  }

  HG27129
  MSG_CompositionFields *fields =
      MSG_CreateCompositionFields(real_addr, real_addr, 
                  toString, 
                  ccString, 
                  bccString,
									"", "", "",
									"", subject, "",
									"", "", "",
									"", 
                  HG21198);
  if (!fields)
  {
    return(MAPI_E_FAILURE);
  }

  // RICHIE
  // INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pOldContext);
  // int16 win_csid = INTL_GetCSIWinCSID(csi);
  if (htmlCompose)
  {
    pDocument = (CGenericDoc*)theApp.m_ComposeTemplate->OpenDocumentFile(NULL,
                                    NULL, winShowFlag);
  }
  else
  {
    pDocument = (CGenericDoc*)theApp.m_TextComposeTemplate->OpenDocumentFile(NULL, 
                                    NULL, winShowFlag);
  }
  if ( !pDocument )
  {
    return(MAPI_E_FAILURE);
  }
  
  CWinCX * pContext = (CWinCX*) pDocument->GetContext();
  if ( !pContext ) 
  {
    return(MAPI_E_FAILURE);
  }

  MSG_CompositionPaneCallbacks Callbacks;
  Callbacks.CreateRecipientsDialog = CreateRecipientsDialog;
  Callbacks.CreateAskHTMLDialog = CreateAskHTMLDialog;

  int16 doccsid;
  MWContext *context = pContext->GetContext();
  CComposeFrame *pCompose = (CComposeFrame *) pContext->GetFrame()->GetFrameWnd();

  pCompose->SetComposeStuff(context, fields); // squirl away stuff for post-create

  // This needs to be set TRUE if using the old non-HTML text frame
  // to prevent dropping dragged URLs
  pContext->m_bDragging = !pCompose->UseHtml();
  if (!pCompose->UseHtml()) 
  {
    pCompose->SetMsgPane(
      MSG_CreateCompositionPane(pContext->GetContext(), 
                                context, 
                                g_MsgPrefs.m_pMsgPrefs, 
                                fields,
                                WFE_MSGGetMaster())
                        );
  }
  else
  {
    pCompose->SetMsgPane(MSG_CreateCompositionPaneNoInit(
                              context, // pContext->GetContext(), 
                              g_MsgPrefs.m_pMsgPrefs,
                              WFE_MSGGetMaster()));

    // Set the callbacks for askHTML and Recipients dialogs
    MSG_SetCompositionPaneCallbacks( pCompose->GetMsgPane(), &Callbacks, 0);
    if (pCompose->GetMsgPane())
    {
      MSG_SetHTMLMarkup(pCompose->GetMsgPane(), TRUE);
    }
  }

  ASSERT(pCompose->GetMsgPane());
  MSG_SetFEData(pCompose->GetMsgPane(),(void *)pCompose);  
  pCompose->UpdateAttachmentInfo();

  // Pass doccsid info to new context for MailToWin conversion
  doccsid = INTL_GetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context));
  INTL_SetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context), 
    (doccsid ? doccsid : INTL_DefaultDocCharSetID(context)));

  pCompose->DisplayHeaders(NULL);

  CComposeBar * pBar = pCompose->GetComposeBar();
  ASSERT(pBar);
  LPADDRESSCONTROL pIAddressList = pBar->GetAddressWidgetInterface();

  if (!pIAddressList->IsCreated()) 
  {
    pBar->CreateAddressingBlock();
  }

  // rhp - Deal with addressing the brute force way! This is a 
  // "fix" for bad behavior when creating these windows and not
  // showing them on the desktop.

  if (!winShowFlag)      // Hack to fix the window not being mapped
  {
    pCompose->AppendAddress(MSG_TO_HEADER_MASK, "");
    pCompose->AppendAddress(MSG_CC_HEADER_MASK, "");
    pCompose->AppendAddress(MSG_BCC_HEADER_MASK, "");
  }

  if (!pCompose->UseHtml())
  {
    // Do this so we don't get popups on "empty" messages
    if ( (!pInitialText) || (!(*pInitialText)) )
      pInitialText = " ";

    // IF we are doing plain text composition!
    pCompose->CompleteComposeInitialization();

    const char * pBody = pInitialText ? pInitialText : MSG_GetCompBody(pCompose->GetMsgPane());
    if (pBody)
    {
      FE_InsertMessageCompositionText(context,pBody,TRUE);
    }
  }
  else
  {
    // Gotta be more than just spaces! - rhp - fixes some bugs with Office
    if ( (pInitialText) && (*pInitialText) )
    {
      char *tmp = (char *)pInitialText; 

      while (*tmp == ' ')
        tmp++; 

      if (*tmp)
        pCompose->SetInitialText(pInitialText); 
    }
    // Gotta be more than just spaces! - rhp - fixes some bugs with Office

    URL_Struct * pUrl = NET_CreateURLStruct(EDT_NEW_DOC_URL,NET_DONT_RELOAD);
    if (pUrl != NULL)
    {
      pContext->GetUrl(pUrl, FO_CACHE_AND_PRESENT);
      pContext->GetContext()->bIsComposeWindow = TRUE;
    }
  }

  // 
  // Now set the message as being edited!    
  //
  pCompose->SetModified(TRUE);

  //
  // Finally deal with the attachments...
  //
  if (sendMailPtr->MSG_nFileCount > 0)
  {
    // Send this puppy when done with the attachments...
    if (!winShowFlag)
    {
      pCompose->SetMAPISendMode(MAPI_SEND);
    }

    MSG_AttachmentData *pAttach = (MSG_AttachmentData *)
                  XP_CALLOC((sendMailPtr->MSG_nFileCount + 1),
                  sizeof(MSG_AttachmentData));
    if (!pAttach)
    {
      return(MAPI_E_INSUFFICIENT_MEMORY);
    }

    memset(pAttach, 0, (sendMailPtr->MSG_nFileCount + 1) * 
                                sizeof(MSG_AttachmentData));
    for (i=0; i<sendMailPtr->MSG_nFileCount; i++)
    {
      CString cs;
      // Create URL from filename...
      WFE_ConvertFile2Url(cs, 
          (const char *)NSStrSeqGet(mailInfoSeq, stringCount++));
      pAttach[i].url = XP_STRDUP(cs);

      // Now also include the "display" name...
      StrAllocCopy(pAttach[i].real_name, NSStrSeqGet(mailInfoSeq, stringCount++));
    }

    // Set the list!
    MSG_SetAttachmentList(pCompose->GetMsgPane(), pAttach);

    // Now free everything...
    for (i=0; i<sendMailPtr->MSG_nFileCount; i++)
    {
      if (pAttach[i].url)
        XP_FREE(pAttach[i].url);

      if (pAttach[i].real_name)
        XP_FREE(pAttach[i].real_name);
    }

    XP_FREE(pAttach);
  }

  //
  // Now, if we were supposed to do the blind send...do it, otherwise,
  // just popup the window...
  //
  if (winShowFlag)      
  {
    // Post message to compose window to set the initial focus.
    pCompose->PostMessage(WM_COMP_SET_INITIAL_FOCUS);
  }
  else if (sendMailPtr->MSG_nFileCount <= 0)  // Send NOW if no attachments!
  {
    pCompose->PostMessage(WM_COMMAND, IDM_SEND);
  }

  return(SUCCESS_SUCCESS);
}

//
// This function will create a composition window and just attach
// the attachments of interest and pop up the window...
//
// Return: appropriate MAPI return code...
//
//
extern "C" LONG
DoPartialMAPIMailOperation(MAPISendDocumentsType *sendDocPtr)
{   
CGenericDoc             *pDocument;

  // Get a context to use for this call...
  MWContext *pOldContext = GetUsableContext();
  if (!pOldContext)
  {
    return(MAPI_E_FAILURE);
  }

  // Don't allow a compose window to be created if the user hasn't 
  // specified an email address
  const char *real_addr = FE_UsersMailAddress();
  if (MISC_ValidateReturnAddress(pOldContext, real_addr) < 0)
  {
    return(MAPI_E_FAILURE);
  }

  //
  // Now, build the fields object w/o much info...
  //
  HG26726
  MSG_CompositionFields *fields =
      MSG_CreateCompositionFields(real_addr, real_addr, NULL, 
                  "", "",
									"", "", "",
									"", "", "",
									"", "", "",
									"", 
                  HG21892);
  if (!fields)
  {
    return(MAPI_E_FAILURE);
  }

  // Check on HTML compose...
  XP_Bool           htmlCompose;
  PREF_GetBoolPref("mail.html_compose", &htmlCompose);

  // RICHIE - INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pOldContext);
  // int16 win_csid = INTL_GetCSIWinCSID(csi);
  if (htmlCompose)
  {
    pDocument = (CGenericDoc*)theApp.m_ComposeTemplate->OpenDocumentFile(NULL,
                                    NULL, TRUE);
  }
  else
  {
    pDocument = (CGenericDoc*)theApp.m_TextComposeTemplate->OpenDocumentFile(NULL, 
                                    NULL, TRUE);
  }
  if ( !pDocument )
  {
    // cleanup fields object
    MSG_DestroyCompositionFields(fields);
    return(MAPI_E_FAILURE);
  }

  CWinCX * pContext = (CWinCX*) pDocument->GetContext();
  if ( !pContext ) 
  {
    return(MAPI_E_FAILURE);
  }

  MSG_CompositionPaneCallbacks Callbacks;
  Callbacks.CreateRecipientsDialog = CreateRecipientsDialog;
  Callbacks.CreateAskHTMLDialog = CreateAskHTMLDialog;

  MWContext *context = pContext->GetContext();
  CComposeFrame *pCompose = (CComposeFrame *) pContext->GetFrame()->GetFrameWnd();
  pCompose->SetComposeStuff(context,fields); // squirl away stuff for post-create

  // This needs to be set TRUE if using the old non-HTML text frame
  // to prevent dropping dragged URLs
  pContext->m_bDragging = !pCompose->UseHtml();
  if (!pCompose->UseHtml()) 
  {
    pCompose->SetMsgPane(MSG_CreateCompositionPane(
      pContext->GetContext(), 
      context,
      g_MsgPrefs.m_pMsgPrefs, fields,
      WFE_MSGGetMaster()));
  }
  else
  {
    pCompose->SetMsgPane(MSG_CreateCompositionPaneNoInit(
                              context, // pContext->GetContext(), 
                              g_MsgPrefs.m_pMsgPrefs,
                              WFE_MSGGetMaster()));

    // Set the callbacks for askHTML and Recipients dialogs
    MSG_SetCompositionPaneCallbacks( pCompose->GetMsgPane(), &Callbacks, 0);
    if (pCompose->GetMsgPane())
    {
      MSG_SetHTMLMarkup(pCompose->GetMsgPane(), TRUE);
    }
  }

  ASSERT(pCompose->GetMsgPane());
  MSG_SetFEData(pCompose->GetMsgPane(),(void *)pCompose);

  pCompose->UpdateAttachmentInfo();

  // Pass doccsid info to new context for MailToWin conversion
  /***
  doccsid = INTL_GetCSIDocCSID(LO_GetDocumentCharacterSetInfo(pOldContext));

  INTL_SetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context), 
    (doccsid ? doccsid : INTL_DefaultDocCharSetID(pOldContext)));
  ****/

  pCompose->DisplayHeaders(NULL);

  CComposeBar * pBar = pCompose->GetComposeBar();
  ASSERT(pBar);
  LPADDRESSCONTROL pIAddressList = pBar->GetAddressWidgetInterface();

  if (!pIAddressList->IsCreated()) 
  {
    pBar->CreateAddressingBlock();
  }

  if (!pCompose->UseHtml())
  {
    // If doing plain text composition!
    pCompose->CompleteComposeInitialization();
  }
  else
  {
    URL_Struct * pUrl = NET_CreateURLStruct(EDT_NEW_DOC_URL,NET_DONT_RELOAD);
    if (pUrl != NULL)
    {
      pContext->GetUrl(pUrl, FO_CACHE_AND_PRESENT);
      pContext->GetContext()->bIsComposeWindow = TRUE;
    }
  }

  //
  // Finally deal with the attachments...
  //
  NSstringSeq     mailInfoSeq = (NSstringSeq) &(sendDocPtr->dataBuf[0]);
  DWORD           stringCount = 0;
  DWORD           i;

  TRACE("MAPI: ProcessMAPISendDocuments() # of Attachments = [%d]\n", sendDocPtr->nFileCount);

  if (sendDocPtr->nFileCount > 0)
  {
      MSG_AttachmentData *pAttach = (MSG_AttachmentData *)
                    XP_CALLOC((sendDocPtr->nFileCount + 1),
                    sizeof(MSG_AttachmentData));
      if (!pAttach)
      {
        return(MAPI_E_INSUFFICIENT_MEMORY);
      }

      memset(pAttach, 0, (sendDocPtr->nFileCount + 1) * 
                                  sizeof(MSG_AttachmentData));
      for (i=0; i<sendDocPtr->nFileCount; i++)
      {
        CString cs;
        // Create URL from filename...
        WFE_ConvertFile2Url(cs, 
            (const char *)NSStrSeqGet(mailInfoSeq, stringCount++));
        pAttach[i].url = XP_STRDUP(cs);

        // Now also include the "display" name...
        StrAllocCopy(pAttach[i].real_name, NSStrSeqGet(mailInfoSeq, stringCount++));
      }

      // Set the list!
      MSG_SetAttachmentList(pCompose->GetMsgPane(), pAttach);

      // Now free everything...
      for (i=0; i<sendDocPtr->nFileCount; i++)
      {
        if (pAttach[i].url)
          XP_FREE(pAttach[i].url);

        if (pAttach[i].real_name)
          XP_FREE(pAttach[i].real_name);
      }

      XP_FREE(pAttach);
  }
    
  // 
  // Now some checking for ... well I'm not sure...
  //
  if (MSG_GetAttachmentList(pCompose->GetMsgPane()))
    pCompose->SetModified(TRUE);
  else
    pCompose->SetModified(FALSE);

  // Post message to compose window to set the initial focus.
  pCompose->PostMessage(WM_COMP_SET_INITIAL_FOCUS);

  //
  // Now, just popup the window...
  //
  pCompose->ShowWindow(TRUE);

  // return pCompose->GetMsgPane(); rhp - used to return the MsgPane
  return(SUCCESS_SUCCESS);
}

static void _GetMailCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
   if (pane != NULL)
   {
     ShowWindow(hwnd, SW_HIDE);
     MSG_Command( pane, MSG_GetNewMail, NULL, 0 );
   }
}

static void _GetMailDoneCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
	for(CGenericFrame * f = theApp.m_pFrameList; f; f = f->m_pNext)
		f->PostMessage(WM_COMMAND, (WPARAM) ID_DONEGETTINGMAIL, (LPARAM) 0);
}

//
// This will fire off a "get mail in background operation" in an
// async. fashion.
//
extern "C" void
MAPIGetNewMessagesInBackground(void)
{
CGenericFrame     *pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();

  // rhp - we should not hit the net if we are offline!
  if (NET_IsOffline())
    return;

  if (!pFrame)
    return;

  MWContext *pOldContext = GetUsableContext();
  if (!pOldContext)
    return;

  TRACE("MAPI: DOWNLOAD MAIL IN BACKGROUND\n");
  new CProgressDialog(
                pFrame->GetFrameWnd(), 
                NULL, 
                _GetMailCallback, NULL, NULL, 
                _GetMailDoneCallback);
}

//
// This function will save a message into the Communicator "Drafts"
// folder with no UI showing.
//
// Return: appropriate MAPI return code...
//
//
extern "C" LONG 
DoMAPISaveMailOperation(MAPISendMailType      *sendMailPtr,
											  const char            *pInitialText)
{
CGenericDoc             *pDocument;
LPSTR                   subject;
NSstringSeq             mailInfoSeq;
DWORD                   stringCount = 6;
DWORD                   i;
BOOL                    winShowFlag = FALSE;

  // Get a context to use for this call...
  MWContext *pOldContext = GetUsableContext();
  if (!pOldContext)
  {
    return(MAPI_E_FAILURE);
  }

  // Check on HTML compose...
  XP_Bool           htmlCompose;
  PREF_GetBoolPref("mail.html_compose", &htmlCompose);
  if (!winShowFlag)
    htmlCompose = FALSE;

  // Don't allow a compose window to be created if the user hasn't 
  // specified an email address
  const char *real_addr = FE_UsersMailAddress();
  if (MISC_ValidateReturnAddress(pOldContext, real_addr) < 0)
  {
    return(MAPI_E_FAILURE);
  }

  //
  // Now, we must build the fields object...
  //
  mailInfoSeq = (NSstringSeq) &(sendMailPtr->dataBuf[0]);
  subject = NSStrSeqGet(mailInfoSeq, 0);

  TRACE("MAPI: ProcessMAPISendMail() Subject   = [%s]\n", subject);
  TRACE("MAPI: ProcessMAPISendMail() Text Size = [%d]\n", strlen((const char *)pInitialText));
  TRACE("MAPI: ProcessMAPISendMail() # of Recipients  = [%d]\n", sendMailPtr->MSG_nRecipCount);


  char  toString[1024] = "";
  char  ccString[1024] = "";
  char  bccString[1024] = "";

  for (i=0; i<sendMailPtr->MSG_nRecipCount; i++)
  {
    LPSTR   ptr;
    UCHAR   tempString[256];

    ULONG addrType = atoi(NSStrSeqGet(mailInfoSeq, stringCount++));

    // figure which type of address this is?
    if (addrType == MAPI_CC)
      ptr = ccString;
    else if (addrType == MAPI_BCC)
      ptr = bccString;
    else
      ptr = toString;

    LPSTR namePtr = (LPSTR) NSStrSeqGet(mailInfoSeq, stringCount++);
    LPSTR emailPtr = (LPSTR) NSStrSeqGet(mailInfoSeq, stringCount++);

    if ( (!emailPtr) && (!namePtr))
    {
      return(MAPI_E_INVALID_RECIPS);
    }

    if (!emailPtr)
      emailPtr = namePtr;

    char *tptr = strchr(emailPtr, ':');
    if (tptr != NULL)
    {
      if ( (*tptr != '\0') && (*(tptr+1) != '\0') )
      {
        emailPtr = (tptr + 1);
      }
    }
/**
    if ( (lstrlen(emailPtr) > 5) && (*(emailPtr + 4) == ':') )
    {
      emailPtr += 5;
    }
**/
    // Now build the temp string to tack on in the format
    // "Rich Pizzarro" <rhp@netscape.com>
    wsprintf((LPSTR) tempString, "\"%s\" <%s>", namePtr, emailPtr);

    // add a comma if not the first one
    if (ptr[0] != '\0')
      lstrcat(ptr, ",");

    // tack on string!
    lstrcat(ptr, (LPSTR) tempString);
  }

  BOOL    bxxx = FALSE;
  BOOL    bxxx2    = FALSE;

  
  MSG_CompositionFields *fields =
      MSG_CreateCompositionFields(real_addr, real_addr, 
                  toString, 
                  ccString, 
                  bccString,
									"", "", "",
									"", subject, "",
									"", "", "",
									"", 
                  bxxx,
                  bxxx2);
  if (!fields)
  {
    return(MAPI_E_FAILURE);
  }

  // RICHIE
  // INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pOldContext);
  // int16 win_csid = INTL_GetCSIWinCSID(csi);
  
  if (htmlCompose)
  {
    pDocument = (CGenericDoc*)theApp.m_ComposeTemplate->OpenDocumentFile(NULL,
                                    NULL, winShowFlag);
  }
  else
  {
    pDocument = (CGenericDoc*)theApp.m_TextComposeTemplate->OpenDocumentFile(NULL, 
                                    NULL, winShowFlag);
  }
  if ( !pDocument )
  {
    return(MAPI_E_FAILURE);
  }
  
  CWinCX * pContext = (CWinCX*) pDocument->GetContext();
  if ( !pContext ) 
  {
    return(MAPI_E_FAILURE);
  }

  MSG_CompositionPaneCallbacks Callbacks;
  Callbacks.CreateRecipientsDialog = CreateRecipientsDialog;
  Callbacks.CreateAskHTMLDialog = CreateAskHTMLDialog;

  int16 doccsid;
  MWContext *context = pContext->GetContext();
  CComposeFrame *pCompose = (CComposeFrame *) pContext->GetFrame()->GetFrameWnd();

  pCompose->SetComposeStuff(context, fields); // squirl away stuff for post-create

  // This needs to be set TRUE if using the old non-HTML text frame
  // to prevent dropping dragged URLs
  pContext->m_bDragging = !pCompose->UseHtml();
  if (!pCompose->UseHtml()) 
  {
    pCompose->SetMsgPane(
      MSG_CreateCompositionPane(pContext->GetContext(), 
                                context, 
                                g_MsgPrefs.m_pMsgPrefs, 
                                fields,
                                WFE_MSGGetMaster())
                        );
  }
  else
  {
    pCompose->SetMsgPane(MSG_CreateCompositionPaneNoInit(
                              context, // pContext->GetContext(), 
                              g_MsgPrefs.m_pMsgPrefs,
                              WFE_MSGGetMaster()));

    // Set the callbacks for askHTML and Recipients dialogs
    MSG_SetCompositionPaneCallbacks( pCompose->GetMsgPane(), &Callbacks, 0);
    if (pCompose->GetMsgPane())
    {
      MSG_SetHTMLMarkup(pCompose->GetMsgPane(), TRUE);
    }
  }

  ASSERT(pCompose->GetMsgPane());
  MSG_SetFEData(pCompose->GetMsgPane(),(void *)pCompose);  
  pCompose->UpdateAttachmentInfo();

  // Pass doccsid info to new context for MailToWin conversion
  doccsid = INTL_GetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context));
  INTL_SetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context), 
    (doccsid ? doccsid : INTL_DefaultDocCharSetID(context)));

  pCompose->DisplayHeaders(NULL);

  CComposeBar * pBar = pCompose->GetComposeBar();
  ASSERT(pBar);
  LPADDRESSCONTROL pIAddressList = pBar->GetAddressWidgetInterface();

  if (!pIAddressList->IsCreated()) 
  {
    pBar->CreateAddressingBlock();
  }

  // rhp - Deal with addressing the brute force way! This is a 
  // "fix" for bad behavior when creating these windows and not
  // showing them on the desktop.
  if (!winShowFlag)      // Hack to fix the window not being mapped
  {
    pCompose->AppendAddress(MSG_TO_HEADER_MASK, "");
    pCompose->AppendAddress(MSG_CC_HEADER_MASK, "");
    pCompose->AppendAddress(MSG_BCC_HEADER_MASK, "");
  }

  if (!pCompose->UseHtml())
  {
    // Do this so we don't get popups on "empty" messages
    if ( (!pInitialText) || (!(*pInitialText)) )
      pInitialText = " ";

    // If doing plain text composition!
    pCompose->CompleteComposeInitialization();

    const char * pBody = pInitialText ? pInitialText : MSG_GetCompBody(pCompose->GetMsgPane());
    if (pBody)
    {
      FE_InsertMessageCompositionText(context,pBody,TRUE);
    }
  }
  else
  {
    // Gotta be more than just spaces! - rhp - fixes some bugs with Office
    if ( (pInitialText) && (*pInitialText) )
    {
      char *tmp = (char *)pInitialText; 

      while (*tmp == ' ')
        tmp++; 
      if (*tmp)
        pCompose->SetInitialText(pInitialText); 
    }
    // Gotta be more than just spaces! - rhp - fixes some bugs with Office

    URL_Struct * pUrl = NET_CreateURLStruct(EDT_NEW_DOC_URL,NET_DONT_RELOAD);
    if (pUrl != NULL)
    {
      pContext->GetUrl(pUrl, FO_CACHE_AND_PRESENT);
      pContext->GetContext()->bIsComposeWindow = TRUE;
    }
  }

  // 
  // Now set the message as being edited!    
  //
  pCompose->SetModified(TRUE);

  //
  // Finally deal with the attachments...
  //
  if (sendMailPtr->MSG_nFileCount > 0)
  {
    // Send this puppy when done with the attachments...
    if (!winShowFlag)
    {
      pCompose->SetMAPISendMode(MAPI_SAVE);
    }

    MSG_AttachmentData *pAttach = (MSG_AttachmentData *)
                  XP_CALLOC((sendMailPtr->MSG_nFileCount + 1),
                  sizeof(MSG_AttachmentData));
    if (!pAttach)
    {
      return(MAPI_E_INSUFFICIENT_MEMORY);
    }

    memset(pAttach, 0, (sendMailPtr->MSG_nFileCount + 1) * 
                                sizeof(MSG_AttachmentData));
    for (i=0; i<sendMailPtr->MSG_nFileCount; i++)
    {
      CString cs;
      // Create URL from filename...
      WFE_ConvertFile2Url(cs, 
          (const char *)NSStrSeqGet(mailInfoSeq, stringCount++));
      pAttach[i].url = XP_STRDUP(cs);

      // Now also include the "display" name...
      StrAllocCopy(pAttach[i].real_name, NSStrSeqGet(mailInfoSeq, stringCount++));
    }

    // Set the list!
    MSG_SetAttachmentList(pCompose->GetMsgPane(), pAttach);

    // Now free everything...
    for (i=0; i<sendMailPtr->MSG_nFileCount; i++)
    {
      if (pAttach[i].url)
        XP_FREE(pAttach[i].url);

      if (pAttach[i].real_name)
        XP_FREE(pAttach[i].real_name);
    }

    XP_FREE(pAttach);
  }

  //
  // Now, if we were supposed to do the blind send...do it, otherwise,
  // just popup the window...
  //
  if (winShowFlag)      
  {
    // Post message to compose window to set the initial focus.
    pCompose->PostMessage(WM_COMP_SET_INITIAL_FOCUS);
  }
  else if (sendMailPtr->MSG_nFileCount <= 0)  // Send NOW if no attachments!
  {
    pCompose->PostMessage(WM_COMMAND, IDM_SAVEASDRAFT);
  }

  return(SUCCESS_SUCCESS);
}
