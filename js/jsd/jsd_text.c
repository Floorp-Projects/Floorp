/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
** JavaScript Debugger Navigator API - Source Text functions
*/

#include "jsd.h"
#ifdef NSPR20
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h"
#endif
#endif

/**
* XXX convert this and jsd_conv so that all accumulation of text done here
*     Also, use handle oriented alloc for Win16 64k limit problem
*/

PRCList jsd_source_list = PR_INIT_STATIC_CLIST(&jsd_source_list);
PRCList jsd_removed_source_list = PR_INIT_STATIC_CLIST(&jsd_removed_source_list);

/***************************************************************************/
/*
* typedef enum
* {
*     JSD_SOURCE_INITED,
*     JSD_SOURCE_PARTIAL,
*     JSD_SOURCE_COMPLETED,
*     JSD_SOURCE_ABORTED,
*     JSD_SOURCE_FAILED
*     
* } JSDSourceStatus;
*/

#ifdef DEBUG
void JSD_ASSERT_VALID_SOURCE_TEXT( JSDSourceText* jsdsrc )
{
    PR_ASSERT( jsdsrc );
    PR_ASSERT( jsdsrc->url );
}
#endif

/***************************************************************************/

/* XXX add serial number instead of dirty */
/* XXX add notification */

static PRUintn g_alterCount = 1;


static void
ClearText( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    if( jsdsrc->text )
        MY_XP_HUGE_FREE(jsdsrc->text);
    jsdsrc->text        = NULL;
    jsdsrc->textLength  = 0;
    jsdsrc->textSpace   = 0;
    jsdsrc->status      = JSD_SOURCE_CLEARED;
    jsdsrc->dirty       = JS_TRUE;
    jsdsrc->alterCount  = g_alterCount++ ;
}    

static JSBool
AppendText( JSDContext* jsdc, JSDSourceText* jsdsrc, 
            const char* text, size_t length )
{
#define MEMBUF_GROW 1000

    PRUintn neededSize = jsdsrc->textLength + length;

    if( neededSize > jsdsrc->textSpace )
    {
        MY_XP_HUGE_CHAR_PTR pBuf;
        PRUintn iNewSize;

        /* if this is the first alloc, the req might be all that's needed*/
        if( ! jsdsrc->textSpace )
             iNewSize = length;
        else
             iNewSize = (neededSize * 5 / 4) + MEMBUF_GROW;

        pBuf = (MY_XP_HUGE_CHAR_PTR) MY_XP_HUGE_ALLOC(iNewSize);
        if( pBuf )
        {
            if( jsdsrc->text )
            {
                MY_XP_HUGE_MEMCPY(pBuf, jsdsrc->text, jsdsrc->textLength);
                MY_XP_HUGE_FREE(jsdsrc->text);
            }
            jsdsrc->text = pBuf;
            jsdsrc->textSpace = iNewSize;
        }
        else 
        {
            /* LTNOTE: throw an out of memory exception */
            ClearText( jsdc, jsdsrc );
            jsdsrc->status = JSD_SOURCE_FAILED;
            return JS_FALSE;
        }
    }

    MY_XP_HUGE_MEMCPY( &jsdsrc->text[jsdsrc->textLength], text, length );
    jsdsrc->textLength += length;
    return JS_TRUE;
}

static JSDSourceText*
NewSource( JSDContext* jsdc, const char* url )
{
    JSDSourceText* jsdsrc = PR_NEWZAP(JSDSourceText);
    if( ! jsdsrc )
        return NULL;
    
    jsdsrc->url     = (char*) url; /* already a copy */
    jsdsrc->status  = JSD_SOURCE_INITED;
    jsdsrc->dirty   = JS_TRUE;
    jsdsrc->alterCount  = g_alterCount++ ;
            
    return jsdsrc;
}

static void
DestroySource( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    PR_ASSERT( NULL == jsdsrc->text );  /* must ClearText() first */
    MY_XP_FREE(jsdsrc->url);
    MY_XP_FREE(jsdsrc);
}

static void
RemoveSource( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    PR_REMOVE_LINK(&jsdsrc->links);
    ClearText( jsdc, jsdsrc );
    DestroySource( jsdc, jsdsrc );
}

static JSDSourceText*
AddSource( JSDContext* jsdc, const char* url )
{
    JSDSourceText* jsdsrc = NewSource( jsdc, url );
    if( ! jsdsrc )
        return NULL;
    PR_INSERT_LINK(&jsdsrc->links, &jsd_source_list);
    return jsdsrc;
}

static void
MoveSourceToFront( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    PR_REMOVE_LINK(&jsdsrc->links);
    PR_INSERT_LINK(&jsdsrc->links, &jsd_source_list);
}

static void
MoveSourceToRemovedList( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    ClearText(jsdc, jsdsrc);
    PR_REMOVE_LINK(&jsdsrc->links);
    PR_INSERT_LINK(&jsdsrc->links, &jsd_removed_source_list);
}

static void
RemoveSourceFromRemovedList( JSDContext* jsdc, JSDSourceText* jsdsrc )
{
    PR_REMOVE_LINK(&jsdsrc->links);
    DestroySource( jsdc, jsdsrc );
}

static JSBool
IsSourceInSourceList( JSDContext* jsdc, JSDSourceText* jsdsrcToFind )
{
    JSDSourceText *jsdsrc;

    for (jsdsrc = (JSDSourceText*)jsd_source_list.next;
         jsdsrc != (JSDSourceText*)&jsd_source_list;
         jsdsrc = (JSDSourceText*)jsdsrc->links.next ) 
    {
        if( jsdsrc == jsdsrcToFind )
            return JS_TRUE;
    }
    return JS_FALSE;
}

/*	compare strings in a case insensitive manner with a length limit
*/

static int 
strncasecomp (const char* one, const char * two, int n)
{
	const char *pA;
	const char *pB;
	
	for(pA=one, pB=two;; pA++, pB++) 
	  {
	    int tmp;
	    if (pA == one+n) 
			return 0;	
	    if (!(*pA && *pB)) 
			return *pA - *pB;
	    tmp = MY_XP_TO_LOWER(*pA) - MY_XP_TO_LOWER(*pB);
	    if (tmp) 
			return tmp;
	  }
}

static char file_url_prefix[]    = "file:";
#define FILE_URL_PREFIX_LEN     (sizeof file_url_prefix - 1)

const char*
jsd_BuildNormalizedURL( const char* url_string )
{
    char *new_url_string;

    if( ! url_string )
        return NULL;

    if (!MY_XP_STRNCASECMP(url_string, file_url_prefix, FILE_URL_PREFIX_LEN) &&
        url_string[FILE_URL_PREFIX_LEN + 0] == '/' &&
        url_string[FILE_URL_PREFIX_LEN + 1] == '/') {
        new_url_string = PR_smprintf("%s%s",
                                     file_url_prefix,
                                     url_string + FILE_URL_PREFIX_LEN + 2);
    } else {
        new_url_string = MY_XP_STRDUP(url_string);
    }
    return new_url_string;
}

/***************************************************************************/

#ifndef JSD_SIMULATION
static PRMonitor *jsd_text_mon = NULL; 
#endif /* JSD_SIMULATION */

void
jsd_LockSourceTextSubsystem(JSDContext* jsdc)
{
#ifndef JSD_SIMULATION
    if (jsd_text_mon == NULL)
        jsd_text_mon = PR_NewNamedMonitor("jsd-text-monitor");

    PR_EnterMonitor(jsd_text_mon);
#endif /* JSD_SIMULATION */
}

void
jsd_UnlockSourceTextSubsystem(JSDContext* jsdc)
{
#ifndef JSD_SIMULATION
    PR_ExitMonitor(jsd_text_mon);
#endif /* JSD_SIMULATION */
}

void
jsd_DestroyAllSources( JSDContext* jsdc )
{
    JSDSourceText *jsdsrc;
    JSDSourceText *next;

    for (jsdsrc = (JSDSourceText*)jsd_source_list.next;
         jsdsrc != (JSDSourceText*)&jsd_source_list;
         jsdsrc = next) 
    {
        next = (JSDSourceText*)jsdsrc->links.next;
        RemoveSource( jsdc, jsdsrc );
    }

    for (jsdsrc = (JSDSourceText*)jsd_removed_source_list.next;
         jsdsrc != (JSDSourceText*)&jsd_removed_source_list;
         jsdsrc = next) 
    {
        next = (JSDSourceText*)jsdsrc->links.next;
        RemoveSourceFromRemovedList( jsdc, jsdsrc );
    }

}

JSDSourceText*
jsd_IterateSources(JSDContext* jsdc, JSDSourceText **iterp)
{
    JSDSourceText *jsdsrc = *iterp;
    
    if (!jsdsrc)
        jsdsrc = (JSDSourceText *)jsd_source_list.next;
    if (jsdsrc == (JSDSourceText *)&jsd_source_list)
        return NULL;
    *iterp = (JSDSourceText *)jsdsrc->links.next;
    return jsdsrc;
}

JSDSourceText*
jsd_FindSourceForURL(JSDContext* jsdc, const char* url)
{
    JSDSourceText *jsdsrc;

    for (jsdsrc = (JSDSourceText *)jsd_source_list.next;
         jsdsrc != (JSDSourceText *)&jsd_source_list;
         jsdsrc = (JSDSourceText *)jsdsrc->links.next) {
        if (0 == strcmp(jsdsrc->url, url))
            return jsdsrc;
    }
    return NULL;
}

const char*
jsd_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    return jsdsrc->url;
}

JSBool
jsd_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen )
{
    *ppBuf = jsdsrc->text;
    *pLen  = jsdsrc->textLength;
    return JS_TRUE;
}

void
jsd_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    if( JSD_SOURCE_INITED  != jsdsrc->status &&
        JSD_SOURCE_PARTIAL != jsdsrc->status )
    {
        ClearText(jsdc, jsdsrc);
    }
}

JSDSourceStatus
jsd_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    return jsdsrc->status;
}

JSBool
jsd_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    return jsdsrc->dirty;
}

void
jsd_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, JSBool dirty)
{
    jsdsrc->dirty = dirty;
}

PRUintn
jsd_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    return jsdsrc->alterCount;
}

PRUintn
jsd_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc)
{
    return ++jsdsrc->alterCount;
}

/***************************************************************************/

#if defined(DEBUG) && 0
void DEBUG_ITERATE_SOURCES( JSDContext* jsdc )
{
    JSDSourceText* iterp = NULL;
    JSDSourceText* jsdsrc = NULL;
    int dummy;
    
    while( NULL != (jsdsrc = jsd_IterateSources(jsdc, &iterp)) )
    {
        const char*     url;
        const char*     text;
        int             len;
        JSBool          dirty;
        JSDStreamStatus status;
        JSBool          gotSrc;

        url     = JSD_GetSourceURL(jsdc, jsdsrc);
        dirty   = JSD_IsSourceDirty(jsdc, jsdsrc);
        status  = JSD_GetSourceStatus(jsdc, jsdsrc);
        gotSrc  = JSD_GetSourceText(jsdc, jsdsrc, &text, &len );
        
        dummy = 0;  /* gives us a line to set breakpoint... */
    }
}
#else
#define DEBUG_ITERATE_SOURCES(x) ((void)x)
#endif

/***************************************************************************/

JSDSourceText*
jsd_NewSourceText(JSDContext* jsdc, const char* url)
{
    JSDSourceText* jsdsrc;
    const char* new_url_string;

    jsd_LockSourceTextSubsystem(jsdc);

    new_url_string = jsd_BuildNormalizedURL(url);
    if( ! new_url_string )
        return NULL;

    jsdsrc = jsd_FindSourceForURL(jsdc, new_url_string);

    if( jsdsrc )
        MoveSourceToRemovedList(jsdc, jsdsrc);

    jsdsrc = AddSource( jsdc, new_url_string );

    jsd_UnlockSourceTextSubsystem(jsdc);

    return jsdsrc;
}

JSDSourceText*
jsd_AppendSourceText(JSDContext* jsdc, 
                     JSDSourceText* jsdsrc,
                     const char* text,       /* *not* zero terminated */
                     size_t length,
                     JSDSourceStatus status)
{
    jsd_LockSourceTextSubsystem(jsdc);

    if( ! IsSourceInSourceList( jsdc, jsdsrc ) )
    {
        RemoveSourceFromRemovedList( jsdc, jsdsrc );
        jsd_UnlockSourceTextSubsystem(jsdc);
        return NULL;
    }

    if( text && length && ! AppendText( jsdc, jsdsrc, text, length ) )
    {
        jsdsrc->dirty  = JS_TRUE;
        jsdsrc->alterCount  = g_alterCount++ ;
        jsdsrc->status = JSD_SOURCE_FAILED;
        MoveSourceToRemovedList(jsdc, jsdsrc);
        jsd_UnlockSourceTextSubsystem(jsdc);
        return NULL;    
    }

    jsdsrc->dirty  = JS_TRUE;
    jsdsrc->alterCount  = g_alterCount++ ;
    jsdsrc->status = status;
    DEBUG_ITERATE_SOURCES(jsdc);
    jsd_UnlockSourceTextSubsystem(jsdc);
    return jsdsrc;
}

/***************************************************************************/

