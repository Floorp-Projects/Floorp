/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "stdafx.h"
#include "template.h"
#include "msgcom.h"
#include "wfemsg.h"
#include "compstd.h"
#include "compbar.h"
#include "compmisc.h"
#include "compfrm.h"
#include "edt.h"
#include "edview.h"
#include "intl_csi.h"
#include "prefapi.h"
#include "apiaddr.h"
#include "postal.h"
#include "mailmisc.h"
#include "dlghtmrp.h"
#include "dlghtmmq.h"

extern "C" {
#include "xpgetstr.h"
extern int MK_MSG_MSG_COMPOSITION;
};


extern "C" int FE_LoadExchangeInfo(MWContext * context);
extern "C" MSG_Pane * DoAltMailComposition(MWContext *pOldContext, MSG_CompositionFields* fields); 

int WFE_InsertMessage(void *closure, const char * data);

extern "C" void FE_DestroyMailCompositionContext(MWContext *pContext)	
{
   CGenericFrame * pFrame = wfe_FrameFromXPContext(pContext);
   if ( pFrame ) {
	   CComposeFrame * pCompose = (CComposeFrame *) pFrame;
	   if (pCompose->UseHtml())
		   EDT_SetDirtyFlag(pContext,FALSE);
	   else
		   pCompose->GetEditorParent()->m_bModified = FALSE;
	   pCompose->SetMsgPane(NULL);
	   if(pCompose) {
			pCompose->PostMessage ( WM_CLOSE );
	   }
   }
}

extern "C" int FE_GetMessageBody(MSG_Pane *pPane, char **ppBody, uint32 *ulBodySize, MSG_FontCode **ppFontChanges)	
{
	ASSERT(pPane);
   CComposeFrame * pCompose = (CComposeFrame *) MSG_GetFEData(pPane);
	ASSERT(pCompose);
	MWContext *pContext = pCompose->GetMainContext()->GetContext();
	ASSERT(pContext);

    if (pCompose->UseHtml())
    {
        MSG_SetHTMLMarkup(pPane,TRUE);
        *ppBody = NULL;
        *ulBodySize = 0;

        if ( pCompose )
        {                                
            // Danger!!! This is not legal!!! We must change this to be a real XP_HUGE for win16.
            // see bug #39756
		    EDT_SaveToBuffer(pContext,(XP_HUGE_CHAR_PTR*) ppBody);

		    if ( !*ppBody)
			    return 1;
		    else {
			    size_t lsize;
			    lsize = strlen(*ppBody);
	            *ulBodySize = lsize;
			    return lsize;
		    }
        }
        return 0;
    }

		// new compose window code          
		size_t lsize = 0; 
#ifdef XP_WIN16		
		unsigned int   textLength;	// Cannot use uint32/int32; the compiler won't
									      // generate the correct code. Use unsigned int, at least
									      // we could get over with the 32767 boundary. 
		
		// Although 16-bit CEdit::GetWindowTextLength returns int casting it to
		// unsigned int we can still get the correct text length if the text length is
		// greater than 32767		
		textLength = (unsigned int) pCompose->GetEditor()->GetWindowTextLength();  
#else
		int32	textLength;
		textLength = pCompose->GetEditor()->GetWindowTextLength();
#endif				
		if ( textLength >= 0 )
		{	
			char * tmp = (char *)XP_CALLOC(1,textLength+1);
     		ASSERT ( tmp );
         pCompose->GetEditor()->GetWindowText(tmp, textLength+1);
			if (pCompose->GetWrapLongLines())
         {
			int32 lineWidth = 72;

			PREF_GetIntPref("mailnews.wraplength", &lineWidth);
			if (lineWidth < 10) lineWidth = 10;
			if (lineWidth > 30000) lineWidth = 30000;

		    *ppBody = (char *) XP_WordWrap(INTL_DefaultWinCharSetID(pContext), 
		         (unsigned char *)tmp, lineWidth, 1 /* look for '>' */);
            free(tmp);                                         
		   }
		   else
		   {
		        /* Else, don't wrap it at all. */
		        *ppBody = tmp;
		   }
         if (ppBody && *ppBody)
            for (const char *ptr = *ppBody; ptr && *ptr; ptr++)
                lsize++;
		}
   *ulBodySize = lsize;
   return 0;
}

//
// Create a composition window
//
extern "C" void FE_InitializeMailCompositionContext(MSG_Pane *pComposePane,
	const char *pFrom,
	const char *pReplyTo,
	const char *pTo,
	const char *pCc,
	const char *pBcc,
	const char *pFcc,
	const char *pNewsgroups,
	const char *pFollowupTo,
	const char *pSubject,
	const char *pAttachment)
{

    MSG_SetCompHeader(pComposePane, MSG_TO_HEADER_MASK, pTo);
    MSG_SetCompHeader(pComposePane, MSG_FROM_HEADER_MASK, pFrom);
    MSG_SetCompHeader(pComposePane, MSG_SUBJECT_HEADER_MASK, pSubject);
    MSG_SetCompHeader(pComposePane, MSG_CC_HEADER_MASK, pCc);
    MSG_SetCompHeader(pComposePane, MSG_BCC_HEADER_MASK, pBcc);
    MSG_SetCompHeader(pComposePane, MSG_FCC_HEADER_MASK, pFcc);
    MSG_SetCompHeader(pComposePane, MSG_REPLY_TO_HEADER_MASK, pReplyTo);
    MSG_SetCompHeader(pComposePane, MSG_FOLLOWUP_TO_HEADER_MASK, pFollowupTo);
    MSG_SetCompHeader(pComposePane, MSG_NEWSGROUPS_HEADER_MASK, pNewsgroups);
}

MWContext * g_NastyContextSavingHack = NULL;

// create a new xpContext with type == MWContextMessageComposition
extern "C" MSG_Pane *FE_CreateCompositionPane(MWContext *pOldContext,
											  MSG_CompositionFields* fields,
											  const char *pInitialText,
											  MSG_EditorType editorType)
{   
    if(!theApp.m_hPostalLib) { 
    CGenericDoc * pDocument;

	// Don't allow a compose window to be created if the user hasn't 
	// specified an email address
	const char *real_addr = FE_UsersMailAddress();
	if (MISC_ValidateReturnAddress(pOldContext, real_addr) < 0)
		return NULL;

	XP_Bool htmlCompose;
	PREF_GetBoolPref("mail.html_compose",&htmlCompose);
	if (theApp.m_bReverseSenseOfHtmlCompose)
		htmlCompose = !htmlCompose;

	if (editorType == MSG_HTML_EDITOR)
	  htmlCompose = TRUE;
	else if (editorType == MSG_PLAINTEXT_EDITOR)
	  htmlCompose = FALSE;

    INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pOldContext);
    int16 win_csid = INTL_GetCSIWinCSID(csi);
    if (!MSG_GetForcePlainText(fields) && htmlCompose)
        pDocument = (CGenericDoc*)theApp.m_ComposeTemplate->OpenDocumentFile(NULL, win_csid);
    else
        pDocument = (CGenericDoc*)theApp.m_TextComposeTemplate->OpenDocumentFile(NULL, win_csid);
    
    g_NastyContextSavingHack = pOldContext;

    if ( pDocument )
    {
        CWinCX * pContext = (CWinCX*) pDocument->GetContext();
        if ( pContext ) {

			MSG_CompositionPaneCallbacks Callbacks;
			Callbacks.CreateRecipientsDialog= CreateRecipientsDialog;
			Callbacks.CreateAskHTMLDialog= CreateAskHTMLDialog;

			MWContext *context = pContext->GetContext();
			CComposeFrame *pCompose = (CComposeFrame *) pContext->GetFrame()->GetFrameWnd();
            pCompose->SetComposeStuff(pOldContext,fields); // squirl away stuff for post-create
			// This needs to be set TRUE if using the old non-HTML text frame
            //   to prevent dropping dragged URLs
            pContext->m_bDragging = !pCompose->UseHtml();
            if (!pCompose->UseHtml()) 
			{
    			pCompose->SetMsgPane(MSG_CreateCompositionPane(
                    pContext->GetContext(), 
                    pOldContext, g_MsgPrefs.m_pMsgPrefs, fields,
			    	WFE_MSGGetMaster()));
			}
            else                
			{
    			pCompose->SetMsgPane(MSG_CreateCompositionPaneNoInit(
                    pContext->GetContext(), 
                    g_MsgPrefs.m_pMsgPrefs,
		    	WFE_MSGGetMaster()));
				//set the callbacks for askHTML and Recipients dialogs
				MSG_SetCompositionPaneCallbacks( pCompose->GetMsgPane(),
											&Callbacks,
											0);

				if (pCompose->GetMsgPane())
					MSG_SetHTMLMarkup(pCompose->GetMsgPane(), TRUE);
			}
			ASSERT(pCompose->GetMsgPane());
			MSG_SetFEData(pCompose->GetMsgPane(),(void *)pCompose);
            pCompose->UpdateAttachmentInfo();

			// Pass doccsid and win_csid info to new context for MailToWin conversion
			INTL_CharSetInfo	old_csi = LO_GetDocumentCharacterSetInfo(pOldContext);
			INTL_CharSetInfo	new_csi = LO_GetDocumentCharacterSetInfo(context);
			int16 doccsid = INTL_GetCSIDocCSID(old_csi);

			INTL_SetCSIDocCSID(new_csi, 
				(doccsid ? doccsid : INTL_DefaultDocCharSetID(pOldContext)));

			INTL_SetCSIWinCSID(new_csi, 
				(win_csid ? win_csid : INTL_DocToWinCharSetID(INTL_GetCSIDocCSID(new_csi))));

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
                pCompose->CompleteComposeInitialization();
                const char * pBody = pInitialText ? pInitialText : MSG_GetCompBody(pCompose->GetMsgPane());
                if (pBody)
                {
                    FE_InsertMessageCompositionText(context,pBody,TRUE);
					// We don't want to quote the original if open as draft
					// Check for the editorType before we do the quoting
					// Since only draft code will specify what editor to
					// use, checking for editorType == MSG_DEFAULT will do
					// the work.
                    if (MSG_ShouldAutoQuote(pCompose->GetMsgPane()) &&
											editorType == MSG_DEFAULT)
                    {
						pCompose->SetQuoteSelection();
                        MSG_QuoteMessage(
                            pCompose->GetMsgPane(),
                                WFE_InsertMessage,
                                (void*)pCompose->GetMainContext()->GetContext());
                    }
                }
            }
            else
            {
				pCompose->SetInitialText (pInitialText);
                URL_Struct * pUrl = NET_CreateURLStruct(EDT_NEW_DOC_URL,NET_DONT_RELOAD);
                if (pUrl != NULL)
                {
//                    pUrl->pre_exit_fn = wfe_GoldDoneLoading;
					pUrl->internal_url = TRUE;
                    pContext->GetUrl(pUrl, FO_CACHE_AND_PRESENT);
                    pContext->GetContext()->bIsComposeWindow = TRUE;
                }
            }

         if (MSG_GetAttachmentList(pCompose->GetMsgPane()))
            pCompose->SetModified(TRUE);
         else
            pCompose->SetModified(FALSE);

            // Post message to compose window to set the initial focus.
            pCompose->PostMessage(WM_COMP_SET_INITIAL_FOCUS);
			return pCompose->GetMsgPane();
		}
    }
    return NULL;
	} 
	else 
		return DoAltMailComposition(pOldContext, fields);
}

extern "C" void FE_RaiseMailCompositionWindow(MSG_Pane *pPane)	
{
    CComposeFrame * pCompose = (CComposeFrame *) MSG_GetFEData(pPane);

#ifdef XP_WIN32
    if(!theApp.m_hPostalLib) {
#endif
		pCompose->SendMessageToDescendants(WM_INITIALUPDATE, 0, 0, TRUE, TRUE);
        pCompose->ShowWindow(SW_SHOW);
		// Make Sure OnInitialUpdate gets called for the view.
        pCompose->DisplayHeaders(pCompose->GetSavedHeaders());
#ifdef XP_WIN32
    }
    else {
        FE_LoadExchangeInfo(pCompose->GetMainContext()->GetContext());
    }
#endif
}

extern "C" void FE_InsertMessageCompositionText(MWContext * context,
									const char* text,
									XP_Bool leaveCursorAtBeginning)
{
	if (context) {
		if (ABSTRACTCX(context)->IsDestroyed())
			return;
	   	CGenericFrame * pFrame = wfe_FrameFromXPContext(context);
		if (pFrame) 
        {
		    CComposeFrame * pCompose = (CComposeFrame *) pFrame;
		    if ( pCompose )
		    {
		        int nStartChar, nEndChar;
                if (!pCompose->UseHtml())
                {
		            if ( leaveCursorAtBeginning ) 
		                pCompose->GetEditor()->GetSel(nStartChar, nEndChar);
		            pCompose->GetEditor()->ReplaceSel ( text );
		            if ( leaveCursorAtBeginning ) 
		                pCompose->GetEditor()->SetSel (nStartChar, nEndChar, TRUE );
                }
                else
                {
                    ED_BufferOffset ins = -1;
		            if ( leaveCursorAtBeginning ){
		                ins = EDT_GetInsertPointOffset( context );
                    }

                    EDT_PasteQuote( context, (char*) text );

		            if ( leaveCursorAtBeginning && ins != -1 ) {
		                EDT_SetInsertPointToOffset( context, ins, 0 );
                    }

                }
                pCompose->GetChrome()->StopAnimation();
		    }
		}
	}
}

extern "C" void FE_DoneWithMessageBody(MSG_Pane *pPane, char* body,
								   uint32 body_size)
{
    XP_FREE(body);
}

extern "C" char * FE_PromptMessageSubject (MWContext * pContext)
{
    HWND hWnd = GetFocus();
    CString csText;
    CString csDefault;
    csText.LoadString(IDS_COMPOSE_NOSUBJECT);
    csDefault.LoadString(IDS_COMPOSE_DEFAULTNOSUBJECT);
    char * ptr = CFE_Prompt(pContext, csText, csDefault );
    SetFocus(hWnd);
    if (!ptr)
    {
       CGenericFrame * pFrame = wfe_FrameFromXPContext(pContext);
       CComposeFrame * pCompose = (CComposeFrame *) pFrame;
       pCompose->GetChrome()->StopAnimation();
    }
    return ptr;
}

#ifdef XP_WIN32

extern void FE_UpdateComposeWindowRecipients(MWContext *pContext,
									char *pTo, char *pCc, char *pBcc)
{
   CGenericFrame * pFrame = wfe_FrameFromXPContext(pContext);
   CComposeFrame * pCompose = (CComposeFrame *) pFrame;
   if ( pCompose )
   {
      CComposeBar * pBar = pCompose->GetComposeBar();
      ASSERT(pBar);
      pBar->UpdateRecipientInfo (pTo, pCc, pBcc);
   }
}

#endif 


extern "C" MSG_Pane * DoAltMailComposition(MWContext *pOldContext, MSG_CompositionFields* fields)
{
	CGenericDoc * pDocument;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pOldContext);
	pDocument = (CGenericDoc*)theApp.m_TextComposeTemplate->OpenDocumentFile(NULL, INTL_GetCSIWinCSID(csi));

	g_NastyContextSavingHack = pOldContext;

	if ( pDocument )
	{
		CWinCX * pContext = (CWinCX*) pDocument->GetContext();
		if ( pContext ) 
		{
			MWContext *context = pContext->GetContext();
			CComposeFrame *pCompose = (CComposeFrame *) pContext->GetFrame()->GetFrameWnd();
			pCompose->SetComposeStuff(pOldContext,fields); // squirl away stuff for post-create

			pCompose->SetMsgPane(MSG_CreateCompositionPane(
                pContext->GetContext(), 
                pOldContext, g_MsgPrefs.m_pMsgPrefs, fields,
			  	WFE_MSGGetMaster()));
			ASSERT(pCompose->GetMsgPane());
			MSG_SetFEData(pCompose->GetMsgPane(),(void *)pCompose);

			pCompose->DisplayHeaders(NULL); 

			FE_LoadExchangeInfo(context);

			return pCompose->GetMsgPane();
		}
	}
	return NULL;
}

#define MAX_MAIL_SIZE 300000
extern "C" void FE_DoneMailTo(PrintSetup * print) 
{
    ASSERT(print);

	MWContext * context = (MWContext *) print->carg;
	
	CGenericFrame * pFrame = wfe_FrameFromXPContext(context);
	ASSERT(pFrame);

	CComposeFrame *  pCompose;
	pCompose = (CComposeFrame *) pFrame;
	ASSERT(pFrame);
	
	MSG_Pane * pMsgPane = pCompose->GetMsgPane();
	ASSERT(pMsgPane);

    fclose(print->out);    

    //
    // No no no, get the size the right way
    //                            
    char * buffer;
    buffer = (char *) malloc(MAX_MAIL_SIZE + 5);
    FILE * fp = fopen(print->filename, "r");
    int len = fread(buffer, 1, MAX_MAIL_SIZE + 5, fp);
    buffer[len] = '\0';
    fclose(fp);                        


    if(theApp.m_hPostalLib) {

        if(theApp.m_bInitMapi) {
            if(theApp.m_fnOpenMailSession) {
                POSTCODE status = (*theApp.m_fnOpenMailSession) (NULL, NULL);
                if(status == POST_OK) {
                    theApp.m_bInitMapi = FALSE;
                }
                else {
                    FE_DestroyMailCompositionContext(context);
                    return;
                }
            }
        }

        // create mail window with no quoting
        if(theApp.m_fnComposeMailMessage)
            (*theApp.m_fnComposeMailMessage) ((const char *) (const char *) MSG_GetCompHeader(pMsgPane, MSG_TO_HEADER_MASK),
                                              (const char *) "",  /* no refs field.  BAD! BUG! */
                                              (const char *) MSG_GetCompHeader(pMsgPane, MSG_ORGANIZATION_HEADER_MASK),
                                              (const char *) "", /* no URL */
                                              (const char *) MSG_GetCompHeader(pMsgPane, MSG_SUBJECT_HEADER_MASK), 
                                              buffer,
											  (const char *) MSG_GetCompHeader(pMsgPane, MSG_CC_HEADER_MASK),											  
											  (const char *) MSG_GetCompHeader(pMsgPane, MSG_BCC_HEADER_MASK));

        // get rid of the file and free the memory  
        remove(print->filename);
        // XP_FREE(print->filename);
        // print->filename = NULL;
        XP_FREE(buffer);

        FE_DestroyMailCompositionContext(context);
        return;

    }
}

extern "C" int FE_LoadExchangeInfo(MWContext * context)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    if(!context)
        return(FALSE);

    if(!theApp.m_hPostalLib)
        return(FALSE);

	CGenericFrame * pFrame = wfe_FrameFromXPContext(context);
	ASSERT(pFrame);

	CComposeFrame *  pCompose;
	pCompose = (CComposeFrame *) pFrame;
	ASSERT(pFrame);
	
	MSG_Pane * pMsgPane = pCompose->GetMsgPane();
	ASSERT(pMsgPane);

    History_entry * hist_ent = NULL;
    if(g_NastyContextSavingHack)
        hist_ent = SHIST_GetCurrent(&(g_NastyContextSavingHack->hist));

    CString csURL;
    if(hist_ent)
        csURL = hist_ent->address;
    else
        csURL = "";

	//Set hist_ent to NULL if context->title is "Message Composition"
	//This is a nasty way of determining if we're in here in response
	//to "Mail Doc" or "New Mail Message".
	//Also, if there's To: field info present(pBar->m_pszTo) then 
	//we know that it's a Mailto: and set hist_ent to NULL 
	//Without this differentiation the code always sends the contents
	//of the previously mailed document even when someone chooses
	//"New Mail Message" or "Mailto:"
	const char * pszTo = MSG_GetCompHeader(pMsgPane, MSG_TO_HEADER_MASK);
	if(!strcmp(XP_GetString(MK_MSG_MSG_COMPOSITION), context->title) || strlen(pszTo) )
		hist_ent = NULL;

    // make sure there was a document loaded
    if(!hist_ent) {

        if(theApp.m_bInitMapi) {
            if(theApp.m_fnOpenMailSession) {
                POSTCODE status = (*theApp.m_fnOpenMailSession) (NULL, NULL);
                if(status == POST_OK) {
                    theApp.m_bInitMapi = FALSE;
                }
                else {
                    FE_DestroyMailCompositionContext(context);
                    return(FALSE);
                }
            }
        }

        // create mail window with no quoting
        if(theApp.m_fnComposeMailMessage)
            (*theApp.m_fnComposeMailMessage) ((const char *) MSG_GetCompHeader(pMsgPane, MSG_TO_HEADER_MASK),
                                              (const char *) "",  /* no refs field.  BAD! BUG! */
											  (const char *) MSG_GetCompHeader(pMsgPane, MSG_ORGANIZATION_HEADER_MASK),
                                              (const char *) "", /* no URL */
                                              (const char *) MSG_GetCompHeader(pMsgPane, MSG_SUBJECT_HEADER_MASK),
                                              "",
											  (const char *) MSG_GetCompHeader(pMsgPane, MSG_CC_HEADER_MASK),											  
											  (const char *) MSG_GetCompHeader(pMsgPane, MSG_BCC_HEADER_MASK));

        FE_DestroyMailCompositionContext(context);
        return(TRUE);
    }

    URL_Struct * URL_s = SHIST_CreateURLStructFromHistoryEntry(g_NastyContextSavingHack, hist_ent);

    // Zero out the saved data
	memset(&URL_s->savedData, 0, sizeof(URL_s->savedData));

    PrintSetup print;                                

    XL_InitializeTextSetup(&print);
    print.width = 68; 
    print.prefix = "";
    print.eol = "\r\n";

    char * name = WH_TempName(xpTemporary, NULL);
    if(!name) {
        FE_DestroyMailCompositionContext(context);
        return(FALSE);
    }

    print.out  = fopen(name, "w");
    print.completion = (XL_CompletionRoutine) FE_DoneMailTo;
    print.carg = context;
    print.filename = name;
    print.url = URL_s;

    // leave pCompose window alive until completion routine
    XL_TranslateText(g_NastyContextSavingHack, URL_s, &print); 
#endif /* MOZ_NGLAYOUT */

	return(TRUE);
}

extern "C" void FE_SecurityOptionsChanged(MWContext * pContext)
{
   CGenericFrame * pFrame = wfe_FrameFromXPContext(pContext);
   if ( pFrame ) {
	   CComposeFrame * pCompose = (CComposeFrame *) pFrame;
       pCompose->UpdateSecurityOptions();
   }
}
