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
#include "allxpstr.h"
#include "xpgetstr.h"

typedef struct {
    char * pszTo;
    char * pszSubject;
    char * pszOrganization;
} MAPI_HEADER_INFO;

static MAPI_HEADER_INFO _temp;

#ifdef XP_WIN32
extern "C" int FE_LoadExchangeInfo(
   MWContext * context,
   char * pszTo, char * pszCc, char * pszBcc,
   char * pszOrganization,
   char * pszNewsgroups,
   char * pszSubject);
#endif

#define MAX_MAIL_SIZE 300000
extern "C" void FE_DoneMailTo(PrintSetup * print) 
{
    ASSERT(print);

	MWContext * context = (MWContext *) print->carg;
	CGenericFrame * pFrame = wfe_FrameFromXPContext(context);
	ASSERT(pFrame);
    fclose(print->out);    

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
            }
        }

        // create mail window with no quoting
        if(theApp.m_fnComposeMailMessage)
            (*theApp.m_fnComposeMailMessage) ((const char *) _temp.pszTo,
                                              (const char *) "",  /* no refs field.  BAD! BUG! */
                                              (const char *) _temp.pszOrganization,
                                              (const char *) "", /* no URL */
                                              (const char *) _temp.pszSubject, 
                                              buffer,
											  (const char *) "",											  
											  (const char *) "");

        if (strlen(_temp.pszTo))
            free(_temp.pszTo);
        if (strlen(_temp.pszSubject))
            free(_temp.pszSubject);
        if (strlen(_temp.pszOrganization))
            free(_temp.pszOrganization);

        // get rid of the file and free the memory  
        remove(print->filename);

        // XP_FREE(print->filename);
        // print->filename = NULL;
        XP_FREE(buffer);

        return;

    }
}

extern "C" int FE_LoadExchangeInfo(
   MWContext * context,
   char * pszTo, char * pszCc, char * pszBcc,
   char * pszOrganization,
   char * pszNewsgroups,
   char * pszSubject)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    _temp.pszTo = _temp.pszSubject = _temp.pszOrganization = "";
    if(!context)
        return(FALSE);

    if(!theApp.m_hPostalLib)
        return(FALSE);

    History_entry * hist_ent = NULL;
    if(context)
        hist_ent = SHIST_GetCurrent(&(context->hist));

    CString csURL;
    if(hist_ent)
        csURL = hist_ent->address;
    else
        csURL = "";

    if (!pszSubject || !strlen(pszSubject))
        pszSubject = (char *)(const char *)csURL;

	//Set hist_ent to NULL if context->title is "Message Composition"
	//This is a nasty way of determining if we're in here in response
	//to "Mail Doc" or "New Mail Message".
	//Also, if there's To: field info present(pBar->m_pszTo) then 
	//we know that it's a Mailto: and set hist_ent to NULL 
	//Without this differentiation the code always sends the contents
	//of the previously mailed document even when someone chooses
	//"New Mail Message" or "Mailto:"
   
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
                    return(FALSE);
                }
            }
        }

        // create mail window with no quoting
        if(theApp.m_fnComposeMailMessage)
            (*theApp.m_fnComposeMailMessage) (
                                    (const char *)pszTo,
                                    (const char *) "",  /* no refs field.  BAD! BUG! */
											   (const char *)pszOrganization,
                                    (const char *) "", /* no URL */
                                    (const char *)pszSubject,
                                    "",
											   (const char *)pszCc,											  
											   (const char *)pszBcc);
        return(TRUE);
    }

    URL_Struct * URL_s = SHIST_CreateURLStructFromHistoryEntry(context, hist_ent);

    // Zero out the saved data
	memset(&URL_s->savedData, 0, sizeof(URL_s->savedData));

    PrintSetup print;                                

    XL_InitializeTextSetup(&print);
    print.width = 68; 
    print.prefix = "";
    print.eol = "\r\n";

    char * name = WH_TempName(xpTemporary, NULL);
    if(!name) {
        return(FALSE);
    }

    print.out  = fopen(name, "w");
    print.completion = (XL_CompletionRoutine) FE_DoneMailTo;
    print.carg = context;
    print.filename = name;
    print.url = URL_s;

    if (pszSubject && strlen(pszSubject))
        _temp.pszSubject = strdup(pszSubject);
    if (pszTo && strlen(pszTo))
        _temp.pszTo = strdup(pszTo);
    if (pszOrganization && strlen(pszOrganization))
        _temp.pszOrganization = strdup(pszOrganization);

    // leave pCompose window alive until completion routine
    XL_TranslateText(context, URL_s, &print); 
#endif /* MOZ_NGLAYOUT */

    return(TRUE);
}

void InitializeMapi(void)
{
   
}

extern "C" void DoAltMailComposition(MWContext *pContext)
{
	if ( pContext ) 
	{
        FE_LoadExchangeInfo(
            pContext,
            "",
            "",
            "",
            (char *)FE_UsersOrganization(),
            "",
            "");
	}
}

extern "C" void FE_AlternateCompose(
    char * from, char * reply_to, char * to, char * cc, char * bcc,
    char * fcc, char * newsgroups, char * followup_to,
    char * organization, char * subject, char * references,
    char * other_random_headers, char * priority,
    char * attachment, char * newspost_url, char * body)
{
    if(theApp.m_bInitMapi) {
        if(theApp.m_fnOpenMailSession) {
            POSTCODE status = (*theApp.m_fnOpenMailSession) (NULL, NULL);
            if(status == POST_OK) {
                theApp.m_bInitMapi = FALSE;
            }
            else
                return;
        }
    }

    // create mail window with no quoting
    if(theApp.m_fnComposeMailMessage)
        (*theApp.m_fnComposeMailMessage) (
            (const char *)to, 
            (const char *)references, 
            (const char *)(organization ? organization : FE_UsersOrganization()), 
            (const char *)"", 
            (const char *)subject, 
            (const char *)body,
            (const char *)cc, 
            (const char *)bcc );
}

