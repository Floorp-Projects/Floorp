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

/*
	This file contains Navigator only code which supports "Send Page..." and
	"mailto:" URLs using an external mail client registered with InternetConfig.
*/

#ifndef MOZ_MAIL_NEWS

#include "msgcom.h"			// MSG_MailDocument()
#include "xlate.h"			// PrintSetup
#include "shist.h"			// SHIST_GetCurrent()
#include "xpgetstr.h"		// XP_GetString()
#include "CURLDispatcher.h"	// CURLDispatcher::DispatchURL()
#include "CNSContext.h"		// ExtractNSContext()
#include "net.h"			// NET_Escape()

extern int MK_MSG_MSG_COMPOSITION;

#define MAX_MAIL_SIZE 300000
extern "C" void FE_DoneMailTo(PrintSetup * print) ;
extern "C" void FE_DoneMailTo(PrintSetup * print) 
{
	const char * prefix = "mailto:?body=";
	const char * blankLines = "\r\n\r\n";
	
    XP_ASSERT(print);
    if (!print)
    	return;
    	
    XP_ASSERT(print->url);
    if (!print->url)
    	return;
    	
    fclose(print->out);    	// don't need this for writing anymore.

	MWContext * context = (MWContext *) (print->carg);
	if (!context)			// we'll require this later
		return;

    char * buffer = (char *) malloc(MAX_MAIL_SIZE);
    
    if (buffer) {
   		strcpy(buffer, print->url->address);
     	strcat(buffer, blankLines);
   	
   		int	buflen = strlen(buffer);
   		
		// now tack as much of the page onto the body as we have space for...
	    FILE * fp = fopen(print->filename, "r");
	    if (fp) {
		    int len = fread(buffer + buflen, 1, MAX_MAIL_SIZE - buflen - (5 /* slop? */), fp);
		    buffer[buflen + len] = '\0';
		    fclose(fp);  
		    
		    char *temp = NET_Escape (buffer, URL_XALPHAS);
		    
		    XP_FREE(buffer);
		    buffer = temp;
		    
	    } else {
	    
		    XP_FREE(buffer);
		    buffer = NULL;
		    
	    }
    }
                        
    // get rid of the file and free the memory  
    remove(print->filename);

	char *buffer2 = NULL;
	
	if (buffer) {
	
		buffer2 = (char *) malloc(strlen(prefix) + strlen(buffer) + 1);
		
		if (buffer2) {
    		strcpy(buffer2, prefix);				// start creating a "mailto:" URL
			strcat(buffer2, buffer);				// the message
		}
	}
	
	if (buffer2 == NULL) {			// no buffer, or we don't have enough memory to use it, try to just send the URL...
		if (buffer)
			XP_FREE(buffer);			// if we're here, we can't use the buffer anyway...
			
		buffer = NET_Escape (print->url->address, URL_XALPHAS);
		if (buffer == NULL)
			return;			// not enough memory to do ANYTHING useful!
			
		buffer2 = (char *) malloc(strlen(prefix) + strlen(buffer) + 1);
		if (buffer2 == NULL) {
			XP_FREE(buffer);
			return;			// not enough memory to do ANYTHING useful!
		}
		
    	strcpy(buffer2, prefix);				// start creating a "mailto:" URL
		strcat(buffer2, buffer);				// the message
	}

	XP_FREE(buffer);
	
    // XP_FREE(print->filename);
    // print->filename = NULL;
    CURLDispatcher::DispatchURL(buffer2, ExtractNSContext(context));
    XP_FREE(buffer2);
}

#ifndef MOZ_MAIL_COMPOSE
extern MSG_Pane* MSG_MailDocument(MWContext *context)
{
    if(!context)
        return NULL;

    History_entry * hist_ent = SHIST_GetCurrent(&(context->hist));
        
    // make sure there was a document loaded
    if(!hist_ent)
        return NULL;


	//Set hist_ent to NULL if context->title is "Message Composition"
	//This is a nasty way of determining if we're in here in response
	//to "Mail Doc" or "New Mail Message".
	//Also, if there's To: field info present(pBar->m_pszTo) then 
	//we know that it's a Mailto: and set hist_ent to NULL 
	//Without this differentiation the code always sends the contents
	//of the previously mailed document even when someone chooses
	//"New Mail Message" or "Mailto:"
   
	if(!strcmp(XP_GetString(MK_MSG_MSG_COMPOSITION), context->title))
        return NULL;

    URL_Struct * URL_s = SHIST_CreateURLStructFromHistoryEntry(context, hist_ent);
    if (!URL_s)
    	return NULL;

    // Zero out the saved data
	memset(&URL_s->savedData, 0, sizeof(URL_s->savedData));

    PrintSetup print;                                

    XL_InitializeTextSetup(&print);
    print.width = 68; 
    print.prefix = "";
    print.eol = "\r\n";

    char * name = WH_TempName(xpTemporary, NULL);
    if(!name) {
    	// Leak URL_s here
        return(FALSE);
    }

    print.out  = fopen(name, "w");
    print.completion = (XL_CompletionRoutine) FE_DoneMailTo;
    print.carg = context;
    print.filename = name;
    print.url = URL_s;

    // leave pCompose window alive until completion routine
    XL_TranslateText(context, URL_s, &print); 

    return NULL;
}
#endif // ! MOZ_MAIL_COMPOSE

#endif // !MOZ_MAIL_NEWS