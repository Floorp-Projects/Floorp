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
    Session History module

    Public Functions
        void         SHIST_InitSession(MWContext * ctxt);
        void         SHIST_AddDocument(MWContext * ctxt, char * name, char * url,
                               void * loc);
        const char * SHIST_GetNext(MWContext * ctxt);
        const char * SHIST_GetPrevious(MWContext * ctxt);
        const char * SHIST_GetEntry(MWContext * ctxt, int entry_number);
        int          SHIST_CanGoBack(MWContext * ctxt);
        int          SHIST_CanGoForward(MWContext * ctxt);
*/

/*#define HAS_FE*/

#include "shist.h"

#include "libi18n.h"

#include "hotlist.h"
#include "net.h"
#include "xp.h"
#include "secnav.h"

#if !defined(XP_MAC) && !defined(XP_WIN32)	/* macOS doesn't need this anymore... nor does Windows */
	#include "bkmks.h"
#endif

#ifdef EDITOR
#include "edt.h" /* for EDT_IS_EDITOR macro */
extern char *XP_NEW_DOC_URL;
extern char *XP_NEW_DOC_NAME;
#include "xpgetstr.h"
#endif

#include "libmocha.h"
#include "glhist.h"

static int32 unique_id;

PRIVATE int shist_update_FE(MWContext * ctxt);


PUBLIC History_entry *
SHIST_HoldEntry (History_entry * entry)
{
	if (entry)
		entry->ref_count++;
	return entry;
}

PUBLIC void
SHIST_FreeHistoryEntry (MWContext * ctxt, History_entry * entry)
{
	if(entry && --entry->ref_count == 0)
	  {
		if(entry->title)
			XP_FREE (entry->title);
		if(entry->address)
			XP_FREE(entry->address);
		if(entry->content_name)
			XP_FREE(entry->content_name);
		if(entry->referer)
			XP_FREE(entry->referer);
	
		if(entry->post_data_is_file && entry->post_data)
			XP_FileRemove(entry->post_data, xpFileToPost);
		if(entry->post_data)
			XP_FREE(entry->post_data);
		if(entry->post_headers)
			XP_FREE(entry->post_headers);
    	    XP_FREE(entry->sec_info);
    	if(entry->refresh_url)
    	    XP_FREE(entry->refresh_url);
    	if(entry->wysiwyg_url)
    	    XP_FREE(entry->wysiwyg_url);
    	if(entry->page_services_url)
    	    XP_FREE(entry->page_services_url);
#ifdef PRIVACY_POLICIES
    	if(entry->privacy_policy_url)
    	    XP_FREE(entry->privacy_policy_url);
#endif /* PRIVACY_POLICIES */

#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
		if(entry->savedData.FormList)
			LO_FreeDocumentFormListData(ctxt, entry->savedData.FormList); 

		if(entry->savedData.EmbedList)
			LO_FreeDocumentEmbedListData(ctxt, entry->savedData.EmbedList); 

		if(entry->savedData.Grid)
			LO_FreeDocumentGridData(ctxt, entry->savedData.Grid); 

		if(entry->savedData.Window)
			LM_DropSavedWindow(ctxt, entry->savedData.Window);
#endif /* MOZ_NGLAYOUT */
		if(entry->savedData.OnLoad)
			PA_FREE(entry->savedData.OnLoad);
		if(entry->savedData.OnUnload)
			PA_FREE(entry->savedData.OnUnload);
		if(entry->savedData.OnFocus)
			PA_FREE(entry->savedData.OnFocus);
		if(entry->savedData.OnBlur)
			PA_FREE(entry->savedData.OnBlur);
		if(entry->savedData.OnHelp)
			PA_FREE(entry->savedData.OnHelp);
		if(entry->savedData.OnMouseOver)
			PA_FREE(entry->savedData.OnMouseOver);
		if(entry->savedData.OnMouseOut)
			PA_FREE(entry->savedData.OnMouseOut);
		if(entry->savedData.OnDragDrop)
			PA_FREE(entry->savedData.OnDragDrop);
		if(entry->savedData.OnMove)
			PA_FREE(entry->savedData.OnMove);
		if(entry->savedData.OnResize)
			PA_FREE(entry->savedData.OnResize);


		XP_FREE(entry);
	  }
}

PUBLIC History_entry *
SHIST_CreateHistoryEntry (URL_Struct * URL_s, char * title)
{
	History_entry * new_entry;

	if(!URL_s)
		return(NULL);

	new_entry = XP_NEW(History_entry);

	if(!new_entry)
		return(NULL);

	memset(new_entry, 0, sizeof(History_entry));

	new_entry->ref_count = 1;
	new_entry->unique_id = ++unique_id;
	new_entry->history_num = URL_s->history_num;

	new_entry->method         = URL_s->method;
	new_entry->is_binary      = URL_s->is_binary;
	new_entry->is_active      = URL_s->is_active;
	new_entry->is_netsite     = URL_s->is_netsite;
	new_entry->last_modified  = URL_s->last_modified;
	new_entry->post_data_size = URL_s->post_data_size;
	new_entry->post_data_is_file = URL_s->post_data_is_file;
    new_entry->security_on    = URL_s->security_on;
    new_entry->sec_info = SECNAV_CopySSLSocketStatus(URL_s->sec_info);

	new_entry->refresh        = URL_s->refresh;

    if(URL_s->memory_copy)
        new_entry->transport_method = SHIST_CAME_FROM_MEMORY_CACHE;
	else if(URL_s->cache_file)
        new_entry->transport_method = SHIST_CAME_FROM_DISK_CACHE;
	else
        new_entry->transport_method = SHIST_CAME_FROM_NETWORK;

    StrAllocCopy(new_entry->referer,      URL_s->referer);
    StrAllocCopy(new_entry->refresh_url,  URL_s->refresh_url);
    StrAllocCopy(new_entry->wysiwyg_url,  URL_s->wysiwyg_url);

	StrAllocCopy(new_entry->title,        title);
	StrAllocCopy(new_entry->address,      URL_s->address);
	StrAllocCopy(new_entry->content_name, URL_s->content_name);
	StrAllocCopy(new_entry->post_data,    URL_s->post_data);
	StrAllocCopy(new_entry->post_headers, URL_s->post_headers);
#ifdef PRIVACY_POLICIES
	StrAllocCopy(new_entry->privacy_policy_url, URL_s->privacy_policy_url);
#endif
	StrAllocCopy(new_entry->page_services_url, URL_s->page_services_url);
	StrAllocCopy(new_entry->etag, URL_s->etag);

	return(new_entry);
}


PUBLIC URL_Struct *
SHIST_CreateURLStructFromHistoryEntry(MWContext * ctxt, History_entry * entry)
{
	URL_Struct * URL_s;
	if(!entry)
	    return(NULL);

	URL_s = NET_CreateURLStruct(entry->address, NET_DONT_RELOAD);
	if(!URL_s)
	    return(NULL);

	URL_s->method = entry->method;
	StrAllocCopy(URL_s->content_name, entry->content_name);
	StrAllocCopy(URL_s->post_data, entry->post_data);
	StrAllocCopy(URL_s->post_headers, entry->post_headers);

	StrAllocCopy(URL_s->referer,      entry->referer);
    StrAllocCopy(URL_s->refresh_url,  entry->refresh_url);
    StrAllocCopy(URL_s->wysiwyg_url,  entry->wysiwyg_url);

	StrAllocCopy(URL_s->etag,  entry->etag);

    URL_s->refresh        = entry->refresh;
    URL_s->security_on    = entry->security_on;
    URL_s->sec_info = SECNAV_CopySSLSocketStatus(entry->sec_info);

	URL_s->post_data_size = entry->post_data_size;
	URL_s->post_data_is_file = entry->post_data_is_file;
	URL_s->position_tag   = entry->position_tag;
	URL_s->history_num    = SHIST_GetIndex(&ctxt->hist, entry);
	URL_s->savedData      = entry->savedData; /* copy whole struct */
	URL_s->is_binary      = entry->is_binary;
	URL_s->is_active      = entry->is_active;
	URL_s->is_netsite     = entry->is_netsite;
	URL_s->last_modified  = entry->last_modified;

	return(URL_s);
}

URL_Struct *
SHIST_CreateWysiwygURLStruct(MWContext * ctxt, History_entry * entry)
{
	URL_Struct *URL_s;

	URL_s = SHIST_CreateURLStructFromHistoryEntry(ctxt, entry);
	if (URL_s && URL_s->wysiwyg_url)
	  {
		StrAllocCopy(URL_s->address, URL_s->wysiwyg_url);
	  }
	return URL_s;
}

/*
 * Returns NULL if the entry cannot be a hotlist item (form submission).
 */

#if !defined(XP_MAC) && !defined(XP_WIN32)	/* macOS doesn't need this anymore -- we're 100% RDF! so is windows! */

BM_Entry* SHIST_CreateHotlistStructFromHistoryEntry(History_entry * h)
{
	BM_Entry* hs = NULL;
	if ((h == NULL) || h->post_data)	/* Cannot make form submission into bookmarks */
		return NULL;
	
	/* if title is empty make the title the URL in the form www.XXX...YYY.html */
	if ( *h->title )
		hs = BM_NewUrl( h->title, h->address, NULL, h->last_access);
	else
	{
		/* Strip off the protocol info (eg, http://) */
		char trunkedAddress [HIST_MAX_URL_LEN + 1];
		char* strippedAddress = SHIST_StripProtocol ( h->address );
		
		/* truncate it */
		INTL_MidTruncateString ( 0, strippedAddress, trunkedAddress, HIST_MAX_URL_LEN );
		hs = BM_NewUrl( trunkedAddress, h->address, NULL, h->last_access);
	}
	
	return hs;
	
} /* SHIST_CreateHotlistStructFromHistoryEntry */
#endif

/*
 * SHIST_StripProtocol
 *
 * Given a URL (eg, http://foo.com/foo/bar), strips off the protocol part. This does not
 * create a new string but returns an index into the original URL.
 */
 
char* SHIST_StripProtocol ( char* inURL )
{
	char* startOfAddress;
	
	if ( !inURL )
		return NULL;

	startOfAddress = strchr ( inURL, ':' );		/* find the colon */
	if ( startOfAddress ) 
	{
		if ( *(startOfAddress+1) == '/' )
			if ( *(startOfAddress+2) == '/' )	/* http://foo, ftp://foo ... */
				startOfAddress += 3;
			else								/* can this case ever exist? */
				startOfAddress += 2;
		else									/* mailto:foo or news:foo */
			startOfAddress++;
	}
	else										/* no colons, should never happen... */
		startOfAddress = inURL;			

	return startOfAddress;
	
} /* SHIST_StripProtocol */


/* ---

   Name: shist_update_FE 
   Parameters:
    ctxt - (read) session context
   Returns:
        status from frontend 
   Description:
        The history list has changed.  Make sure the forward / back button
        on the front end are in sync.

--- */
PRIVATE int
shist_update_FE(MWContext * ctxt)
{
    int status = TRUE;

#ifdef XP_UNIX

    if ((ctxt->is_grid_cell)&&(ctxt->grid_parent != NULL))
    {
	status = shist_update_FE(ctxt->grid_parent);
    }

    if(ctxt->hist.num_entries > ctxt->hist.cur_doc)
    {
	status = FE_EnableForwardButton(ctxt);
    }
    else
    {
	if ((ctxt->grid_children != NULL)&&(LO_GridCanGoForward(ctxt)))
	{
		status = FE_EnableForwardButton(ctxt);
	}
	else
	{
		status = FE_DisableForwardButton(ctxt);
	}
    }

    if(status < 0)
        return(status);

    if(ctxt->hist.cur_doc > 1)
    {
	status = FE_EnableBackButton(ctxt);
    }
    else
    {
	if ((ctxt->grid_children != NULL)&&(LO_GridCanGoBackward(ctxt)))
	{
		status = FE_EnableBackButton(ctxt);
	}
	else
	{
		status = FE_DisableBackButton(ctxt);
	}
    }

#endif /* XP_UNIX */

    return(status);

} /* shist_update_FE */


/* ---

   Name: SHIST_InitSession
   Parameters:
    ctxt - (modified) session context
   Returns:
        void
   Description:
        Initialize the session history module.  Since we are initializing
        assume there is not a current history list that we need to free.

--- */
PUBLIC void
SHIST_InitSession(MWContext * ctxt)
{

    if(!ctxt)
        return;

    ctxt->hist.cur_doc  = 0;
    ctxt->hist.cur_doc_ptr  = 0;
	ctxt->hist.list_ptr = XP_ListNew();
	ctxt->hist.num_entries = 0;
	ctxt->hist.max_entries = -1;
    shist_update_FE(ctxt);

#ifdef JAVA
	PR_INIT_CLIST(&ctxt->javaContexts);
#if defined(XP_PC) || defined(XP_UNIX)
	{
		char *ev = getenv("NS_HISTORY_LIMIT");
		int limit = 50;
		if (ev && ev[0]) {
			limit = atoi(ev);
			if ((limit < 3) || (limit >= 32767)) {
				limit = 50;
			}
		}
		ctxt->hist.max_entries = limit;
	}
#endif /* PC or UNIX */
#endif /* JAVA */

} /* SHIST_InitSession */

PUBLIC void
SHIST_EndSession(MWContext * ctxt)
{
	XP_List * list_ptr = ctxt->hist.list_ptr;
	History_entry * old_entry;

    while((old_entry = (History_entry *) XP_ListRemoveTopObject(list_ptr))!=0)
	    SHIST_FreeHistoryEntry(ctxt, old_entry);

	XP_ListDestroy(ctxt->hist.list_ptr);

	ctxt->hist.list_ptr = 0;
    ctxt->hist.cur_doc = 0;
    ctxt->hist.cur_doc_ptr = 0;
}

/* duplicate the given entry
 */
PUBLIC History_entry *
SHIST_CloneEntry(History_entry * old_entry)
{
    History_entry *new_entry = NULL;

    if(old_entry == NULL)
	return NULL;

    new_entry = XP_NEW_ZAP(History_entry);

    if(new_entry == NULL)
	return NULL;

    StrAllocCopy(new_entry->title, old_entry->title);
    StrAllocCopy(new_entry->address, old_entry->address);
    StrAllocCopy(new_entry->content_name, old_entry->content_name);
#ifdef PRIVACY_POLICIES
    StrAllocCopy(new_entry->privacy_policy_url, old_entry->privacy_policy_url);
#endif /* PRIVACY_POLICIES */
    StrAllocCopy(new_entry->referer, old_entry->referer);
    StrAllocCopy(new_entry->post_data, old_entry->post_data);
    StrAllocCopy(new_entry->post_headers, old_entry->post_headers);

    new_entry->unique_id = ++unique_id;
    new_entry->post_data_size = old_entry->post_data_size;
    new_entry->post_data_is_file = old_entry->post_data_is_file;
    new_entry->method         = old_entry->method;
    new_entry->is_binary      = old_entry->is_binary;
    new_entry->is_active      = old_entry->is_active;
    new_entry->is_netsite     = old_entry->is_netsite;
    new_entry->position_tag   = old_entry->position_tag;
    new_entry->security_on    = old_entry->security_on;
    new_entry->sec_info = SECNAV_CopySSLSocketStatus(old_entry->sec_info);

    new_entry->last_modified  = old_entry->last_modified;

	StrAllocCopy(new_entry->page_services_url, old_entry->page_services_url);

    new_entry->ref_count = 1;
    return new_entry;

}



/* copys all the session data from the old context into the
 * new context.  Does not effect data in old_context session history
 *
 * if new_context has not had SHIST_InitSession called for it
 * it will be called to initalize it.
 */
PUBLIC void
SHIST_CopySession(MWContext * new_context, MWContext * old_context)
{
    XP_List * list_ptr;
    History_entry *new_entry, *old_entry;
    int x, y;

    if(!new_context || !old_context)
	return;

    if(!new_context->hist.list_ptr)
	SHIST_InitSession(new_context);

    list_ptr = old_context->hist.list_ptr;

    x = XP_ListCount(new_context->hist.list_ptr);
    y = XP_ListCount(old_context->hist.list_ptr);

    while((old_entry = (History_entry *) XP_ListNextObject(list_ptr))!=0) {
#ifdef EDITOR
        /* Skip new document entries when copying from Editor context */
        if ( EDT_IS_EDITOR(old_context) && old_entry->address && 
             (0 == XP_STRCMP(old_entry->address, XP_NEW_DOC_NAME) ||
              0 == XP_STRCMP(old_entry->address, XP_NEW_DOC_URL)) )
        {
            continue;            
        }
#endif
    	new_entry = SHIST_CloneEntry(old_entry);
    	if(!new_entry)
	    continue;

	XP_ListAddObjectToEnd(new_context->hist.list_ptr, SHIST_HoldEntry(new_entry));
    }

    new_context->hist.num_entries = XP_ListCount(new_context->hist.list_ptr);
    new_context->hist.max_entries = old_context->hist.max_entries;
    if(new_context->hist.num_entries > 0) {
	new_context->hist.cur_doc = 1;
	new_context->hist.cur_doc_ptr = 
	    (History_entry *) XP_ListGetObjectNum(new_context->hist.list_ptr,
						  new_context->hist.cur_doc);
    }

    shist_update_FE(new_context);
}

/* ---

   Name: SHIST_AddDocument
   Parameters:
    ctxt - (modified) session context
        name - (read) name of document to add
        url  - (read) url of document to add
        loc  - (read) location in current document for backtracking later
   Returns:
        void
   Description:
        Add a document after the current document in the history list.  If
        there were other documents after the current document free their
        records cuz we won't have any logical way to get to them via the
        history list.

        We need to save some sort of location information about the current
        document so if we backtrack to it later we know what to show to the
        user.

--- */         
PUBLIC void
SHIST_AddDocument(MWContext * ctxt, History_entry * new_entry)
{
	int count;
/*	XP_List * list_ptr = ctxt->hist.list_ptr;*/
	History_entry * old_entry;
    
    if(!ctxt || !new_entry)
        return;
#ifdef EDITOR
    /* If not done, get our strings out of XP_MSG.I resources 
       (Probably done during first NET_GetURL() call, but lets be sure)
    */
    if (XP_NEW_DOC_URL == NULL) {
        StrAllocCopy(XP_NEW_DOC_URL, "about:editfilenew");
        /* StrAllocCopy(XP_NEW_DOC_URL, XP_GetString(XP_NEW_EDIT_DOC_URL) ); */
    }
    if (XP_NEW_DOC_NAME == NULL) {
        StrAllocCopy(XP_NEW_DOC_NAME, "file:///Untitled" );
        /* StrAllocCopy(XP_NEW_DOC_NAME, XP_GetString(XP_NEW_EDIT_DOC_NAME) );*/
    }
	/* Detect attempts to add "about:editfilenew"
	   history entry and replace with "file:///Untitled".
    */
	if (new_entry->address && 
	    0 == XP_STRCMP(new_entry->address, XP_NEW_DOC_URL) )
    {
        XP_FREE(new_entry->address);
        /* Remove title */
        if(new_entry->title) {
            XP_FREE(new_entry->title);
			new_entry->title = NULL;   /* as long as we're XP_FREEing it, lets NULL it out... */
        }
        new_entry->address = XP_STRDUP(XP_NEW_DOC_NAME);
        /* Leave title empty -- it will showup as "Untitled" in caption */
        /* new_entry->title = XP_STRDUP(XP_NEW_DOC_NAME); */
	    /* Unlikely, but abort if no name generated */
        if (!new_entry->address)
	      {
		    SHIST_FreeHistoryEntry(ctxt, new_entry);
		    return;
	      }
    }

#endif

	/* If this new entry doesn't have a history_num, and it has the same
	   URL as the topmost history entry, then don't add this to the
	   history - just use the old one. */
	old_entry = SHIST_GetCurrent(&ctxt->hist);
	if (old_entry &&
		!new_entry->history_num &&
		!old_entry->post_data && !new_entry->post_data &&
		!XP_STRCMP(old_entry->address, new_entry->address))
	  {
		/* don't add this one to the history */
		SHIST_FreeHistoryEntry(ctxt, new_entry);
		return;
	  }

	/* Only web browser windows record history - mail, news, composition,
	   etc windows do not.

	   In a non-web-browser window, cause there to be exactly one entry
	   on the history list (there needs to be one, because many things
	   use the history to find out the current URL.)  Do this by emptying
	   out the list each time this is called - we will then add this entry
	   to then end of the now-empty list.
	 */
	if (ctxt->type != MWContextBrowser && ctxt->type != MWContextPane &&
		/* For mail and news windows, history_num is only non-zero when the
		   window is being resized - in which case, the only item on the
		   history has the same URL (right?) and we will copy the contents
		   of the new entry into the old, free the new entry, and not free
		   the form data that is in the old.  Gag. */
		new_entry->history_num == 0)
	  {
		XP_List * list_ptr = ctxt->hist.list_ptr;
		History_entry * old_entry;

		while((old_entry = (History_entry *)XP_ListRemoveTopObject(list_ptr))
			  !=0)
		{
			SHIST_FreeHistoryEntry(ctxt, old_entry);
		}
		ctxt->hist.cur_doc = 0;
		ctxt->hist.cur_doc_ptr = 0;
	  }

	/* If this entry doesn't have a URL in it, throw it away. */
	if (!new_entry->address || !*new_entry->address)
	  {
		SHIST_FreeHistoryEntry(ctxt, new_entry);
		return;
	  }

	if(new_entry->history_num)
	  {
    	old_entry = (History_entry *) XP_ListGetObjectNum(ctxt->hist.list_ptr, 
														  new_entry->history_num);

		/* make sure that we have the same entry in case something weird 
		 * happened
		 *
		 * @@@ approx match. should check post data
		 */
		if(old_entry &&
		   (!XP_STRCMP(old_entry->address, new_entry->address) ||
			old_entry->replace ||
			(old_entry->wysiwyg_url &&
			 !XP_STRCMP(old_entry->wysiwyg_url, new_entry->address))))
		  {
			if(old_entry->replace)
			  {
				StrAllocCopy(old_entry->title, new_entry->title);
				StrAllocCopy(old_entry->address, new_entry->address);
				StrAllocCopy(old_entry->post_data, new_entry->post_data);
				StrAllocCopy(old_entry->post_headers, new_entry->post_headers);

				old_entry->post_data_size = new_entry->post_data_size;
				old_entry->post_data_is_file = new_entry->post_data_is_file;
				old_entry->method         = new_entry->method;
				old_entry->position_tag   = new_entry->position_tag;
				old_entry->is_binary      = new_entry->is_binary;
				old_entry->is_active      = new_entry->is_active;
				old_entry->is_netsite     = new_entry->is_netsite;

				/* Unique identifier */
				old_entry->unique_id = ++unique_id;
			  }

			/* this stuff has the possibility of changing,
			 * so copy it
			 */
    		StrAllocCopy(old_entry->referer,      new_entry->referer);
    		StrAllocCopy(old_entry->refresh_url,  new_entry->refresh_url);
    		StrAllocCopy(old_entry->wysiwyg_url,  new_entry->wysiwyg_url);
    		StrAllocCopy(old_entry->content_name, new_entry->content_name);

    		old_entry->refresh          = new_entry->refresh;
    		old_entry->security_on      = new_entry->security_on;
            old_entry->sec_info =
                SECNAV_CopySSLSocketStatus(new_entry->sec_info);

    		old_entry->transport_method = new_entry->transport_method;
		    old_entry->last_modified  = new_entry->last_modified;
    		old_entry->last_access = time(NULL);

			ctxt->hist.cur_doc = new_entry->history_num;
		    ctxt->hist.cur_doc_ptr = old_entry;

		    SHIST_FreeHistoryEntry(ctxt, new_entry);
    		shist_update_FE(ctxt);
			return;
		  }
		/* something went wrong. Add it to the end of the list
		 */
	  }

    new_entry->last_access = time(NULL);
    
	/* take 'em off from the end so we don't bust the XP_List layering */
	for(count = XP_ListCount(ctxt->hist.list_ptr); count > ctxt->hist.cur_doc; count--) {
		old_entry = (History_entry *) XP_ListRemoveEndObject(ctxt->hist.list_ptr);
		SHIST_FreeHistoryEntry(ctxt, old_entry);
	}

	/* if the history list is too long, shorten it by taking things off
	   the start of the list. */
	if (ctxt->hist.max_entries > 0) {
		while (ctxt->hist.cur_doc >= ctxt->hist.max_entries)
		{
			old_entry = (History_entry *)
				XP_ListRemoveTopObject(ctxt->hist.list_ptr);
			SHIST_FreeHistoryEntry(ctxt, old_entry);
			ctxt->hist.cur_doc--;
		}
	}

	/* add the new element to the end */	
    XP_ListAddObjectToEnd(ctxt->hist.list_ptr, new_entry);
	ctxt->hist.num_entries = XP_ListCount(ctxt->hist.list_ptr);

    ctxt->hist.cur_doc++;
	ctxt->hist.cur_doc_ptr = (History_entry *) XP_ListGetObjectNum(ctxt->hist.list_ptr, 
												 ctxt->hist.cur_doc);

#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    LO_CleanupGridHistory(ctxt);
    if (ctxt->is_grid_cell)
    {
	LO_UpdateGridHistory(ctxt);
    }
#endif /* MOZ_NGLAYOUT */

    shist_update_FE(ctxt);

} /* SHIST_AddDocument */

#if defined(DEBUG) && !defined(_WINDOWS)
/* dump a history list to stdout */
PUBLIC void
SHIST_DumpHistory(MWContext * ctxt) 
{
	XP_List * list_ptr = ctxt->hist.list_ptr;
	History_entry * entry;

	while((entry = (History_entry*) XP_ListNextObject(list_ptr)))
	  {
#ifdef XP_UNIX 
		printf("SHIST: cur_doc:%d  	address:%s	\n", ctxt->hist.cur_doc, entry->address);
#endif
      }
#ifdef XP_UNIX 
    printf("---End of History List---\n");
#endif
} /* SHIST_DumpHistory */
#endif /* DEBUG */



/* ---
    Name: SHIST_GetPrevious
    Return:
         URL of previous document or NULL on failure
    Parameters:
         ctxt - (modified) window context
    Description:
		The entry returned should not be free'd by the caller

         calls FE functions to fix dimming of next and
    previous buttons as needed.

--- */
PUBLIC History_entry *
SHIST_GetPrevious(MWContext * ctxt)
{
    /* check to make sure is a previous record */
    if(!ctxt || ctxt->hist.cur_doc < 2)
        return NULL;

    return((History_entry *) XP_ListGetObjectNum(ctxt->hist.list_ptr, ctxt->hist.cur_doc-1));
}

PUBLIC History_entry *
SHIST_GetCurrent(History * hist)
{

    if(!hist)
        return NULL;

    /* return URL */
    return(hist->cur_doc_ptr);

} 

#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2)
PUBLIC void
SHIST_SetTitleOfCurrentDoc( MWContext *context )
{
 	History_entry *  he;
    URL_Struct *     pUrl;
    XP_Bool hidden;

    if( !context )
    {
        return;
    }
    
 	he = SHIST_GetCurrent( &context->hist );
    
	if( he )
    {
		StrAllocCopy( he->title, context->title );

        /*
         * This is the place where we set the title for *global* history hash records
         * In addition to not making grid cells normal history entries we've added two 
	 * more types, js_dependent windows and the Netcaster window.  Yes, it's a hack
	 * but we're short on time.
	 */
	hidden = context->is_grid_cell || context->js_parent || 
		(context->name && !XP_STRCMP("Netcaster_SelectorTab", context->name));
    	pUrl = NET_CreateURLStruct( he->address, NET_DONT_RELOAD );
        GH_UpdateURLTitle( pUrl, he->title, hidden);
        NET_FreeURLStruct( pUrl );
    }
}
#else
PUBLIC void
SHIST_SetTitleOfCurrentDoc(History * hist, char * title)
{
 	History_entry * he = SHIST_GetCurrent(hist);
    URL_Struct *pUrl;
    
	if( he )
    {
		StrAllocCopy(he->title, title);

        /*
         * This is the place where we set the title for *global* history hash records
         */
    	pUrl = NET_CreateURLStruct( he->address, NET_DONT_RELOAD );
        GH_UpdateURLTitle( pUrl, he->title, FALSE );
        NET_FreeURLStruct( pUrl );
    }
}
#endif

PUBLIC void
SHIST_SetCurrentDocFormListData(MWContext * context, void * form_data)
{
 	History_entry * he = SHIST_GetCurrent(&context->hist);

	if(he)
	  {
		he->savedData.FormList = form_data;
	  }
}

PUBLIC void
SHIST_SetCurrentDocEmbedListData(MWContext * context, void * embed_data)
{
 	History_entry * he = SHIST_GetCurrent(&context->hist);

	if(he)
	  {
		he->savedData.EmbedList = embed_data;
	  }
}

PUBLIC void
SHIST_SetCurrentDocGridData(MWContext * context, void * grid_data)
{
 	History_entry * he = SHIST_GetCurrent(&context->hist);

	if(he)
	  {
		he->savedData.Grid = grid_data;
	  }
}

PUBLIC void
SHIST_SetCurrentDocWindowData(MWContext * context, void *window_data)
{
 	History_entry * he = SHIST_GetCurrent(&context->hist);

	if(he)
	  {
		he->savedData.Window = window_data;
	  }
}

PUBLIC void
SHIST_SetPositionOfCurrentDoc(History * hist, int32 position_tag)
{
    History_entry * he = SHIST_GetCurrent(hist);    

    if(he)  
      { 
        he->position_tag = position_tag;
      }
}




/* ---
    Name: SHIST_GetNext
    Return:
         URL of next document or NULL on failure
    Parameters:
         ctxt - (modified) window context
    Description:
         The URL string should NOT be free'd by the user.  If they need
    it for any length of time they should make their own copy.  If there
    is no previous document return NULL.

         This function will set the "current" document pointer to the
    document whose URL was just retreived.

         There needs to be a way to get the correct location out too and
    pass it to whomever wants it. 

         Should probably call FE functions to fix dimming of next and
    previous buttons as needed.

--- */
PUBLIC History_entry *
SHIST_GetNext(MWContext * ctxt)
{
    /* check to make sure is a next record */
    if(!ctxt || ctxt->hist.num_entries <= ctxt->hist.cur_doc)
        return NULL;

    return((History_entry *) XP_ListGetObjectNum(ctxt->hist.list_ptr, ctxt->hist.cur_doc+1));

} /* SHIST_GetNext */

/* ---

    Return TRUE if there is a previous record in the history
    list we can go back to else FALSE

--- */
PUBLIC int
SHIST_CanGoBack(MWContext * ctxt)
{
#ifdef MOZ_NGLAYOUT
  return FALSE;
#else
    History *hist;

    hist = &ctxt->hist;

    /* check to make sure is a previous record */
    if(!hist ||  hist->cur_doc < 2)
    {
	if ((ctxt->grid_children != NULL)&&(LO_GridCanGoBackward(ctxt)))
	{
	    return TRUE;
	}
	return FALSE;
    }
    return TRUE;
#endif /* MOZ_NGLAYOUT */
}

/* ---

    Return TRUE if there is a next record in the history
    list we can go forward to else FALSE

--- */
PUBLIC int
SHIST_CanGoForward(MWContext * ctxt)
{
#ifdef MOZ_NGLAYOUT
  return FALSE;
#else
    History *hist;

    hist = &ctxt->hist;

    /* check to make sure is a next record */
    if(!hist || hist->num_entries <= hist->cur_doc)
    {
	if ((ctxt->grid_children != NULL)&&(LO_GridCanGoForward(ctxt)))
	{
	    return TRUE;
	}
	return FALSE;
    }

    return TRUE;
#endif /* MOZ_NGLAYOUT */
}


/* ---

  Return the URL for a document in the middle of the history list.
  entry_number is the zero based index of the entry to look up
  the current document pointers are updated appropriately
  returns NULL on failure or a string that the user should NOT
   free
 
--- */
PUBLIC History_entry * 
SHIST_GetEntry(History * hist, int entry_number)
{
    /* validate parameters */
    if(!hist)
        return NULL;

	/* return the current URL */
    return((History_entry *) XP_ListGetObjectNum(hist->list_ptr, entry_number+1));

} /* SHIST_GetEntry */

PUBLIC int
SHIST_GetIndex(History * hist, History_entry * entry)
{
    return(XP_ListGetNumFromObject(hist->list_ptr, entry));
}

PUBLIC History_entry *
SHIST_GetObjectNum(History * hist, int index)
{
    return((History_entry *)XP_ListGetObjectNum(hist->list_ptr, index));
}


/* sets the current doc pointer to the index specified in the call
 */
PUBLIC void
SHIST_SetCurrent(History * hist, int entry_number)
{
	hist->cur_doc = entry_number;
	hist->cur_doc_ptr = SHIST_GetObjectNum(hist, hist->cur_doc);
}



/* ---

    Return a list of the current history list

	DO NOT FREE the returned values

--- */
PUBLIC XP_List *
SHIST_GetList(MWContext * ctxt)
{

    /* error checking */
    if(!ctxt)
        return NULL;

	return(ctxt->hist.list_ptr);

} /* SHIST_GetList */


/* Returns TRUE if the current page handles Page Services
   and FALSE if it doesn't.
*/

PUBLIC int
SHIST_CurrentHandlesPageServices(MWContext * ctxt)
{
	History_entry * entry;

	if(ctxt){
		entry = SHIST_GetCurrent(&ctxt->hist);
		if(entry){
			return (entry->page_services_url ? TRUE : FALSE);
		}
	}

	return FALSE;

	

}

/* Returns the url of the page that contains the properties
   of the current page if Page Services is available.  It returns
   NULL if Page Services is not available*/
PUBLIC char *
SHIST_GetCurrentPageServicesURL(MWContext * ctxt)
{
	History_entry * entry;

	if(ctxt){
		entry = SHIST_GetCurrent(&ctxt->hist);
		if(entry){
			return entry->page_services_url;
		}
	}

	return NULL;

}

