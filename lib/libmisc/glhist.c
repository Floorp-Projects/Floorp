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
 */
#include "glhist.h"
#include "xp_hash.h"
#include "net.h"
#include "xp.h"
#include "mcom_db.h"
#include <time.h>
#include "merrors.h"
#include "xpgetstr.h"
#if !defined(XP_MAC)	/* macOS doesn't need this, but other platforms may still */
	#include "bkmks.h"
#endif
#include "prefapi.h"
#include "xplocale.h"
#include "libi18n.h"
#include "xp_qsort.h"

#if defined(XP_MAC)
	#include "extcache.h"
#endif

extern int XP_GLHIST_INFO_HTML;
extern int XP_GLHIST_DATABASE_CLOSED;
extern int XP_GLHIST_UNKNOWN;
extern int XP_GLHIST_DATABASE_EMPTY;
extern int XP_GLHIST_HTML_DATE    ;
extern int XP_GLHIST_HTML_TOTAL_ENTRIES;
extern int XP_HISTORY_SAVE;

/*PRIVATE XP_HashList * global_history_list = 0;*/
PRIVATE Bool          global_history_has_changed = FALSE;
PRIVATE time_t gh_cur_date = 0;
PRIVATE int32 global_history_timeout_interval = -1;

/* Autocomplete stuff */
PRIVATE Bool urlLookupGlobalHistHasChanged = FALSE;
PRIVATE int32 entriesToSearch = 100;
PRIVATE Bool enableUrlMatch=TRUE;


PRIVATE DB * gh_database = 0;
PRIVATE HASHINFO gh_hashinfo;

#ifdef XP_MAC
 /* The implementation for ppc/mac std lib does b - a for difftime.. weird  */
 #define difftime(a,b) ((double)((double)(a)-(double)(b)))
#endif

#define SYNC_RATE 30  /* number of stores before sync */

/*
*  Flags for individual records
*/
#define GH_FLAGS_SHOW         0x00000001
#define GH_FLAGS_FRAMECELL    0x00000002

static const char *pref_link_expiration = "browser.link_expiration";

/*------------------------------------------------------------------------------
//
// Global History context/cursor structure local types
//
------------------------------------------------------------------------------*/

/* Structure defining an array element for the Sort
*/
typedef struct _gh_HistList
{
    void *         pKeyData;
    void *         pData;
}gh_HistList;

/* Structure defining a list of elements
*/
typedef struct _gh_RecordList
{
    struct _gh_RecordList *  pNext;
    
    DBT  key;
    DBT  data;
    
} gh_RecordList;

/* Structure defining a node in an undo/redo list.
*/
typedef struct _gh_URList
{
    struct _gh_URList *  pNext;
    
    gh_RecordList *pURItem;
    
} gh_URList;

/* Structure defining the Global History undo/redo context.
*/
typedef struct _gh_URContext
{
    gh_URList *   pUndoList;
    gh_URList *   pRedoList;
    
} gh_URContext;

/* Structure defining the Global History context/cursor.
// This is the handle passed back and forth for all the Global History
// Context functions.
*/
typedef struct _gh_HistContext
{
    struct _gh_HistContext *   pNext;
    struct _gh_HistContext *   pPrev;    
    
    uint32                     uNumRecords;
    gh_Filter *                pFilter;
    gh_SortColumn              enGHSort;
    gh_HistList XP_HUGE **     pHistSort;
    
    GHISTORY_NOTIFYPROC        pfNotifyProc;
    
    gh_URContext *             pURContext;
    
    void *                     pUserData;
    
} gh_HistContext;

/* The list of all contexts in use. */
static gh_HistContext *pHistContextList = NULL;

#define GLHIST_COOKIE			"<!DOCTYPE NETSCAPE-history-file-1>"

#ifndef BYTE_ORDER
Error!  byte order must be defined
#endif

#if !defined(XP_MAC)
	#define COPY_INT32(_a,_b)  XP_MEMCPY(_a, _b, sizeof(int32));
#endif

PUBLIC void GH_CollectGarbage(void);

PRIVATE void            GH_CreateURContext( gh_HistContext *hGHContext );
PRIVATE void            GH_NotifyContexts( int32 iNotifyMsg, char *pszKey );
PRIVATE gh_RecordList * GH_CreateRecordNode( DBT *pKey, DBT *pData );
PRIVATE void            GH_PushRecord( gh_RecordList **ppRecordList, gh_RecordList *pRecordNode );
PRIVATE void            GH_DeleteRecordList( gh_RecordList *pRecordList );
PRIVATE char *          GH_GetTitleFromURL( char *pszURL );
PRIVATE void            GH_PushGroupUndo( gh_URContext *pURContext, gh_RecordList *pRecordNode );
PRIVATE gh_URList *     GH_CreateURNode( gh_RecordList *pRecordList );
PRIVATE void            GH_PushUR( gh_URList **ppURList, gh_URList *pURNode );
PRIVATE gh_URList *     GH_PopUR( gh_URList **ppURList );
PRIVATE void            GH_DeleteURList( gh_URList *pURList );

static int
gh_write_ok(const char* str, int length, XP_File fp)
{
  if (length < 0) length = XP_STRLEN(str);
  if ((int)XP_FileWrite(str, length, fp) < length) return -1;
  return 0;
}
#define WRITE(str, length, fp) \
   if (gh_write_ok((str), (length), (fp)) < 0) return -1

#if defined(XP_MAC) || defined(XP_UNIX)
/* set the maximum time for an object in the Global history in
 * number of seconds
 */
PUBLIC void
GH_SetGlobalHistoryTimeout(int32 timeout_interval)
{
    global_history_timeout_interval = timeout_interval;
}
#endif

PRIVATE void
gh_set_hash_options(void)
{
    gh_hashinfo.bsize = 4*1024;
    gh_hashinfo.nelem = 0;
    gh_hashinfo.hash = NULL;
    gh_hashinfo.ffactor = 0;
    gh_hashinfo.cachesize = 64 * 1024U;
    gh_hashinfo.lorder = 0;
}


PRIVATE void
gh_open_database(void)
{
#ifndef NO_DBM
	static Bool have_tried_open=FALSE;

	if(gh_database)
	  {
		return;
	  }
	else
	  {
		char* filename;
    	gh_set_hash_options();
		filename = WH_FileName("", xpGlobalHistory);
		gh_database = dbopen(filename,
							 O_RDWR | O_CREAT,
							 0600,
							 DB_HASH,
							 &gh_hashinfo);
		if (filename) XP_FREE(filename);

		if(!have_tried_open && !gh_database)
	      {
            XP_StatStruct stat_entry;

			have_tried_open = TRUE; /* only try this once */

			TRACEMSG(("Could not open gh database -- errno: %d", errno));


            /* if the file is zero length remove it
             */
            if(XP_Stat("", &stat_entry, xpGlobalHistory) != -1)
              {
                  if(stat_entry.st_size <= 0)
				    {
					  char* filename = WH_FileName("", xpGlobalHistory);
					  if (!filename) return;
					  XP_FileRemove(filename, xpGlobalHistory);
					  XP_FREE(filename);
				    }
				  else
				    {
						XP_File fp;
#define BUFF_SIZE 1024
						char buffer[BUFF_SIZE];

						/* open the file and look for
						 * the old magic cookie.  If it's
						 * there delete the file
						 */
						fp = XP_FileOpen("", xpGlobalHistory, XP_FILE_READ);

						if(fp)
						  {
							XP_FileReadLine(buffer, BUFF_SIZE, fp);
		
							XP_FileClose(fp);

							if(XP_STRSTR(buffer, "Global-history-file")) {
								char* filename = WH_FileName("", xpGlobalHistory);
								if (!filename) return;
					  		    XP_FileRemove(filename, xpGlobalHistory);
								XP_FREE(filename);
							}
						  }
				    }
              }

            /* try it again */
			filename = WH_FileName("", xpGlobalHistory);
			gh_database = dbopen(filename,
							 O_RDWR | O_CREAT,
							 0600,
							 DB_HASH,
							 &gh_hashinfo);
			if (filename) XP_FREE(filename);

			return;

	      }

        if(gh_database && -1 == (*gh_database->sync)(gh_database, 0))
          {
            TRACEMSG(("Error syncing gh database"));
			(*gh_database->close)(gh_database);
            gh_database = 0;
          }
	  }
#endif /* NO_DBM */

}

/* if the url was found in the global history then a number between
 * 0 and 99 is returned representing the percentage of time that
 * has elapsed in the expiration cycle.
 *  0  means most recently accessed
 * 99  means least recently accessed (about to be expired)
 *
 * If the url was not found -1 is returned
 *
 * define USE_PERCENTS if you want to get percent of time
 * through expires cycle.
 */

PUBLIC void
GH_DeleteHistoryItem (char * url) {
	DBT key;
	if(url && gh_database) {
	  key.data = (void *) url;
	  key.size = (XP_STRLEN(url)+1) * sizeof(char);
	  (*gh_database->del)(gh_database, &key, 0);
	  (*gh_database->sync)(gh_database, 0);
	}
}


PUBLIC int
GH_CheckGlobalHistory(char * url)
{
	DBT key;
    DBT data;
	int status;
	time_t entry_date;

	if(!url)
	    return(-1);

	if(!gh_database)
		return(-1);

	key.data = (void *) url;
	key.size = (XP_STRLEN(url)+1) * sizeof(char);

	status = (*gh_database->get)(gh_database, &key, &data, 0);

	if(status < 0)
	  {
		TRACEMSG(("Database ERROR retreiving global history entry"));
		return(-1);
	  }
	else if(status > 0)
	  {
		return(-1);
	  }

	/* otherwise */

	/* object was found.
	 * check the time to make sure it hasn't expired
	 */
	COPY_INT32( &entry_date, data.data );
    if(global_history_timeout_interval > 0
		&& entry_date+global_history_timeout_interval < gh_cur_date)
	  {
		/* remove the object
	 	 */
	    (*gh_database->del)(gh_database, &key, 0);

        /*
        // Notify the contexts of the update
        */
        GH_NotifyContexts( GH_NOTIFY_DELETE, (char *)key.data );
		
		/* return not found
		 */
		return(-1);
	  }

	return(1);
}

/* callback routine invoked by prefapi when the pref value changes */
/* fix Mac warning about missing prototype */
int PR_CALLBACK gh_link_expiration_changed(const char * newpref, void * data);

int PR_CALLBACK gh_link_expiration_changed(const char * newpref, void * data)
{
	int32 iExp;
		
	/* Get the number of days for link expiration */
	PREF_GetIntPref(pref_link_expiration, &iExp);

	/* Convert to seconds */
	global_history_timeout_interval = iExp * 60 * 60 * 24;

	if (iExp == 0)
		GH_ClearGlobalHistory();
	
	return PREF_NOERROR;
}

/* start global history tracking
 */
PUBLIC void
GH_InitGlobalHistory(void)
{
#if defined(XP_WIN) || defined(XP_OS2)
    int32 iExp;
	
	/* Get the number of days for link expiration */
	PREF_GetIntPref(pref_link_expiration, &iExp);

    /* Convert to seconds */
	global_history_timeout_interval = iExp * 60 * 60 * 24;

	/* Observe the preference */
	PREF_RegisterCallback(pref_link_expiration, gh_link_expiration_changed, NULL);
#endif
	
	gh_open_database();
}

PRIVATE void
gh_RemoveDatabase(void)
{
	char* filename;
	if(gh_database)
	  {
		(*gh_database->close)(gh_database);
		gh_database = 0;
	  }
	filename = WH_FileName("", xpGlobalHistory);
	if (!filename) return;
	XP_FileRemove(filename, xpGlobalHistory);
	XP_FREE(filename);
}

/* Notify all the contexts of something e.g., updates, deletions
*/
PRIVATE void
GH_NotifyContexts( int32 iNotifyMsg, char *pszKey )
{
   gh_HistContext *  pCsr  = pHistContextList;
   gh_HistContext *  pCopy = NULL;
   
   int32 iCount = 0;
   int32 i = 0;

   gh_NotifyMsg stNM;
   XP_MEMSET( &stNM, 0, sizeof(gh_NotifyMsg) );
   stNM.iNotifyMsg = iNotifyMsg;
   stNM.pszKey     = pszKey;
   
   if( !pCsr )
   {
      return;
   }

   /* Note we must first make a copy of the context information before we notify.
      The reason: If the notifyee decides to release his context during the notification,
      the list is compromised i.e., GH_ReleaseContext() changes the list.
   */
   
   /* Count the contexts first */
   do
   {
      iCount++;
      pCsr = pCsr->pNext;
      
   }while( pCsr != pHistContextList );

   /* Allocate the mem for the copy */   
   pCopy = (gh_HistContext *)XP_ALLOC( iCount * sizeof(gh_HistContext) );
   XP_MEMSET( pCopy, 0, iCount * sizeof(gh_HistContext) );

   /* Fill in the context copy */
   pCsr = pHistContextList;
   do
   {
      pCopy[i].pUserData = pCsr->pUserData;
      pCopy[i].pfNotifyProc = pCsr->pfNotifyProc;
      i++;
      
      pCsr = pCsr->pNext;
      
   }while( pCsr != pHistContextList );

   /* Now notify the contexts */
   for( i = 0; i < iCount; i++ )
   {
      if( !pCopy[i].pfNotifyProc )
      {
         continue;
      }
      
      stNM.pUserData = pCopy[i].pUserData;
      
      pCopy[i].pfNotifyProc( &stNM );
   }
   
   XP_FREE( pCopy );
}

PRBool blockedHistItem (char* url) ;
/* add or update the url in the global history
 */
PUBLIC void
GH_UpdateGlobalHistory(URL_Struct * URL_s)
{
	char *url=NULL, *atSign=NULL, *passwordColon=NULL, *afterProtocol=NULL;
	DBT key, data, dataComp;
	int status;
	static int32 count=0;
    int8 *pData;
    
    int32 iNameLen = 0;

    int32 iDefault = 1;
    int32 iCount;
    
	/* check for NULL's
	 * and also don't allow ones with post-data in here
 	 */
	if(!URL_s || !URL_s->address || URL_s->post_data)
	    return;

	/* Never save these in the history database */
	if (!strncasecomp(URL_s->address, "about:", 6) ||
		!strncasecomp(URL_s->address, "javascript:", 11) ||
		!strncasecomp(URL_s->address, "livescript:", 11) ||
		!strncasecomp(URL_s->address, "mailbox:", 8) ||
		!strncasecomp(URL_s->address, "mailto:", 7) ||
		!strncasecomp(URL_s->address, "mocha:", 6) ||
		!strncasecomp(URL_s->address, "news:", 5) ||
		!strncasecomp(URL_s->address, "pop3:", 5) ||
		!strncasecomp(URL_s->address, "snews:", 6) ||
		!strncasecomp(URL_s->address, "view-source:", 12))
	  return;

	gh_cur_date = time(NULL);

	/*	BM_UpdateBookmarksTime(URL_s, gh_cur_date); */

	if(global_history_timeout_interval == 0)
	    return;  /* don't add ever */

	gh_open_database();

	if(!gh_database)
		return;

	global_history_has_changed = TRUE;
	urlLookupGlobalHistHasChanged = TRUE;

	count++;  /* increment change count */

	/* Don't allow passwords through. If there's an at sign, check for a password. */
	if( (atSign = XP_STRCHR(URL_s->address, '@')) != NULL )
	{
		*atSign = '\0';

		/* get a position beyond the protocol */
		afterProtocol = XP_STRCHR(URL_s->address, ':');
		if(afterProtocol
			&& (afterProtocol[1] == '/')
			&& (afterProtocol[2] == '/')) {
			afterProtocol += 3;
		}
		else {
			*atSign = '@';
			return; /* url is in bad format */
		}
		
		if( (passwordColon = XP_STRCHR(afterProtocol, ':')) != NULL)
		{
			/* Copy everything up to the password colon */
			*passwordColon = '\0';
			StrAllocCopy(url, URL_s->address);

			/* Put the stripped chars back */
			*passwordColon = ':';
			*atSign = '@';

			if(!url)
				return;
			/* Concatenate everyting from the at sign on, skipping the password */
			StrAllocCat(url, atSign);
			if(!url)
				return;
			key.data = (void *) url;
			key.size = (XP_STRLEN(url)+1) * sizeof(char);
		}
		/* There was no password, just a username perhaps */
		else {
			*atSign = '@';
			key.data = (void *) URL_s->address;
			key.size = (XP_STRLEN(URL_s->address)+1) * sizeof(char);
		}
	}
	/* No at sign, no chance of a password. Business as usual */
	else {
		key.data = (void *) URL_s->address;
		key.size = (XP_STRLEN(URL_s->address)+1) * sizeof(char);
	}

#if 0 /* Old Format */
	COPY_INT32(&date, &gh_cur_date);
	data.data = (void *)&date;
	data.size = sizeof(int32);
#else
    iNameLen = (URL_s->content_name && *URL_s->content_name) ? XP_STRLEN( URL_s->content_name )+1 : 1;
    data.size = sizeof(int32) + sizeof(int32) + sizeof(int32) + sizeof(int32) + iNameLen*sizeof(char);
    
    pData = XP_ALLOC( data.size );

    data.data = (void *)pData;
    
    /*
    // last_accessed...
    */
    COPY_INT32( pData, &gh_cur_date );
    
    /*
    // iFlags
    */
    XP_MEMSET( pData+3*sizeof(int32), 0, sizeof(int32) );
        
    /*
    // pszName...
    //
    // Note the content_name member is rarely if ever used, so the title is always blank
    */
    if( iNameLen > 1  )
    {
       XP_STRCPY( (char *)pData+4*sizeof(int32), URL_s->content_name );
    }
    else
    {
        *(char *)(pData+4*sizeof(int32)) = 0;
    }
   
	if( 0 == (*gh_database->get)( gh_database, &key, &dataComp, 0 ) )
    {
        if( dataComp.size > sizeof(int32) )
        {
            /* New format...
            
            //
            // first_accessed
            */
            
            COPY_INT32( pData+sizeof(int32), (int8 *)dataComp.data + sizeof(int32) );
            
            /*
            // iCount
            */
            
            COPY_INT32(&iCount, (int8 *)dataComp.data + 2*sizeof(int32));
			iCount++;
			COPY_INT32( pData + 2*sizeof(int32), &iCount );
        }
        else
        {
            /* Old format...
            
            //
            // first_accessed
            */
            
            COPY_INT32( pData+sizeof(int32), dataComp.data );

            /*
            // iCount
            */
            
            COPY_INT32( pData+2*sizeof(int32), &iDefault );
        }
    }
    else
    {
        /* New record...
        
        //
        // first_accessed
        */
    
        COPY_INT32( pData+sizeof(int32), &gh_cur_date );

        /*
        // iCount
        */
        COPY_INT32( pData+2*sizeof(int32), &iDefault );        
    }

#endif   
	status = (*gh_database->put)( gh_database, &key, &data, 0 );


    XP_FREE( pData );
	XP_FREEIF(url);
    
	if(status < 0)
    {
        TRACEMSG(("Global history update failed due to database error"));
        gh_RemoveDatabase();
    }
    else if(count >= SYNC_RATE)
    {
        count = 0;
        if( -1 == (*gh_database->sync)( gh_database, 0 ) )
        {
            TRACEMSG(("Error syncing gh database"));
        }
    }
#if 0 /* Not a good idea right now - with a large hash this could cause some lag.
         Instead, do it during GH_UpdateURLTitle() since these are what we're interested in anyway */
    /*
    // Notify the contexts of the update
    */
    GH_NotifyContexts( GH_NOTIFY_UPDATE, (char *)key.data );
#endif
}


#define MAX_HIST_DBT_SIZE 1024

PRIVATE DBT *
gh_HistDBTDup(DBT *obj)
{
    DBT * rv = XP_NEW(DBT);

	if(!rv || obj->size > MAX_HIST_DBT_SIZE)
		return NULL;

    rv->size = obj->size;
    rv->data = XP_ALLOC(rv->size);
	if(!rv->data)
	  {
		XP_FREE(rv);
		return NULL;
	  }

    XP_MEMCPY(rv->data, obj->data, rv->size);

    return(rv);

}

PRIVATE void
gh_FreeHistDBTdata(DBT *stuff)
{
    XP_FREE(stuff->data);
    XP_FREE(stuff);
}


/* runs through a portion of the global history
 * database and removes all objects that have expired
 *
 */
PUBLIC void
GH_CollectGarbage(void)
{
#define OLD_ENTRY_ARRAY_SIZE 100
    DBT *old_entry_array[OLD_ENTRY_ARRAY_SIZE];

	DBT key, data;	
	DBT *newkey;
	time_t entry_date;
	int i, old_entry_count=0;
	
	if(!gh_database || global_history_timeout_interval < 1)
		return;

	gh_cur_date = time(NULL);

	if(0 != (*gh_database->seq)(gh_database, &key, &data, R_FIRST))
		return;
	
	global_history_has_changed = TRUE;
	urlLookupGlobalHistHasChanged = TRUE;

	do
	  {
    	COPY_INT32(&entry_date, data.data);
    	if(global_history_timeout_interval > 0
           && entry_date+global_history_timeout_interval < gh_cur_date)
      	  {
        	/* put the object on the delete list since it is expired
         	 */
			if(old_entry_count < OLD_ENTRY_ARRAY_SIZE)
				old_entry_array[old_entry_count++] = gh_HistDBTDup(&key);
			else
				break;
      	  }
	  }
	while(0 == (gh_database->seq)(gh_database, &key, &data, R_NEXT));

	for(i=0; i < old_entry_count; i++)
	  {
		newkey = old_entry_array[i];
		if(newkey)
		  {
       	    (*gh_database->del)(gh_database, newkey, 0);
            
            /*
            // Notify the contexts of the update
            */
            GH_NotifyContexts( GH_NOTIFY_DELETE, (char *)newkey->data );
    		
		    gh_FreeHistDBTdata(newkey);
		  }
	  }
}

/* save the global history to a file while leaving the object in memory
 */
PUBLIC void
GH_SaveGlobalHistory(void)
{

	if(!gh_database)
		return;

	GH_CollectGarbage();

	if(global_history_has_changed)
      {
	    if(-1 == (*gh_database->sync)(gh_database, 0))
	      {
		    TRACEMSG(("Error syncing gh database"));
			(*gh_database->close)(gh_database);
			gh_database = 0;
	      }
	    global_history_has_changed = FALSE;
		urlLookupGlobalHistHasChanged = TRUE;
	}
}

/* free the global history list
 */
PUBLIC void
GH_FreeGlobalHistory(void)
{
	if(!gh_database)
		return;

	if(-1 == (*gh_database->close)(gh_database))
	  {
		 TRACEMSG(("Error closing gh database"));
	  }

	gh_database = 0;
	
}

/* clear the global history list
 */
PUBLIC void
GH_ClearGlobalHistory(void)
{
	char* filename;
	if(!gh_database)
		return;


	GH_FreeGlobalHistory();

    gh_set_hash_options();

#ifndef NO_DBM
	filename = WH_FileName("", xpGlobalHistory);
	gh_database = dbopen(filename,
						 O_RDWR | O_TRUNC,
						 0600,
						 DB_HASH,
						 &gh_hashinfo);
	if (filename) XP_FREE(filename);
#endif /* NO_DBM */
    if(gh_database && -1 == (*gh_database->sync)(gh_database, 0))
      {
        TRACEMSG(("Error syncing gh database"));
		(*gh_database->close)(gh_database);
        gh_database = 0;
      }

	global_history_has_changed = FALSE;
	urlLookupGlobalHistHasChanged = TRUE;
}


/* create an HTML stream and push a bunch of HTML about
 * the global history
 */
MODULE_PRIVATE int
NET_DisplayGlobalHistoryInfoAsHTML(MWContext *context,
								   URL_Struct *URL_s,
								   int format_out)
{
	char *buffer = (char*)XP_ALLOC(256);
   	NET_StreamClass * stream;
	DBT key, data;
	Bool long_form = FALSE;
	time_t entry_date;
	int status = MK_NO_DATA;
	int32 count=0;
	static char LINK_START[] = "<A href=\"";
	static char LINK_END[] = "\">";
	static char END_LINK[] = "</A>";

	if(!buffer)
	  {
		return(MK_UNABLE_TO_CONVERT);
	  }

	if(strcasestr(URL_s->address, "?long"))
		long_form = TRUE;

	StrAllocCopy(URL_s->content_type, TEXT_HTML);

	format_out = CLEAR_CACHE_BIT(format_out);
	stream = NET_StreamBuilder(format_out,
							   URL_s,
							   context);

	if(!stream)
	  {
		return(MK_UNABLE_TO_CONVERT);
	  }


	/* define a macro to push a string up the stream
	 * and handle errors
	 */
#define PUT_PART(part)													\
status = (*stream->put_block)(stream,			\
										part ? part : XP_GetString(XP_GLHIST_UNKNOWN),	 \
										part ? XP_STRLEN(part) : 7);	\
if(status < 0)												\
  goto END;

	XP_SPRINTF(buffer, XP_GetString(XP_GLHIST_INFO_HTML));

	PUT_PART(buffer);

	if(!gh_database)
	  {
		XP_STRCPY(buffer, XP_GetString(XP_GLHIST_DATABASE_CLOSED));
		PUT_PART(buffer);
		goto END;
	  }

    if(0 != (*gh_database->seq)(gh_database, &key, &data, R_FIRST))
	  {
		XP_STRCPY(buffer, XP_GetString(XP_GLHIST_DATABASE_EMPTY));
		PUT_PART(buffer);
		goto END;
	  }

	/* define some macros to help us output HTML tables
	 */
#define TABLE_TOP(arg1)				\
	XP_SPRINTF(buffer, 				\
"<TR><TD ALIGN=RIGHT><b>%s</TD>\n"	\
"<TD>", arg1);						\
PUT_PART(buffer);

#define TABLE_BOTTOM				\
	XP_SPRINTF(buffer, 				\
"</TD></TR>");						\
PUT_PART(buffer);

    do
      {
		count++;

		COPY_INT32(&entry_date, data.data);

		/* print url */
		XP_STRCPY(buffer, "<TT>&nbsp;URL:</TT> ");
		PUT_PART(buffer);

		/* make the URL a link */
		PUT_PART(LINK_START);
		if(status < 0)
  			goto END;

		/* push the key special since we know the size */
		status = (*stream->put_block)(stream,
									  (char*)key.data, key.size);
		if(status < 0)
  			goto END;

		PUT_PART(LINK_END);
		if(status < 0)
  			goto END;

		/* push the key special since we know the size */
		status = (*stream->put_block)(stream,
									  (char*)key.data, key.size);
		if(status < 0)
  			goto END;

		PUT_PART(END_LINK);
		if(status < 0)
  			goto END;

		XP_SPRINTF(buffer, XP_GetString(XP_GLHIST_HTML_DATE), ctime(&entry_date));
		PUT_PART(buffer);

      }
    while(0 == (*gh_database->seq)(gh_database, &key, &data, R_NEXT));

	
	XP_SPRINTF(buffer, XP_GetString(XP_GLHIST_HTML_TOTAL_ENTRIES), count);
	PUT_PART(buffer);

END:
	XP_FREE(buffer);
	
	if(status < 0)
		(*stream->abort)(stream, status);
	else
		(*stream->complete)(stream);
	XP_FREE(stream);

	return(status);
}

/*------------------------------------------------------------------------------
//
*/
PRIVATE void
GH_CreateURContext( gh_HistContext *hGHContext )
{
    if( !hGHContext || hGHContext->pURContext )
    {
        return;
    }
    
    hGHContext->pURContext = XP_ALLOC( sizeof(gh_URContext) );
    
    hGHContext->pURContext->pUndoList = hGHContext->pURContext->pRedoList = NULL;
}

/*------------------------------------------------------------------------------
//
*/
PRIVATE void
GH_ReleaseURContext( gh_URContext *pURContext )
{
    if( !pURContext )
    {
        return;
    }
    
    if( pURContext->pUndoList )
    {
       GH_DeleteURList( pURContext->pUndoList );
    }

    if( pURContext->pRedoList )
    {
       GH_DeleteURList( pURContext->pRedoList );
    }
    
    XP_FREE( pURContext );
}

/*------------------------------------------------------------------------------
// 
// QSort compare callbacks
*/
static int QSortCompStr( const void *elem1, const void *elem2 )
{
    gh_HistList ** p1 = (gh_HistList **)elem1;
    gh_HistList ** p2 = (gh_HistList **)elem2;
    return XP_StrColl( (char *)(*p1)->pData, (char *)(*p2)->pData );
}
static int QSortCompStr2( const void *elem1, const void *elem2 )
{
    gh_HistList ** p1 = (gh_HistList **)elem1;
    gh_HistList ** p2 = (gh_HistList **)elem2;
    return XP_StrColl( (char *)(*p1)->pKeyData, (char *)(*p2)->pKeyData );
}

#ifdef SUNOS4
/* difftime() doesn't seem to exist on SunOS anywhere. -mcafee */
static double difftime(time_t time1, time_t time0)
{
  return (double) (time1 - time0);
}
#endif

static int QSortCompDate( const void *elem1, const void *elem2 )
{
    gh_HistList ** p1 = (gh_HistList **)elem1;
    gh_HistList ** p2 = (gh_HistList **)elem2;
    return difftime( *(time_t *)(*p2)->pData, *(time_t *)(*p1)->pData ) >= 0 ? 1 : -1;
}
static int QSortCompInt32( const void *elem1, const void *elem2 )
{
    gh_HistList ** p1 = (gh_HistList **)elem1;
    gh_HistList ** p2 = (gh_HistList **)elem2;
    return (*(int32 *)(*p1)->pData >= *(int32 *)(*p2)->pData) ? 1 : -1;
}

/*------------------------------------------------------------------------------
// 
// Supplies a "context", or handle, to the hash table.  The context serves as a 
// quasi-cursor to the table, offering the ability to navigate and enumerate the 
// records sorting on a specified column/field.
*/
#define SORT_ARRAY_GROW_SIZE 1024
PUBLIC GHHANDLE
GH_GetContext( enum                 gh_SortColumn enGHSort, 
               gh_Filter *          pFilter, 
               GHISTORY_NOTIFYPROC  pfNotifyProc,
               GHURHANDLE           hUR,
               void *               pUserData )
{
	DBT key, data;
    void XP_HUGE *pBase = NULL;
#ifdef XP_WIN16
    void XP_HUGE *pBuf = NULL;    
#endif
    int16 csid = INTL_DefaultWinCharSetID( 0 );
    
    gh_HistList *pNode;
	gh_HistContext *hGHContext = XP_ALLOC( sizeof(gh_HistContext) );
    XP_MEMSET( hGHContext, 0, sizeof(gh_HistContext) );
    
	if( !gh_database || global_history_timeout_interval < 1 )
    {
		return NULL;
    }

    hGHContext->uNumRecords    = 0;
    hGHContext->enGHSort       = enGHSort;
    hGHContext->pFilter        = pFilter;
    hGHContext->pfNotifyProc   = pfNotifyProc;
    hGHContext->pUserData      = pUserData;
    hGHContext->pURContext     = (gh_URContext *)hUR;
    
    /* Append the context to the list */
    if( !pHistContextList )
    {
       pHistContextList = hGHContext;
       pHistContextList->pNext = pHistContextList->pPrev = pHistContextList;
    }
    else
    {
       hGHContext->pPrev = pHistContextList->pPrev;
       hGHContext->pNext = pHistContextList;
       pHistContextList->pPrev->pNext = hGHContext;
       pHistContextList->pPrev = hGHContext;
    }
    
    data.size = key.size = 0;
    
    /*
    // Build the array.  We don't know the number of entries in the hash, so we
    // have to grow the array as we read the entries.  Gross.
    */
	if( 0 != (*gh_database->seq)( gh_database, &key, &data, R_FIRST ) )
    {
		return hGHContext;
	}
    
    pBase = (void XP_HUGE *)XP_HUGE_ALLOC( SORT_ARRAY_GROW_SIZE*sizeof(gh_HistList *) );
	do
	{
        if( data.size > sizeof(int32) )
        {
            /*
            // The entry/record is of the new format...        
            //
            
            // Ignore history records which are NOT flagged as having been explicitly loaded
            // e.g., don't expose gif images that are only part of a page
            */
            /*int32 iFlags = *(int32 *)((int8 *)data.data + 3*sizeof(int32));*/
            int32 iFlags;
            COPY_INT32( &iFlags, (int8 *)data.data + 3*sizeof(int32) );

            if( !(iFlags & GH_FLAGS_SHOW) )
            {
               continue;
            }
            
            /*
            // Filter records according to the supplied filter struct.
            */
            
            if( pFilter )
            {
                Bool bKeep = FALSE;
                int i;
                
                for( i = 0; i < pFilter->iNumConditions; i++ )
                {
                   if( i > 0 )
                   {
                      if( pFilter->enOps[i-1] == eGH_FLOAnd )
                      {
                         if( !bKeep )
                         {
                            /* No need to evaluate logical-and expression if already false */
                            continue;
                         }
                      }
                      else /* eGH_FLOOr */
                      {
                         if( bKeep )
                         {
                            /* No need to evaluate logical-or expression if already true */
                            continue;
                         }
                      }
                   }
                   
                   /* Guilty until proven innocent */
                   bKeep = FALSE;
                   
                   switch( pFilter->pConditions[i].enCol )
                   {
                      case eGH_LocationSort:
                      {
                         int iRes = XP_StrColl( pFilter->pConditions[i].tests.pszTest, key.data );
                      
                         switch( pFilter->pConditions[i].enOp )
                         {
                             case eGH_FOEquals:
                             {
                                if( !iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOEqualsNot:
                             {
                                if( iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreater:
                             {
                                if( iRes > 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreaterEqual:
                             {
                                if( iRes >= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLess:
                             {
                                if( iRes < 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLessEqual:
                             {
                                if( iRes <= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOHas:
                             {
                                bKeep = INTL_Strcasestr( csid, key.data, pFilter->pConditions[i].tests.pszTest ) != NULL;
                                break;
                             }
                             case eGH_FOHasNot:
                             {
                                bKeep = INTL_Strcasestr( csid, key.data, pFilter->pConditions[i].tests.pszTest ) == NULL;
                                break;
                             }
                             default:
                                bKeep = FALSE;
                         }
                         break;
                      }
                      
                      case eGH_NameSort:
                      {
                         char *pText = (char *)((int8 *)data.data + 4*sizeof(int32));
                         
                         int iRes = XP_StrColl( pFilter->pConditions[i].tests.pszTest, pText );
                      
                         switch( pFilter->pConditions[i].enOp )
                         {
                             case eGH_FOEquals:
                             {
                                if( !iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOEqualsNot:
                             {
                                if( iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreater:
                             {
                                if( iRes > 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreaterEqual:
                             {
                                if( iRes >= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLess:
                             {
                                if( iRes < 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLessEqual:
                             {
                                if( iRes <= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOHas:
                             {
                                bKeep = INTL_Strcasestr( csid, pText, pFilter->pConditions[i].tests.pszTest ) != NULL;
                                break;
                             }
                             case eGH_FOHasNot:
                             {
                                bKeep = INTL_Strcasestr( csid, pText, pFilter->pConditions[i].tests.pszTest ) == NULL;
                                break;
                             }
                             default:
                                bKeep = FALSE;
                         }
                         break;
                      }

                      case eGH_VisitCountSort:           
                      {
                         int32 iCount; 
                         COPY_INT32( &iCount, (int8 *)data.data + 2*sizeof(int32) );                         
                      
                         switch( pFilter->pConditions[i].enOp )
                         {
                             case eGH_FOEquals:
                             {
                                if( iCount == pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOEqualsNot:
                             {
                                if( iCount != pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreater:
                             {
                                if( iCount > pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreaterEqual:
                             {
                                if( iCount >= pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLess:
                             {
                                if( iCount < pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLessEqual:
                             {
                                if( iCount <= pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             default:
                                bKeep = FALSE;
                         }
                         break;
                      }

                      case eGH_FirstDateSort:           
                      case eGH_LastDateSort:                                 
                      {
                         /*
                         // Assuming we are comparing ONLY the date, set the time to 00:00:00 for both time values
                         */
                         
                         time_t time0, time1 = pFilter->pConditions[i].tests.iTest;
                         struct tm *pTM;
                         struct tm tm0;
                         struct tm tm1;                         
                                               
                         if( pFilter->pConditions[i].enCol == eGH_FirstDateSort )
                         {
                            COPY_INT32( &time0, (int8 *)data.data + sizeof(int32) );                         
                         }
                         else
                         {
                            COPY_INT32( &time0, (int8 *)data.data );                         
                         }
                         
                         pTM = localtime( &time0 );
                         XP_MEMSET( &tm0, 0, sizeof(struct tm) );
                         tm0.tm_mday = pTM->tm_mday;
                         tm0.tm_mon  = pTM->tm_mon;
                         tm0.tm_year = pTM->tm_year;
                         pTM = localtime( &time1 );
                         XP_MEMSET( &tm1, 0, sizeof(struct tm) );
                         tm1.tm_mday = pTM->tm_mday;
                         tm1.tm_mon  = pTM->tm_mon;
                         tm1.tm_year = pTM->tm_year;
                         
                         time0 = mktime( &tm0 );
                         time1 = mktime( &tm1 );
                         
                         switch( pFilter->pConditions[i].enOp )
                         {
                             case eGH_FOEquals:
                             {
                                if( time0 == time1 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOEqualsNot:
                             {
                                if( time0 != time1 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreater:
                             {
                                if( time0 > time1 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreaterEqual:
                             {
                                if( time0 >= time1 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLess:
                             {
                                if( time0 < time1 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLessEqual:
                             {
                                if( time0 <= time1 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             default:
                                bKeep = FALSE;
                         }
                         break;
                      }
					 default:
					   break;
                   }
                }
                
                if( !bKeep )
                {
                   /*
                   // Did NOT pass filter.  Test next record.
                   */
                   continue;
                }
            }
        }
        else
        {   
            /*
            // The entry/record is of the old format...
            //
            
            // Try to be somewhat smart and filter the implicitly loaded stuff i.e., non-html docs
            */
            char *pszExt = strrchr( key.data, '.' );
            
            if( ((char *)key.data)[XP_STRLEN(key.data)-1] == '/' )
            {
            }
            else if( XP_STRLEN(key.data) < 5 )
            {
               continue;
            }
            else if( pszExt )
            {
               pszExt++;
               if( strncasecomp( pszExt, "htm", 3 ) )
               {
                  continue;
               }
            }
            else
            {
               continue;
            }

            /*
            // Filter records according to the supplied filter struct
            */

            if( pFilter )
            {
                Bool bKeep = FALSE;
                int i;
                
                for( i = 0; i < pFilter->iNumConditions; i++ )
                {
                
                   if( i > 0 )
                   {
                      if( pFilter->enOps[i-1] == eGH_FLOAnd )
                      {
                         if( !bKeep )
                         {
                            /* No need to evaluate logical-and expression if already false */
                            continue;
                         }
                      }
                      else /* eGH_FLOOr */
                      {
                         if( bKeep )
                         {
                            /* No need to evaluate logical-or expression if already true */
                            continue;
                         }
                      }
                   }
                   
                   /* Guilty until proven innocent */
                   bKeep = FALSE;
                
                   switch( pFilter->pConditions[i].enCol )
                   {
                      case eGH_LocationSort:
                      {
                         int iRes = XP_StrColl( pFilter->pConditions[i].tests.pszTest, key.data );
                      
                         switch( pFilter->pConditions[i].enOp )
                         {
                             case eGH_FOEquals:
                             {
                                if( !iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOEqualsNot:
                             {
                                if( iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreater:
                             {
                                if( iRes > 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreaterEqual:
                             {
                                if( iRes >= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLess:
                             {
                                if( iRes < 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLessEqual:
                             {
                                if( iRes <= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOHas:
                             {
                                bKeep = INTL_Strcasestr( csid, key.data, pFilter->pConditions[i].tests.pszTest ) != NULL;
                                break;
                             }
                             case eGH_FOHasNot:
                             {
                                bKeep = INTL_Strcasestr( csid, key.data, pFilter->pConditions[i].tests.pszTest ) == NULL;
                                break;
                             }
                             default:
                                bKeep = FALSE;
                         }
                         break;
                      }
                      
                      case eGH_NameSort:
                      {
                         /*
                         // Since there is no title available from the old format, let's try and pull
                         // a meaningful title from the URL.
                         */
                      
                         char *pszTitle = GH_GetTitleFromURL( key.data );
                         int iRes = XP_StrColl( pFilter->pConditions[i].tests.pszTest, pszTitle );
                      
                         switch( pFilter->pConditions[i].enOp )
                         {
                             case eGH_FOEquals:
                             {
                                if( !iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOEqualsNot:
                             {
                                if( iRes )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreater:
                             {
                                if( iRes > 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreaterEqual:
                             {
                                if( iRes >= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLess:
                             {
                                if( iRes < 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLessEqual:
                             {
                                if( iRes <= 0 )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOHas:
                             {
                                bKeep = INTL_Strcasestr( csid, pszTitle, pFilter->pConditions[i].tests.pszTest ) != NULL;
                                break;
                             }
                             case eGH_FOHasNot:
                             {
                                bKeep = INTL_Strcasestr( csid, pszTitle, pFilter->pConditions[i].tests.pszTest ) == NULL;
                                break;
                             }
                             default:
                                bKeep = FALSE;
                         }
                         
                         break;
                      }
                      
                      case eGH_FirstDateSort:           
                      case eGH_LastDateSort:                                 
                      {
                         time_t time;
                                               
                         COPY_INT32( &time, (int8 *)data.data );                         
                         
                         switch( pFilter->pConditions[i].enOp )
                         {
                             case eGH_FOEquals:
                             {
                                if( time == pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOEqualsNot:
                             {
                                if( time != pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreater:
                             {
                                if( time > pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOGreaterEqual:
                             {
                                if( time >= pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLess:
                             {
                                if( time < pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             case eGH_FOLessEqual:
                             {
                                if( time <= pFilter->pConditions[i].tests.iTest )
                                {
                                   bKeep = TRUE;
                                }
                                break;                                
                             }
                             default:
                                bKeep = FALSE;
                         }
                         break;
                      }
					 default:
					   break;
                   }
                }
                
                if( !bKeep )
                {
                   /* Did NOT pass filter.  Test next record. */
                   continue;
                }
            }                        
        }
        
        pNode = XP_ALLOC( sizeof(gh_HistList) );
        pNode->pKeyData = XP_ALLOC( (XP_STRLEN(key.data)+1) * sizeof(char) );
        XP_STRCPY( pNode->pKeyData, key.data );
        
        switch( enGHSort )
        {
           case eGH_LocationSort:
           {
              pNode->pData = pNode->pKeyData;
              break;
           }
           
           case eGH_NameSort:
           {
              if( data.size > sizeof(int32) )
              {
                 /* The entry/record is of the new format i.e., record has title data */

                 pNode->pData = XP_ALLOC( (XP_STRLEN((char *)data.data + 4*sizeof(int32))+1) * sizeof(char) );
                 XP_STRCPY( pNode->pData, (char *)data.data + 4*sizeof(int32) );
              }
              else
              {
                #if 0
                 /*
                 // The entry/record is of the old format i.e., no title data available
                 // Note this is only for internal sorting to keep unititled records at bottom of A->Z sort
                 */
                 pNode->pData = XP_ALLOC( (XP_STRLEN("~~")+1) * sizeof(char) );
                 XP_STRCPY( pNode->pData, "~~" );                 
                #else
                 /*
                 // Since there is no title available from the old format, let's try and pull
                 // a meaningful title from the URL.
                 */
                 char *pszTitle = GH_GetTitleFromURL( key.data );
                 pNode->pData = XP_ALLOC( (XP_STRLEN( pszTitle )+1) * sizeof(char) );
                 XP_STRCPY( pNode->pData, pszTitle );
                 pszTitle = strrchr( pNode->pData, '/' );
                 if( pszTitle )
                 {
				   /* Remove the trailing slash from the title. */
				   *pszTitle = 0;
                 }
                #endif
              }
              break;
           }

           case eGH_VisitCountSort:           
           {
              if( data.size > sizeof(int32) )
              {
                 /* The entry/record is of the new format i.e., record has a visit count */

                 pNode->pData = XP_ALLOC( sizeof(int32) );
                 COPY_INT32( pNode->pData, (int8 *)data.data + 2*sizeof(int32) );
              }
              else
              {
                 /* The entry/record is of the old format i.e., no visit count available */
                 
                 pNode->pData = XP_ALLOC( sizeof(int32) );
                 *(int32 *)pNode->pData = 1;
              }
              break;
           }

           case eGH_FirstDateSort:           
           {
              if( data.size > sizeof(int32) )
              {
                 /* The entry/record is of the new format */

                 time_t date;
                 COPY_INT32( &date, data.data );
                 
                 pNode->pData = XP_ALLOC( sizeof(int32) );
                 
                 COPY_INT32( pNode->pData, (int8 *)data.data + sizeof(int32) );
                 
                 break;
              }
              
              /* Fall through if old format (i.e., sort by last_accessed) */
           }
           
           case eGH_LastDateSort:           
           default:           
           {
              /* Note the last_accessed field is at the beginning of the data regardless of new/old format */
              
              time_t date;
              COPY_INT32( &date, data.data );
              
              pNode->pData = XP_ALLOC( sizeof(int32) );
              
              COPY_INT32( pNode->pData, (int8 *)data.data );
              
              break;
           }
        }
        
        ((gh_HistList XP_HUGE **)pBase)[hGHContext->uNumRecords] = pNode;
        
        hGHContext->uNumRecords++;
        
        if( !(hGHContext->uNumRecords % SORT_ARRAY_GROW_SIZE) )
        {
           #ifndef XP_WIN16
            pBase = (void *)XP_REALLOC( pBase, (hGHContext->uNumRecords + SORT_ARRAY_GROW_SIZE)*sizeof(gh_HistList *) );
           #else
            pBuf = (void *)XP_HUGE_ALLOC( (hGHContext->uNumRecords + SORT_ARRAY_GROW_SIZE)*sizeof(gh_HistList *) );
            XP_HUGE_MEMCPY( pBuf, pBase, hGHContext->uNumRecords * sizeof(gh_HistList *) );
            XP_HUGE_FREE( pBase );
            pBase = pBuf;
           #endif /* XP_WIN16 */
        }
	}while( 0 == (gh_database->seq)(gh_database, &key, &data, R_NEXT) );

    /*
    // Perform a quick sort on the array, sorting by the pData member.
    */
    
    switch( enGHSort )
    {
       case eGH_NameSort:
       case eGH_LocationSort:           
       {
          int32 i;
          gh_HistList XP_HUGE **pList;
          
          XP_QSORT( pBase, hGHContext->uNumRecords, sizeof(void *), QSortCompStr );
          
          if( enGHSort == eGH_LocationSort )
          {
             break;
          }

          /*
          *  Now perform a secondary sort on Location
          */
          pList = (gh_HistList XP_HUGE **)pBase;
          
          for( i = 1; i < (int32)hGHContext->uNumRecords; i++ )
          {
             int32 iStartPos = i-1;
             int32 iBlockLen;             
             while( (i < (int32)hGHContext->uNumRecords) &&
                    !XP_StrColl( pList[i]->pData, pList[i-1]->pData ) )
             {
                i++;
             }
             iBlockLen = i - iStartPos;
             if( iBlockLen > 1 )
             {
                XP_QSORT( pList+iStartPos, iBlockLen, sizeof(void *), QSortCompStr2 );
             }
          }
          
          break;
       }
          
       case eGH_FirstDateSort:
       case eGH_LastDateSort:
       {
          int32 i;       
          gh_HistList XP_HUGE **pList;
                 
          XP_QSORT( pBase, hGHContext->uNumRecords, sizeof(void *), QSortCompDate );    

          /*
          *  Now perform a secondary sort on Location
          */
          pList = (gh_HistList XP_HUGE **)pBase;
          
          for( i = 1; i < (int32)hGHContext->uNumRecords; i++ )
          {
             int32 iStartPos = i-1;
             int32 iBlockLen;             
             while( (i < (int32)hGHContext->uNumRecords) &&
                    *(time_t *)pList[i]->pData == *(time_t *)pList[i-1]->pData )
             {
                i++;
             }
             iBlockLen = i - iStartPos;
             if( iBlockLen > 1 )
             {
                XP_QSORT( pList+iStartPos, iBlockLen, sizeof(void *), QSortCompStr2 );
             }
          }
          
          break;
       }
       
       case eGH_VisitCountSort:
       {
          int32 i;       
          gh_HistList XP_HUGE **pList;
                 
          XP_QSORT( pBase, hGHContext->uNumRecords, sizeof(void *), QSortCompInt32 );    

          /*
          *  Now perform a secondary sort on Location
          */
          pList = (gh_HistList XP_HUGE **)pBase;
          
          for( i = 1; i < (int32)hGHContext->uNumRecords; i++ )
          {
             int32 iStartPos = i-1;
             int32 iBlockLen;             
             
             while( (i < (int32)hGHContext->uNumRecords) &&
                    *(int32 *)pList[i]->pData == *(int32 *)pList[i-1]->pData )
             {
                i++;
             }
             iBlockLen = i - iStartPos;
             if( iBlockLen > 1 )
             {
                XP_QSORT( pList+iStartPos, iBlockLen, sizeof(void *), QSortCompStr2 );
             }
          }
          
          break;
       }
	  default:
		break;
    }
        
    hGHContext->pHistSort = (gh_HistList XP_HUGE **)pBase;
    
    return hGHContext;
}

PUBLIC void
GH_ReleaseContext( GHHANDLE pContext, Bool bReleaseUR )
{
   gh_HistContext *hGHContext = (gh_HistContext *)pContext;
   uint32 uRow;
   
   if( !pContext )
   {
      return;
   }
   
   XP_ASSERT( pHistContextList );
   
   if( pHistContextList == hGHContext )
   {
      if( hGHContext->pNext == hGHContext )
      {
         pHistContextList = NULL;
      }
      else
      {
         pHistContextList = hGHContext->pNext;
      }
   }
   hGHContext->pPrev->pNext = hGHContext->pNext;
   hGHContext->pNext->pPrev = hGHContext->pPrev;
   
   /*
   // Release all the memory associated with the History Context
   */
   
   for( uRow = 0; uRow < hGHContext->uNumRecords; uRow++ )
   {
      XP_FREE( hGHContext->pHistSort[uRow]->pKeyData );
      
      if( hGHContext->enGHSort != eGH_LocationSort )
      {
         /* The pData member points to the same storage as pKeyData for Location sort */
         
         XP_FREE( hGHContext->pHistSort[uRow]->pData );      
      }
      
      XP_FREE( hGHContext->pHistSort[uRow] );
   }
   
   XP_HUGE_FREE( hGHContext->pHistSort );
   
   if( bReleaseUR )
   {
      GH_ReleaseURContext( hGHContext->pURContext );
   }
}

PUBLIC uint32 
GH_GetNumRecords( GHHANDLE pContext )
{
   gh_HistContext *hGHContext = (gh_HistContext *)pContext;
   
   return hGHContext ? hGHContext->uNumRecords : 0;
}

PUBLIC gh_SortColumn
GH_GetSortField( GHHANDLE pContext )
{
   gh_HistContext *hGHContext = (gh_HistContext *)pContext;
   
   return hGHContext ? hGHContext->enGHSort : eGH_NoSort;
}


PR_PUBLIC_API(void) updateNewHistItem (DBT *key, DBT *data);


PUBLIC int
GH_UpdateURLTitle( URL_Struct *pUrl, char *pszTitle, Bool bFrameCell )
{
	DBT key, data, dataNew;
	int status;
    int iNameLen;
    int8 *pData;
	static int32 count=0;
    int32 iFlags = bFrameCell ? (GH_FLAGS_SHOW | GH_FLAGS_FRAMECELL) : GH_FLAGS_SHOW;
        
	if( !pUrl || !pUrl->address || !pszTitle )
    {
	    return -1;
    }
    
	if( !gh_database )
    {
		return -1;
    }

	gh_open_database();

	if( !gh_database )
    {
		return -1;
    }
    
	global_history_has_changed = TRUE;
	urlLookupGlobalHistHasChanged = TRUE;

	count++;  /* Increment change count */
    
	key.data = (void *)pUrl->address;
	key.size = (XP_STRLEN(pUrl->address)+1) * sizeof(char);

	status = (*gh_database->get)(gh_database, &key, &data, 0);

	if( status < 0 )
    {
       TRACEMSG(("Database ERROR retreiving global history entry"));
       return -1;
    }
	else if( status > 0 )
    {
       return -1;
    }

	/* Object was found */
    
    iNameLen = XP_STRLEN( pszTitle )+1;    
    dataNew.size = sizeof(int32) + sizeof(int32) + sizeof(int32) + sizeof(int32) + iNameLen*sizeof(char);
    
    pData = XP_ALLOC( dataNew.size );

    dataNew.data = (void *)pData;
    
    /*
    // Copy the record's data into the new buffer
    */
    XP_MEMCPY( pData, data.data, (data.size < dataNew.size) ? data.size : dataNew.size );
    
    /*
    // Now overwrite the old title with the new
    */
    if( iNameLen > 1  )
    {
        XP_STRCPY( (char *)pData+4*sizeof(int32), pszTitle );
    }
    else
    {
        *(char *)(pData+4*sizeof(int32)) = 0;
    }

    /*
    // Mark this record for global history viewing
    */
    COPY_INT32( pData+3*sizeof(int32), &iFlags );
    
    /*
    // Update the table
    */
	status = (*gh_database->put)( gh_database, &key, &dataNew, 0 );


	/* update the  history display in nav center if its open */
    if (iFlags == 1) updateNewHistItem(&key, &dataNew);


    XP_FREE( pData );
    
	if( status < 0 )
    {
        TRACEMSG(("Global history update failed due to database error"));
        gh_RemoveDatabase();
    }
    else if( count >= SYNC_RATE )
    {
        count = 0;
        if( -1 == (*gh_database->sync)( gh_database, 0 ) )
        {
            TRACEMSG(("Error syncing gh database"));
        }
    }
    
    /*
    // Notify the contexts of the update
    */
    GH_NotifyContexts( GH_NOTIFY_UPDATE, (char *)key.data );
    return 0;
}

PUBLIC gh_HistEntry *
GH_GetRecord( GHHANDLE pContext, uint32 uRow )
{
   DBT key, data;
   static gh_HistEntry ghEntry;
   static char szTitle[1024];
   int32 iDefault = 1;
   
   gh_HistContext *hGHContext = (gh_HistContext *)pContext;
   
   if( !hGHContext || !hGHContext->uNumRecords )
   {
      return NULL;
   }
   
   if( uRow > (hGHContext->uNumRecords-1) )
   {
      /* Row is out of range (cannot be less than 0 since it is unsigned */
      return NULL;
   }
   
   key.data = (void *)hGHContext->pHistSort[uRow]->pKeyData;
   key.size = (XP_STRLEN(key.data)+1) * sizeof(char);
   
   ghEntry.address = key.data;
   
   if( 0 == (*gh_database->get)( gh_database, &key, &data, 0 ) )
   {
      if( data.size > sizeof(int32) )
      {
         /* The entry/record is of the new format */

         COPY_INT32( &ghEntry.first_accessed, (int8 *)data.data + sizeof(int32) );
         
         COPY_INT32( &ghEntry.last_accessed, data.data );
         
         COPY_INT32( &ghEntry.iCount, (int8 *)data.data + 2*sizeof(int32) );         
         
         XP_STRNCPY_SAFE( szTitle, (char *)data.data + 4*sizeof(int32), sizeof(szTitle)-1 );
         ghEntry.pszName = szTitle;
      }
      else
      {
         /* The entry/record is of the old format i.e., only last_accessed date available */
         
         char *pszTitle = GH_GetTitleFromURL( ghEntry.address );         
         XP_STRNCPY_SAFE( szTitle, pszTitle, sizeof(szTitle)-1 );
         pszTitle = strrchr( szTitle, '/' );
         if( pszTitle )
         {
		   /* Remove the trailing slash from the title */
            *pszTitle = 0;
         }
         ghEntry.pszName = szTitle;
                  
         COPY_INT32( &ghEntry.first_accessed, data.data );
         
         COPY_INT32( &ghEntry.last_accessed, data.data );         
         
         COPY_INT32( &ghEntry.iCount, &iDefault );         
      }
      return &ghEntry;
      
   }
   else
   {
      /* The entry is not there, we're out of sync somehow. */
      return NULL;
   }
}

PUBLIC void
GH_DeleteRecord( GHHANDLE pContext, uint32 uRow, Bool bGroup )
{
    DBT key, data;
    int status;
       
    gh_HistContext *hGHContext = (gh_HistContext *)pContext;
    
    if( !hGHContext || !hGHContext->uNumRecords )
    {
       return;
    }
    
    if( uRow > (hGHContext->uNumRecords-1) )
    {
       /* Row is out of range (cannot be less than 0 since it is unsigned */
       return;
    }
    
    key.data = (void *)hGHContext->pHistSort[uRow]->pKeyData;
    key.size = (XP_STRLEN(key.data)+1) * sizeof(char);

    if( hGHContext->pURContext )
    {
        gh_RecordList *  pRecordNode = NULL;
        gh_URList *      pURNode = NULL;
        
        /* Get the record's data so we can undo the operation if necessary */
        
        status = (*gh_database->get)( gh_database, &key, &data, 0 );
        if( status < 0 )
        {
        	TRACEMSG(("Database ERROR retreiving global history entry"));
        	return;
        }
        else if( status > 0 )
        {
        	return;
        }
        
        pRecordNode = GH_CreateRecordNode( &key, &data );
        
        if( bGroup )
        {
            GH_PushGroupUndo( hGHContext->pURContext, pRecordNode );
        }
        else
        {
            pURNode = GH_CreateURNode( pRecordNode );
            GH_PushUR( &hGHContext->pURContext->pUndoList, pURNode );
            
            /* Must purge the redo list whenever we add a new undo item */
            GH_DeleteURList( hGHContext->pURContext->pRedoList );
            hGHContext->pURContext->pRedoList = NULL;
        }
    }
    
    (*gh_database->del)( gh_database, &key, 0 );    
    
    /*
    // Notify the contexts of the deletion
    */
    GH_NotifyContexts( GH_NOTIFY_DELETE, (char *)key.data );
}

PUBLIC int32 
GH_GetRecordNum( GHHANDLE pContext, char *pszLocation )
{
   int32 i = 0;
      
   gh_HistContext *hGHContext = (gh_HistContext *)pContext;
   
   if( !hGHContext || !hGHContext->uNumRecords || !pszLocation )
   {
      return -1;
   }

   for( i = 0; i < (int32)hGHContext->uNumRecords; i++ )
   {
      if( !XP_STRCMP( pszLocation, hGHContext->pHistSort[i]->pKeyData ) )
      {
         return i;
      }
   }
   
   return -1;
}

/* Writes out a URL entry to look like:
 *
 * <DT><A HREF="http://www.netscape.com" \
 * ADD_DATE="777240414" LAST_VISIT="802992591">Welcome To Netscape</A>
 *
 */
PRIVATE int
GH_WriteURL( XP_File fp, gh_HistEntry *item )
{
   char buffer[16];

   WRITE( "<DT>", -1, fp );

   /* write address */
   WRITE( "<A HREF=\"", -1, fp );
   WRITE( item->address, -1, fp );
   WRITE( "\"", -1, fp );

   /* write the addition date  */
   WRITE( " FIRST_VISIT=\"", -1, fp );
   XP_SPRINTF( buffer, "%ld", item->first_accessed );
   WRITE( buffer, -1, fp );
   WRITE( "\"", -1, fp );

   /* write the last visited date */
   WRITE( " LAST_VISIT=\"", -1, fp );
   XP_SPRINTF( buffer, "%ld\"", item->last_accessed );
   WRITE( buffer, -1, fp );

   /* write the last modified date */
   WRITE( " VISIT_COUNT=\"", -1, fp );
   XP_SPRINTF( buffer, "%i\"", item->iCount );
   WRITE( buffer, -1, fp );

   WRITE( ">", -1, fp );

   /* write the name */

   if( item->pszName ) 
   {
      WRITE( item->pszName, -1, fp );
   }

   WRITE( "</A>", -1, fp );
   WRITE( LINEBREAK, LINEBREAK_LEN, fp );

   return 0;
}

PRIVATE void
GH_WriteHTML( MWContext *context, char *filename, GHHANDLE pContext )
{
   XP_File           fp = NULL;
   XP_FileType       tmptype;
   char *            tmpname = NULL;
   int32             i = 0;  
   gh_HistContext *  hGHContext = (gh_HistContext *)pContext;

   if( !hGHContext || !filename || (filename[0] == '\0') )
   {
      return;
   }
   
 #if 0
   tmpname = FE_GetTempFileFor( NULL, filename, xpGlobalHistoryList, &tmptype );
 #else
   tmpname = filename;
   tmptype = xpFileToPost; /* let's us simply use the name the user types */
 #endif

   fp = XP_FileOpen( tmpname, tmptype, XP_FILE_WRITE );

   if( !fp )
   {
      goto FAIL;
   }

   /* write cookie */
   if( gh_write_ok( GLHIST_COOKIE, -1, fp) < 0 ) goto FAIL;
   if( gh_write_ok( LINEBREAK, LINEBREAK_LEN, fp ) < 0 ) goto FAIL;
   if( gh_write_ok( LINEBREAK, LINEBREAK_LEN, fp ) < 0 ) goto FAIL;

   /* Write out all the history records according to the context/cursor */
   for( i = 0; i < (int32)hGHContext->uNumRecords; i++ )
   {
      gh_HistEntry *pHistEntry = GH_GetRecord( hGHContext, i );
      if( pHistEntry )
      {
         GH_WriteURL( fp, pHistEntry );
      }
   }
  
   if( XP_FileClose( fp ) != 0 ) 
   {
      fp = NULL;
      goto FAIL;
   }
   fp = NULL;
 #if 0  
   XP_FileRename( tmpname, tmptype, filename, xpGlobalHistoryList );
 #else
   XP_FREE( tmpname );
 #endif
   tmpname = NULL;

   return;

 FAIL:
   if( fp ) 
   {
      XP_FileClose(fp);
   }
   
   if( tmpname ) 
   {
      XP_FileRemove( tmpname, tmptype );
      XP_FREE( tmpname );
      tmpname = NULL;
   }

   return;
}

PUBLIC void
GH_FileSaveAsHTML( GHHANDLE pContext, MWContext *pMWContext )
{
   if( !pContext )
   {
      return;
   }
   
   FE_PromptForFileName( pMWContext, XP_GetString( XP_HISTORY_SAVE ), 0, FALSE, FALSE, GH_WriteHTML, pContext );
}

PUBLIC GHURHANDLE GH_GetURContext( GHHANDLE pContext )
{
   gh_HistContext *hGHContext = (gh_HistContext *)pContext;
   
   if( !pContext )
   {
      return NULL;
   }

   return hGHContext->pURContext;
}

PUBLIC void GH_SupportUndoRedo( GHHANDLE pContext )
{
   gh_HistContext *hGHContext = (gh_HistContext *)pContext;
   
   if( !pContext )
   {
      return;
   }

   GH_CreateURContext( hGHContext );
}

PRIVATE gh_RecordList *
GH_CreateRecordNode( DBT *pKey, DBT *pData )
{
   gh_RecordList *pRecordNode = NULL;
   
   if( !pKey ||
       !pKey->data ||
       !pData ||
       !pData->data )
   {
      return NULL;
   }
   
   pRecordNode = XP_ALLOC( sizeof(gh_RecordList) );
   
   pRecordNode->key.data  = XP_ALLOC( pKey->size );
   XP_MEMCPY( pRecordNode->key.data, pKey->data, pKey->size );   
   pRecordNode->key.size = pKey->size;
   
   pRecordNode->data.data = XP_ALLOC( pData->size );
   XP_MEMCPY( pRecordNode->data.data, pData->data, pData->size );   
   pRecordNode->data.size = pData->size;

   pRecordNode->pNext = NULL;
   
   return pRecordNode;
}

PRIVATE void 
GH_PushRecord( gh_RecordList **ppRecordList, gh_RecordList *pRecordNode )
{
   if( !ppRecordList || !pRecordNode )
   {
      return;
   }
   
   pRecordNode->pNext = *ppRecordList ? *ppRecordList : NULL;
   *ppRecordList = pRecordNode;
}

PRIVATE void 
GH_PushGroupUndo( gh_URContext *pURContext, gh_RecordList *pRecordNode )
{
   if( !pURContext || !pURContext->pUndoList || !pRecordNode )
   {
       return;
   }
   
   GH_PushRecord( &pURContext->pUndoList->pURItem, pRecordNode );
}

PRIVATE gh_URList *
GH_CreateURNode( gh_RecordList *pRecordList )
{
   gh_URList *pURNode = NULL;

   if( !pRecordList )   
   {
      return NULL;
   }
   
   pURNode = XP_ALLOC( sizeof(gh_URList) );
   pURNode->pURItem = pRecordList;
   pURNode->pNext   = NULL;
   
   return pURNode;
}

PRIVATE void 
GH_PushUR( gh_URList **ppURList, gh_URList *pURNode )
{
   if( !ppURList || !pURNode )
   {
      return;
   }
   
   pURNode->pNext = *ppURList ? *ppURList : NULL;
   *ppURList = pURNode;
}

PRIVATE gh_URList *
GH_PopUR( gh_URList **ppURList )
{
   gh_URList *pURNode = NULL;
      
   if( !ppURList )
   {
      return NULL;
   }

   pURNode = *ppURList;
   *ppURList = (*ppURList)->pNext;
   
   return pURNode;
}

PUBLIC void 
GH_Undo( GHHANDLE hContext )
{
    gh_HistContext *pContext = (gh_HistContext *)hContext;
    gh_URList *pURNode = NULL;
 
    int status;
    gh_RecordList *pCsr;
  
    if( !pContext || 
        !pContext->pURContext || 
        !pContext->pURContext->pUndoList )
    {
       return;
    }
    
    pURNode = GH_PopUR( &pContext->pURContext->pUndoList );
    if( !pURNode )
    {
       return;
    }

	if( !gh_database )
    {
		return;
    }

	gh_open_database();

	if( !gh_database )
    {
		return;
    }
    
	global_history_has_changed = TRUE;
	urlLookupGlobalHistHasChanged = TRUE;


    pCsr = pURNode->pURItem;
    while( pCsr )
    {
        /*
        // Update the table
        */
    	status = (*gh_database->put)( gh_database, &pCsr->key, &pCsr->data, 0 );

    	if( status < 0 )
        {
            TRACEMSG(("Global history update failed due to database error"));
            gh_RemoveDatabase();
        }

        /*
        // Notify the contexts of the update
        */
        GH_NotifyContexts( GH_NOTIFY_UPDATE, (char *)pCsr->key.data );
        
        pCsr = pCsr->pNext;
    }

    GH_PushUR( &pContext->pURContext->pRedoList, pURNode );
}

PUBLIC void 
GH_Redo( GHHANDLE hContext )
{
    gh_HistContext *pContext = (gh_HistContext *)hContext;
    gh_URList *pURNode = NULL;
 
    int status;
    gh_RecordList *pCsr;
  
    if( !pContext || 
        !pContext->pURContext || 
        !pContext->pURContext->pRedoList )
    {
       return;
    }
    
    pURNode = GH_PopUR( &pContext->pURContext->pRedoList );
    if( !pURNode )
    {
       return;
    }

	if( !gh_database )
    {
		return;
    }

	gh_open_database();

	if( !gh_database )
    {
		return;
    }
    
	global_history_has_changed = TRUE;
	urlLookupGlobalHistHasChanged = TRUE;


    pCsr = pURNode->pURItem;
    while( pCsr )
    {
        /*
        // Delete the record
        */
    	status = (*gh_database->del)( gh_database, &pCsr->key, 0 );

    	if( status < 0 )
        {
            TRACEMSG(("Global history update failed due to database error"));
            gh_RemoveDatabase();
        }
        else if( status > 0 )
        {
            /* Not found */
            pCsr = pCsr->pNext;        
            continue;
        }

        /*
        // Notify the contexts of the deletion
        */
        GH_NotifyContexts( GH_NOTIFY_DELETE, (char *)pCsr->key.data );
        
        pCsr = pCsr->pNext;
    }

    GH_PushUR( &pContext->pURContext->pUndoList, pURNode );
}

PUBLIC Bool GH_CanUndo( GHHANDLE hContext )
{
    Bool bRet = FALSE;
    gh_HistContext *pContext = (gh_HistContext *)hContext;
    
    if( pContext &&
        pContext->pURContext &&
        pContext->pURContext->pUndoList )
    {
       bRet = TRUE;
    }
    
    return bRet;
}

PUBLIC Bool GH_CanRedo( GHHANDLE hContext )
{
    Bool bRet = FALSE;
    gh_HistContext *pContext = (gh_HistContext *)hContext;
    
    if( pContext &&
        pContext->pURContext &&
        pContext->pURContext->pRedoList )
    {
       bRet = TRUE;
    }
    
    return bRet;
}

PRIVATE void GH_DeleteURList( gh_URList *pURList )
{
   gh_URList *pTrash = NULL;
   
   if( !pURList )
   {
      return;
   }
   
   do
   {
      pTrash = pURList;
      pURList = pURList->pNext;
      GH_DeleteRecordList( pTrash->pURItem );
      XP_FREE( pTrash );
      
   }while( pURList );
}

PUBLIC int GH_GetMRUPage( char *pszURL, int iMaxLen )
{
    /*
    //  Note this function will not return a cell in a frame, instead it returns the mru frame set url.
    */
    time_t           date1, date2 = 0;
	DBT              key, data;
    
	if( !gh_database || (global_history_timeout_interval < 1) )
    {
		return 0;
    }

    if( !pszURL || (iMaxLen <= 0) )
    {
       return 0;
    }
    
    *pszURL = 0;
    
    data.size = key.size = 0;
    
    /*
    // Visit each page in the history list while maintaining the most recently visited page.
    */
	if( 0 != (*gh_database->seq)( gh_database, &key, &data, R_FIRST ) )
    {
		return 0;
	}
    
	do
	{
        if( data.size > sizeof(int32) )
        {
            /*
            // The entry/record is of the new format...        
            //
            
            // Ignore history records which are NOT flagged as having been explicitly loaded
            // e.g., don't expose gif images that are only part of a page
            */
            int32 iFlags;
            COPY_INT32( &iFlags, (int8 *)data.data + 3*sizeof(int32) );
            
            if( !(iFlags & GH_FLAGS_SHOW) || (iFlags & GH_FLAGS_FRAMECELL) )
            {
               /* The record is either not flagged for showing or is just a cell or both */
               
               continue;
            }
        }
        else
        {   
            /*
            // The entry/record is of the old format...
            //
            
            // Try to be somewhat smart and filter the implicitly loaded stuff i.e., non-html docs
            */
            char *pszExt = strrchr( key.data, '.' );
            
            if( ((char *)key.data)[XP_STRLEN(key.data)-1] == '/' )
            {
            }
            else if( XP_STRLEN(key.data) < 5 )
            {
               continue;
            }
            else if( pszExt )
            {
               pszExt++;
               if( strncasecomp( pszExt, "htm", 3 ) )
               {
                  continue;
               }
            }
            else
            {
               continue;
            }
        }
        
        /* Note the last_accessed field is at the beginning of the data regardless of new/old format */
        
        COPY_INT32( &date1, data.data );
        
        if( difftime( date1, date2 ) > 0 )
        {
            XP_STRNCPY_SAFE( pszURL, key.data, iMaxLen );
            COPY_INT32( &date2, (int8 *)data.data );
        }      
        
	}while( 0 == (gh_database->seq)( gh_database, &key, &data, R_NEXT ) );

    return XP_STRLEN( pszURL );
}

PRIVATE void GH_DeleteRecordList( gh_RecordList *pRecordList )
{
   gh_RecordList *pTrash = NULL;
   
   if( !pRecordList )
   {
      return;
   }
   
   do
   {
      pTrash = pRecordList;
      pRecordList = pRecordList->pNext;
      XP_FREE( pTrash->key.data );
      XP_FREE( pTrash->data.data );
      XP_FREE( pTrash );
      
   }while( pRecordList );
}

/*
// Return a pointer somewhere in pszURL where a somewhat meaningful title may be.
// A pointer to the char following the last or second to last slash is returned
// if a string exists after the slash, otherwise pszURL is returned.
*/
PRIVATE char *GH_GetTitleFromURL( char *pszURL )
{
   char *pszTitle = NULL;
   char *pszSlash = NULL;
   
   if( !pszURL || !*pszURL )
   {
      return pszURL;
   }

   pszTitle = strrchr( pszURL, '/' );

   if( pszTitle )
   {
      if( *(pszTitle+1) )
      {
         /*
         // The location does not end with a slash so we'll use the sub-string
         // following the the last slash.
         */
         
         pszTitle++;
      }
      else
      {
         /*
         // The location ends with a slash, so we'll start at the second from last slash.
         */
         
         *pszTitle = 0;
         pszSlash = pszTitle; /* Save this position so we can put the slash back */
         pszTitle = strrchr( pszURL, '/' );
         *pszSlash = '/';
         
         if( pszTitle )
         {
            if( *(pszTitle+1) )
            {
               pszTitle++;
            }
            else
            {
               pszTitle = pszURL;
            }
         }
      }
   }
   else
   {
      pszTitle = pszURL;
   }
   
   return pszTitle;
}

/* fix Mac warning about missing prototype */
PUBLIC Bool
NET_EnableUrlMatch(void);

PUBLIC Bool
NET_EnableUrlMatch(void)
{
	return enableUrlMatch;
}

/* fix Mac warning about missing prototype */
PUBLIC void
NET_SetEnableUrlMatchPref(Bool x);

PUBLIC void
NET_SetEnableUrlMatchPref(Bool x)
{
	enableUrlMatch=x;
}

/* fix Mac warning about missing prototype */
MODULE_PRIVATE int PR_CALLBACK
NET_EnableUrlMatchPrefChanged(const char *pref, void *data);

MODULE_PRIVATE int PR_CALLBACK
NET_EnableUrlMatchPrefChanged(const char *pref, void *data)
{
	Bool x;

	PREF_GetBoolPref("network.enableUrlMatch", &x);
	NET_SetEnableUrlMatchPref(x);
	return PREF_NOERROR;
}

PUBLIC void
NET_RegisterEnableUrlMatchCallback(void)
{
	Bool x;

	PREF_GetBoolPref("network.enableUrlMatch", &x);
	NET_SetEnableUrlMatchPref(x);
	PREF_RegisterCallback("network.enableUrlMatch", NET_EnableUrlMatchPrefChanged, NULL);
}

/*	Is the string passed in too general. */
PRIVATE Bool
net_url_sub_string_too_general(const char *criteria, int32 len)
{
	if( (!criteria) || (len < 1) )
		return TRUE;

	/* case insensative compares */
	if( !strncasecomp(criteria, "www.", len) ||
		!strncasecomp(criteria, "http://www.", len) ||
		!strncasecomp(criteria, "ftp.", len) ||
		!strncasecomp(criteria, "ftp://ftp.", len) ||
		!strncasecomp(criteria, "file:", len)
		)
		return TRUE;
	return FALSE;
}

/* Determines whether we want to deal with this url. I'm doing some interpretation here. If the user has
   www.abc.com/cgi/laksjdlskjds121212121 in their global history, I'm assuming they don't want this to come
   up in the completeion. They may though. You can't satisfy everyone. */
PRIVATE Bool
net_url_weed_out(const char *url, int32 len)
{
	if( (!url) || (len < 0) )
		return TRUE;
	url = url + len - 1;
	if(!(*url == 'l' ||
		*url == 'L' || 
		*url == 'm' || 
		*url == 'M' || 
		*url == 'p' || /* some msoft frontpage-like cgi filename i.e. default.asp */
		*url == 'P' ||
		*url == '/') )
		return TRUE;
	return FALSE;
}


/* Description:
	Tries to find a match in the global history database given the criteria string. If found
	the match is returned via result (result is passed in as an unallocated address of a char *,
	and returned as an allocated char * if the return val of the fctn is foundDone; you're 
	responsible for freeing it).

   Parameters:
    criteria - a string for me to search for.
	result - the address of a char * where I will put data if I find it.
	freshStart - Is this the first time calling me? Yes == TRUE.
	scroll - Am I being called because user wants to scroll through matches?

   Return Type:
    enum autoCompStatus (declared in glhist.h)
   Return Values:
    foundDone - the unallocated address of a char * you passed in now has data in it. The data
		consists of the best match I could find.
	notFoundDone - I searched all my resources and couldn't find anything (ie don't call me again
		with the same criteria), or I found something but it was the same thing I last returned to 
		you, or what you passed in is what I returned to you last time you called.
	stillSearching - I don't search through all my resources at once, call me again
	dontCallOnIdle - Don't call me again with the same criteria, the criteria is too general and
		I don't want to waste cycles.

   This function uses a totally inefficient means of searching (sequential). Function is optimized
   for speed, not flexibility or readibility.

  Created by: Judson Valeski, 1997
*/
PUBLIC enum autoCompStatus
urlMatch(const char *criteria, char **result, Bool freshStart, Bool scroll)
{
	static char *lastURLCompletion=NULL;
	int32 eLen, cLen, cPathLen=0, count=0;
	DBT key, data;
	char *t=NULL, *p=NULL, *host=NULL, *ePath=NULL, *cPath=NULL;
	char *eProtocolColon=NULL, *cProtocolColon=NULL;
	
	if(!NET_EnableUrlMatch())
		return notFoundDone;

	if(!criteria || (*criteria == '/') )
		return notFoundDone;

	/* Is it ok to use the database. */
	if(!gh_database || global_history_timeout_interval < 1)
		return notFoundDone;

	cLen = XP_STRLEN(criteria);

	/* Is the criteria too general? ie. www or ftp, etc */
	if( net_url_sub_string_too_general(criteria, cLen) )
		return dontCallOnIdle;

	/* Did the user include a protocol? If so, we want to search with protocol included. */
	cProtocolColon=XP_STRSTR(criteria, "://");

	/* Check to see if user has path info on url */
	if(cProtocolColon)
		cPath=XP_STRCHR(cProtocolColon+3, '/');
	else
		cPath=XP_STRCHR(criteria, '/');

	if(cPath)
		cPathLen=XP_STRLEN(cPath);

	if(freshStart || urlLookupGlobalHistHasChanged)	{
		if(0 != (*gh_database->seq)(gh_database, &key, &data, R_FIRST))
			return notFoundDone;
	}
	else {
		if(0 != (gh_database->seq)(gh_database, &key, &data, R_NEXT) )
			return notFoundDone;
	}

	urlLookupGlobalHistHasChanged = FALSE;

	/* Main search loop */
	do {
		if(count > entriesToSearch) /* entries to search is a static defined above */
			return stillSearching;

		/* If there's no url or there's no slash in it, move on */
		if( !((char *)key.data) || !(host=XP_STRCHR((char *)key.data, '/')) ) {
			count++;
			continue;
		}

		/* Get the url out of the db entry and determine whether or not we want
		   to include the protocol in our search. After this if stmt, t will point to
		   allocated memory that must be free'd. */
		if(cProtocolColon) {
			t = XP_STRDUP((char *)key.data);
			if(!t) {
				count++;
				continue;
			}
		}
		else {
			/* host is assigned in the previous if stmt and is guaranteed to be valid. */
			if( !(host[0] && host[1] && host[2]) ) {
				/* the db entry isn't in the format that we were expecting */ 
				count++;
				continue;
			}
			t = XP_STRDUP(host+2);
			if(!t) {
				count++;
				continue;
			}
		}

		if( (eProtocolColon=XP_STRSTR(t, "://")) != NULL)
			ePath=XP_STRCHR(eProtocolColon+3, '/');
		else
			ePath=XP_STRCHR(t, '/');		

		/* If there's no path in the entity url then the db entry was bad. Move on. */
		if(!ePath) {
			XP_FREE(t);
			count++;
			continue;
		}

		eLen = XP_STRLEN(t);

		if(cPath)
			/* Do we want to weed out the url, ie. it's full of cgi stuff. */
			if( net_url_weed_out(t, eLen) )	{
				XP_FREE(t);
				count++;
				continue;
			}

		/* See if domains are the same. Case-insensative. */
		*ePath='\0';
		if(cPath)
			*cPath='\0';
		/* If the domains aren't the same. Move on. */
		if(strncasecomp(t, criteria, cLen)) {
			if(cPath)
				*cPath='/';
			XP_FREE(t);
			count++;
			continue;
		}
		*ePath='/';
		if(cPath)
			*cPath='/';

		/* See if the paths are the same. Case-sensative. 
		   If there's no cPath and we've gotten this far then the user hasn't specified anything
		   more than the domain and the check above determined that the domain matched so continue.
		   Otherwise check the remaining chars. */
		if( !cPath || !XP_STRNCMP(ePath, cPath, cPathLen)) {
			/* if user didn't specify path info ,
			   set the char just after the end to null byte */
			if(!cPath) {
				if(cProtocolColon) {
					if( (p=XP_STRCHR(t, '/')) != NULL && p[1] && p[2] ) {
						if( (p=XP_STRCHR(p+2, '/')) != NULL ) {
							p[1] = '\0';
						}
						else {
							XP_FREE(t);
							count++;
							continue;
						}
					}
				}
				else {
					if( (p=XP_STRCHR(t, '/')) != NULL) {
						p[1]= '\0';
					}
					else {
						XP_FREE(t);
						count++;
						continue;
					}
				}
			}

			/* if we're scrolling && lastURLCompletion is not empty && what we're currently
			   matching against isn't what we last returned, or, what we're matching against
			   is identical to what the caller passed in. */
			if( (( scroll && lastURLCompletion && (!strcasecomp(t, lastURLCompletion))) ) 
				||
				(!strcasecomp(t, criteria)) ) {

				if(!scroll) {
					XP_FREEIF(lastURLCompletion);
					lastURLCompletion=NULL;
					XP_FREE(t);
					return notFoundDone;
				}

				XP_FREE(t);
				count++;
				continue;
			}
			XP_FREEIF(lastURLCompletion);
			lastURLCompletion = XP_STRDUP(t);
			*result = XP_STRDUP(t);
			XP_FREE(t);
			return foundDone;
		}

		XP_FREE(t);
		count++;
	}
	while( 0 == (gh_database->seq)(gh_database, &key, &data, R_NEXT) );
	if(!scroll) {
		XP_FREEIF(lastURLCompletion);
		lastURLCompletion=NULL;
	}
	return notFoundDone;
}
