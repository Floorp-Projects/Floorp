/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 *  Function naming conventions:
 *      NPL_   Functions exported to other libs or FEs (prototyped in np.h)
 *		NPN_   Function prototypes exported to plug-ins via our function table (prototyped in npapi.h)
 *      npn_   Function definitions whose addresses are placed in the function table 
 *      np_    Miscellaneous functions internal to libplugin (this file)
 *
 */

#include "xp.h"
#include "npglue.h"

#ifdef ANTHRAX
static char* np_FindAppletNForMimeType(const char* mimetype, char index);
static int32 np_GetNumberOfInstalledApplets(const char* mimetype);
static void np_ReplaceChars(char* word, char oldChar, char newChar);
static char* np_CreateMimePref(const char* mimetype, const char* pref);
#endif /* ANTHRAX */

#ifdef PLUGIN_TIMER_EVENT
static void np_SetTimerInterval(NPP npp, uint32 msecs);
static void np_TimerCallback(void* app);
#define DEFAULT_TIMER_INTERVAL 1000
#endif

/* list of all plugins */
static np_handle *np_plist = 0;

/* list of all applets for ANTHRAX */
#ifdef ANTHRAX
static np_handle *np_alist = NULL;
#endif

int np_debug = 0;

NPNetscapeFuncs npp_funcs;

/*
 * Determine whether or not this is a new-style plugin.
 */
static inline XP_Bool
np_is50StylePlugin(np_handle* handle)
{
    XP_ASSERT(handle != NULL);
    return (handle->userPlugin != NULL);
}

/* Find a mimetype in the handle */
static np_mimetype *
np_getmimetype(np_handle *handle, const char *mimeStr, XP_Bool wildCard)
{
	np_mimetype *mimetype;

	for (mimetype = handle->mimetypes; mimetype; mimetype = mimetype->next)
	{
		if (mimetype->enabled)
		{
			if ((wildCard && !strcmp(mimetype->type, "*")) ||
				!strcasecomp(mimetype->type, mimeStr))
				return (mimetype);
		}
	}
	return (NULL);
}
    

/*
 * Standard conversion from NetLib status
 * code to plug-in reason code.
 */
static NPReason
np_statusToReason(int status)
{
	if (status == MK_DATA_LOADED)
		return NPRES_DONE;
	else if (status == MK_INTERRUPTED)
		return NPRES_USER_BREAK;
	else
		return NPRES_NETWORK_ERR;
}


/*
 * Add a node to the list of URLs for this
 * plug-in instance.  The caller must fill
 * out the fields of the node, which is returned.
 */
static np_urlsnode*
np_addURLtoList(np_instance* instance)
{
	np_urlsnode* node;
	
	if (!instance)
		return NULL;
		
	if (!instance->url_list)
		instance->url_list = XP_ListNew();
	if (!instance->url_list)
		return NULL;
		
	node = XP_NEW_ZAP(np_urlsnode);
	if (!node)
		return NULL;
		
	XP_ListAddObject(instance->url_list, node);
	
	return node;
}


/*
 * Deal with one URL from the list of URLs for the instance
 * before the URL is removed from the list.  If the URL was
 * locked in the cache, we must unlock it and delete the URL.
 * Otherwise, notify the plug-in that the URL is done (if 
 * necessary) and break the reference from the URL to the 
 * instance. 
 */
static void
np_processURLNode(np_urlsnode* node, np_instance* instance, int status)
{
	if (node->cached)
	{
		/* Unlock the cache file */
		XP_ASSERT(!node->notify);
		if (node->urls)
		{
        	NET_ChangeCacheFileLock(node->urls, NP_UNLOCK);

        	NET_FreeURLStruct(node->urls);
        	node->urls = NULL;
        }
	    
	    return;
	}
	
	if (node->notify)
	{
		/* Notify the plug-in */
		XP_ASSERT(!node->cached);
        if (instance) {
            TRACEMSG(("npglue.c: CallNPP_URLNotifyProc"));
            if (np_is50StylePlugin(instance->handle)) {
                nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*)instance->npp->pdata;
                NPIPluginInstance* userInst = peerInst->GetUserInstance();
                userInst->URLNotify(node->urls->address, node->urls->window_target,
                                    (NPPluginReason)np_statusToReason(status),
                                    node->notifyData);
            }
            else if (ISFUNCPTR(instance->handle->f->urlnotify)) {
                CallNPP_URLNotifyProc(instance->handle->f->urlnotify,
                                      instance->npp,
                                      node->urls->address,
                                      np_statusToReason(status),
                                      node->notifyData);
            }
		}
	}
	
	/* Break the reference from the URL to the instance */
	if (node->urls)
		node->urls->fe_data = NULL;
}


/*
 * Remove an individual URL from the list of URLs for this instance.
 */
static void
np_removeURLfromList(np_instance* instance, URL_Struct* urls, int status)
{
    XP_List* url_list;
    np_urlsnode* node;

    if (!instance || !urls || !instance->url_list)
        return;

    /* Look for the URL in the list */
    url_list = instance->url_list;
    while ((node = (np_urlsnode*)XP_ListNextObject(url_list)) != NULL)
    {
        if (node->urls == urls)
        {
        	XP_ASSERT(!node->cached);
        	np_processURLNode(node, instance, status);

	        XP_ListRemoveObject(instance->url_list, node);
			XP_FREE(node);
			
	        /* If the list is now empty, delete it */
	        if (XP_ListCount(instance->url_list) == 0)
	        {
	            XP_ListDestroy(instance->url_list);
	            instance->url_list = NULL;
	        }

	        return;
        }
    }
}


/* 
 * Remove all URLs from the list of URLs for this instance.
 */
static void
np_removeAllURLsFromList(np_instance* instance)
{
	XP_List* url_list;
    np_urlsnode* node;

    if (!instance || !instance->url_list)
        return;
        
	url_list = instance->url_list;
	while ((node = (np_urlsnode*)XP_ListNextObject(url_list)) != NULL)
	{
		/* No notification of URLs now: the instance is going away */
		node->notify = FALSE;
		np_processURLNode(node, instance, 0);
	}

	/* Remove all elements from the list */
	url_list = instance->url_list;
    while ((node = (np_urlsnode*)XP_ListRemoveTopObject(url_list)) != NULL)
    	XP_FREE(node); 
        
	/* The list should now be empty, so delete it */
	XP_ASSERT(XP_ListCount(instance->url_list) == 0);
	XP_ListDestroy(instance->url_list);
	instance->url_list = NULL;
}



/* maps from a urls to the corresponding np_stream.  
   you might well ask why not just store the urls in the stream and
   put the stream in the netlib obj.  the reason is that there can be
   more than one active netlib stream for each plugin stream and
   netlib specific data for seekable streams (current position and
   so-on is stored in the urls and not in the stream.  what a travesty.
*/
static np_stream *
np_get_stream(URL_Struct *urls)
{
    if(urls)
    {
        NPEmbeddedApp *pEmbeddedApp = (NPEmbeddedApp*)urls->fe_data;
        if(pEmbeddedApp)
        {
            np_data *ndata = (np_data *)pEmbeddedApp->np_data;
            if(ndata && ndata->instance)
            {
                np_stream *stream;
                for (stream=ndata->instance->streams; stream; stream=stream->next)
                {
                	/*
                	 * Matching algorithm: Either this URL is the inital URL for
                	 * the stream, or it's a URL generated by a subsequent byte-
                	 * range request.  We don't bother to keep track of all the 
                	 * URLs for byte-range requests, but we can still detect if 
                	 * the URL matches this stream: since we know that this URL
                	 * belongs to one of the streams for this instance and that
                	 * there can only be one seekable stream for an instance
                	 * active at a time, then we know if this stream is seekable
                	 * and the URL is a byte-range URL then they must match.
                	 * NOTE: We check both urls->high_range and urls->range_header
                	 * because we could have been called before the range_header
                	 * has been parsed and the high_range set.
                	 */
                	if ((stream->initial_urls == urls) ||
                		(stream->seek && (urls->high_range || urls->range_header)))
                		return stream;
                }
            }
        }
    }
    return NULL;
}


/*
 * Create the two plug-in data structures we need to go along
 * with a netlib stream for a plugin: the np_stream, which is
 * private to libplugin, and the NPStream, which is exported
 * to the plug-in.  The NPStream has an opaque reference (ndata)
 * to the associated np_stream.  The np_stream as a reference
 * (pstream) to the NPStream, and another reference (nstream)
 * to the netlib stream.
 */
static np_stream*
np_makestreamobjects(np_instance* instance, NET_StreamClass* netstream, URL_Struct* urls)
{
	np_stream* stream;
	NPStream* pstream;
    XP_List* url_list;
    np_urlsnode* node;
    void*  notifyData;
	
	/* check params */
	if (!instance || !netstream || !urls)
		return NULL;
		
    /* make a npglue stream */
    stream = XP_NEW_ZAP(np_stream);
    if (!stream)
        return NULL;
    
    stream->instance = instance;
    stream->handle = instance->handle;
    stream->nstream = netstream;
    stream->initial_urls = urls;
    
    XP_ASSERT(urls->address);
    StrAllocCopy(stream->url, urls->address);
    stream->len = urls->content_length;

	/* Look for notification data for this URL */
	notifyData = NULL;
    url_list = instance->url_list;
    while ((node = (np_urlsnode*)XP_ListNextObject(url_list)) != NULL)
    {
        if (node->urls == urls && node->notify)
        {
        	notifyData = node->notifyData;
        	break;
        }
    }

    /* make a plugin stream (the thing the plugin sees) */
    pstream = XP_NEW_ZAP(NPStream);
    if (!pstream) 
    {
        XP_FREE(stream);
        return NULL;
    }
    pstream->ndata = stream;    /* confused yet? */
    pstream->url = stream->url;
    pstream->end = urls->content_length;
    pstream->lastmodified = (uint32) urls->last_modified;
	pstream->notifyData = notifyData;

    /* make a record of it */
    stream->pstream = pstream;
    stream->next = instance->streams;
    instance->streams = stream;
 
    NPTRACE(0,("np: new stream, %s, %s", instance->mimetype->type, urls->address));

	return stream;
}

/*
 * Do the reverse of np_makestreamobjects: delete the two plug-in
 * stream data structures (the NPStream and the np_stream).
 * We also need to remove the np_stream from the list of streams
 * associated with its instance.
 */
static void
np_deletestreamobjects(np_stream* stream)
{
	np_instance* instance = stream->instance;
	
    /* remove it from the instance list */
    if (stream == instance->streams)
        instance->streams = stream->next;
    else
    {
        np_stream* sx;
        for (sx = instance->streams; sx; sx = sx->next)
            if (sx->next == stream)
            {
                sx->next = sx->next->next;
                break;
            }
    }

    /* and nuke the stream */
    if (stream->pstream) 
    {
        XP_FREE(stream->pstream);
        stream->pstream = 0;
    }
    stream->handle = 0;
    XP_FREE(stream);
}           



/*
 * (a) There should be a delayed load LIST (multiple requests get lost currently!)
 * (b) We should call NET_GetURL with the app in the fe_data (notification uses it!)
 * (c) We need to store the target context (may be different than instance context!)
 */
static void
np_dofetch(URL_Struct *urls, int status, MWContext *window_id)
{
	np_stream *stream = np_get_stream(urls);
    if(stream && stream->instance)
    {
        FE_GetURL(stream->instance->cx, stream->instance->delayedload);
    }
}


unsigned int
NPL_WriteReady(NET_StreamClass *stream)
{
    URL_Struct *urls = (URL_Struct *)stream->data_object;
    np_stream *newstream = nil;
    int ret = 0;	

    if(!(newstream = np_get_stream(urls)))
        return 0;

	if (newstream->asfile == NP_ASFILEONLY)
		return NP_MAXREADY;

	/* if prev_stream is not ready to write, then neither is this one... */
	if (newstream->prev_stream != NULL){
		ret = (newstream->prev_stream->is_write_ready(newstream->prev_stream));
		if (ret == FALSE)
			return FALSE;
	}
		
    XP_ASSERT(newstream->instance);
    newstream->instance->reentrant = 1;

    if (newstream->seek >= 0) {
        TRACEMSG(("npglue.c: CallNPP_WriteReadyProc"));
        if (newstream->handle->userPlugin) {
            nsPluginStreamPeer* peerStream = (nsPluginStreamPeer*)newstream->pstream->pdata;
            NPIPluginStream* userStream = peerStream->GetUserStream();
            ret = userStream->WriteReady();
        }
        else if (ISFUNCPTR(newstream->handle->f->writeready)) {
            ret = CallNPP_WriteReadyProc(newstream->handle->f->writeready,
                                         newstream->instance->npp, newstream->pstream);
        }
    }
    else
        ret = NP_MAXREADY;

#if defined(XP_WIN) || defined(XP_OS2)
	/* Prevent WinFE from going to sleep when plug-in blocks */
	if (ret == 0){
	  if(!newstream->instance->calling_netlib_all_the_time){
		newstream->instance->calling_netlib_all_the_time = TRUE;
		NET_SetCallNetlibAllTheTime(newstream->instance->cx, "npglue");
	  }
	  else{

		NET_ClearCallNetlibAllTheTime(newstream->instance->cx, "npglue");
		newstream->instance->calling_netlib_all_the_time = FALSE;
	  }
	}
#endif
    if(!newstream->instance->reentrant)
    {
        urls->pre_exit_fn = np_dofetch;
        return (unsigned int)-1;
    }
    newstream->instance->reentrant = 0;

    return ret;
}

int
NPL_Write(NET_StreamClass *stream, const unsigned char *str, int32 len)
{
    int ret;
    URL_Struct *urls = (URL_Struct *)stream->data_object;
    np_stream *newstream = np_get_stream(urls);	

    if(!newstream || !(newstream->handle->userPlugin ? 1 : ISFUNCPTR(newstream->handle->f->write)))
        return -1;

	if (newstream->asfile == NP_ASFILEONLY)
		return len;

	if (newstream->prev_stream != NULL){
		ret = newstream->prev_stream->put_block(newstream->prev_stream,(const char *)str,len); 
		/* should put check here for ret == len?  if not, then what? */
	}


    /* if this is the first non seek write after we turned the
       stream, then abort this (netlib) stream */

    if(!urls->high_range && (newstream->seek == -1))
        return MK_UNABLE_TO_CONVERT; /* will cause an abort */

    XP_ASSERT(newstream->instance);
    newstream->instance->reentrant = 1;

    newstream->pstream->end = urls->content_length;

    if(newstream->seek)
    {
        /* NPTRACE(0,("seek stream gets %d bytes with high %d low %d pos %d\n", len, 
           urls->high_range, urls->low_range, urls->position)); */
        /* since we get one range per urls, position will only be non-zero
           if we are getting additional writes */
        if(urls->position == 0)
            urls->position = urls->low_range;
    }
    /*TRACEMSG(("npglue.c: CallNPP_WriteProc"));
    */
    if (newstream->handle->userPlugin) {
        nsPluginStreamPeer* peerStream = (nsPluginStreamPeer*)newstream->pstream->pdata;
        NPIPluginStream* userStream = peerStream->GetUserStream();
        ret = userStream->Write(len, (const char*)str);
    }
    else if (ISFUNCPTR(newstream->handle->f->write)) {
        ret = CallNPP_WriteProc(newstream->handle->f->write, newstream->instance->npp, newstream->pstream, 
                                urls->position, len, (void *)str);
    }

    urls->position += len;

    if(!newstream->instance->reentrant)
    {
        urls->pre_exit_fn = np_dofetch;
        return -1;
    }
    newstream->instance->reentrant = 0;

    return ret;
}


static char *
np_make_byterange_string(NPByteRange *ranges)
{
    char *burl;
    NPByteRange *br;
    int count, maxlen;

    for(count=0, br=ranges; br; br=br->next)
        count++;
    maxlen = count*22 + 64; /* two 32-bit numbers, plus slop */

    burl = (char*)XP_ALLOC(maxlen);
    if(burl)
    {    
        char range[64];
        int len=0;
        int iBytesEqualsLen;

        iBytesEqualsLen = strlen(RANGE_EQUALS);

	    /* the range must begin with bytes=
	    * a final range string looks like:
	    *  bytes=5-8,12-56
	    */
	    XP_STRCPY(burl, RANGE_EQUALS);

        for(br=ranges; br; br=br->next)
        {
            int32 brlen = br->length;
            if(len)
                XP_STRCAT(burl, ",");
            if(br->offset<0)
                sprintf(range, "%ld", -((long)(br->length)));
            else
                sprintf(range, "%ld-%ld", br->offset, (br->offset+(brlen-1)));

            len += XP_STRLEN(range);
            if((len + iBytesEqualsLen) >= maxlen)
                break;
            XP_STRCAT(burl, range);
        }

        if(len == 0) /* no ranges added */
            *burl = 0;
    }
    return burl;
}

static NPByteRange *
np_copy_rangelist(NPByteRange *rangeList)
{
    NPByteRange *r, *rn, *rl=0, *rh=0;

    for(r=rangeList; r; r=r->next)
    {
        if(!(rn = XP_NEW_ZAP(NPByteRange)))
            break;
        rn->offset = r->offset;
        rn->length = r->length;

        if(!rh) 
            rh = rn;
        if(rl) 
            rl->next = rn;
        rl = rn;
    }
    return rh;
}


static void
np_lock(np_stream *stream)
{
    if(!stream->islocked)
    {
        NET_ChangeCacheFileLock(stream->initial_urls, NP_LOCK);
        stream->islocked = 1;
    }
}

static void
np_unlock(np_stream *stream)
{
    if(stream->islocked)
    {
        NET_ChangeCacheFileLock(stream->initial_urls, NP_UNLOCK);
        stream->islocked = 0;
    }
}

static void 
np_GetURL(URL_Struct *pURL,FO_Present_Types iFormatOut,MWContext *cx,   Net_GetUrlExitFunc *pExitFunc, NPBool notify){

	XP_ASSERT(pURL->owner_data == NULL);
	pURL->owner_id   = 0x0000BAC0; /* plugin's unique identifier */
	pURL->owner_data = pURL->fe_data;
	pURL->fe_data = NULL;
	FE_GetURL(cx,pURL);
}



NPError NP_EXPORT
npn_requestread(NPStream *pstream, NPByteRange *rangeList)
{
    np_stream *stream;

    if (pstream == NULL || rangeList == NULL)
        return NPERR_INVALID_PARAM;

    stream = (np_stream *)pstream->ndata;

    if (stream == NULL)
        return NPERR_INVALID_PARAM;

    /* requestread may be called before newstream has returned */
    if (stream)
    {
        if (!stream->seekable)
        {
	    	/*
	    	 * If the stream is not seekable, we can only fulfill
	    	 * the request if we're going to cache it (seek == 2);
	    	 * otherwise it's an error.   If we are caching it,
	    	 * the request must still wait until the entire file
	    	 * is cached (when NPL_Complete is finally called).
	    	 */
        	if (stream->seek == 2)
        	{
				/* defer the work */
	            NPTRACE(0,("\tdeferred the request"));

	            if(!stream->deferred)
	                stream->deferred = np_copy_rangelist(rangeList);
	            else
	            {
	                NPByteRange *r;
	                /* append */
	                for(r=stream->deferred;r && r->next;r++)
	                    ;
	                if(r)
	                {
	                    XP_ASSERT(!r->next);
	                    r->next = np_copy_rangelist(rangeList);
	                }
	            }
	    	}
	    	else
	    		return NPERR_STREAM_NOT_SEEKABLE;
        }
        else
        {
            char *ranges;

            /* if seeking is ok, we delay this abort until now to get
               the most out of the existing connection */
            if(!stream->seek)
                stream->seek = -1; /* next write aborts */

            if ((ranges = np_make_byterange_string(rangeList)) != NULL)
            {
                URL_Struct *urls;
                urls = NET_CreateURLStruct(stream->url, NET_DONT_RELOAD);
                urls->range_header = ranges;
                XP_ASSERT(stream->instance);
                if(stream->instance)
                {
                    urls->fe_data = (void *)stream->instance->app;
                    (void) NET_GetURL(urls, FO_CACHE_AND_BYTERANGE, stream->instance->cx, NPL_URLExit);
                }
            }
        }
    }
    
    return NPERR_NO_ERROR;
}


static void
np_destroystream(np_stream *stream, NPError reason)
{
    if (stream)
    {
    	/* Tell the plug-in that the stream is going away, and why */
        np_instance *instance = stream->instance;
        TRACEMSG(("npglue.c: CallNPP_DestroyStreamProc"));
        if (stream->handle->userPlugin) {
            nsPluginStreamPeer* peerStream = (nsPluginStreamPeer*)stream->pstream->pdata;
            NPIPluginStream* userStream = peerStream->GetUserStream();
            peerStream->SetReason((NPPluginReason)reason);
            userStream->Release();      // must be released before peer
            peerStream->Release();
        }
        else if (ISFUNCPTR(stream->handle->f->destroystream)) {
            CallNPP_DestroyStreamProc(stream->handle->f->destroystream, instance->npp, stream->pstream, reason);
        }
        
		/* Delete the np_stream and associated NPStream objects */
		np_deletestreamobjects(stream);
    }           
}


void
np_streamAsFile(np_stream* stream)
{
    char* fname = NULL;
	XP_ASSERT(stream->asfile);

    if (stream->initial_urls)
    {
#ifdef XP_MAC
		/* XXX - we should eventually do this for all platforms, but when I tested it with
		PC, I encountered an issue w/ EXE files just before shipping, so we're only
		checking this in for MAC.  */
		if (NET_IsURLInDiskCache(stream->initial_urls))
#else
		if (stream->initial_urls->cache_file)
#endif
        {
        	np_urlsnode* node;
        	
			/* paranoia check for ifdef mac change above */
			XP_ASSERT(stream->initial_urls->cache_file != NULL);

            fname = WH_FileName(stream->initial_urls->cache_file, xpCache);

			/* Lock the file in the cache until we're done with it */
			np_lock(stream);
			node = np_addURLtoList(stream->instance);
            if (node)
            {
                /* make a copy of the urls */
                URL_Struct* newurls = NET_CreateURLStruct(stream->initial_urls->address, NET_DONT_RELOAD);

                /* add the struct to the node */
                node->urls = newurls;
                node->cached = TRUE;
            }
        }
		else if (NET_IsLocalFileURL(stream->initial_urls->address))
		{
			char* pathPart = NET_ParseURL(stream->initial_urls->address, GET_PATH_PART);
            fname = WH_FileName(pathPart, xpURL);
			XP_FREE(pathPart);
		}
			
	}
	
    /* fname can be NULL if something went wrong */
    TRACEMSG(("npglue.c: CallNPP_StreamAsFileProc"));
    if (stream->handle->userPlugin) {
        nsPluginStreamPeer* peerStream = (nsPluginStreamPeer*)stream->pstream->pdata;
        NPIPluginStream* userStream = peerStream->GetUserStream();
        userStream->AsFile(fname);
    }
    else if (ISFUNCPTR(stream->handle->f->asfile)) {
        CallNPP_StreamAsFileProc(stream->handle->f->asfile, stream->instance->npp,
                                 stream->pstream, fname);
    }

	if (fname) XP_FREE(fname);
}


void
NPL_Complete(NET_StreamClass *stream)
{
    URL_Struct *urls = (URL_Struct *)stream->data_object;
    np_stream *newstream = nil;	

    if(!(newstream = np_get_stream(urls)))
        return;

	if (newstream->prev_stream != NULL)
		newstream->prev_stream->complete(newstream->prev_stream) ;

    if(newstream->seek)
    {
        if(newstream->seek == 2)
        {
            /* request all the outstanding reads that had been waiting */
            newstream->seekable = 1; /* for cgi hack */
            newstream->seek = 1;
            np_lock(newstream);
            npn_requestread(newstream->pstream, newstream->deferred);
            /* and delete the copies we made */
            {
                NPByteRange *r, *rl=0;
                for(r=newstream->deferred; r; rl=r, r=r->next)
                    if(rl) XP_FREE(rl);
                if(rl) XP_FREE(rl);
                newstream->deferred = 0;
            }
            np_unlock(newstream);
        }
    }

    if (newstream->asfile)
		np_streamAsFile(newstream);
		
    newstream->nstream = NULL;		/* Remove reference to netlib stream */

	newstream->prev_stream = NULL;

    if (!newstream->dontclose)
        np_destroystream(newstream, NPRES_DONE);
}


void 
NPL_Abort(NET_StreamClass *stream, int status)
{
    URL_Struct *urls = (URL_Struct *)stream->data_object;
    np_stream *newstream = nil;	

    if(!(newstream = np_get_stream(urls)))
        return;

	if (newstream->prev_stream != NULL)
		newstream->prev_stream->abort(newstream->prev_stream, status);

    if(newstream->seek == -1)
    {
        /* this stream is being turned around */
        newstream->seek = 1;
    }

    newstream->nstream = NULL;		/* Remove reference to netlib stream */

	/*
	 * MK_UNABLE_TO_CONVERT is the special status code we
	 * return from NPL_Write to cancel the original netlib
	 * stream when we get a byte-range request, so we don't
	 * want to destroy the plug-in stream in this case (we
	 * shouldn't get this status code any other time here).
	 */
    if (!newstream->dontclose || (status < 0 && status != MK_UNABLE_TO_CONVERT))
        np_destroystream(newstream, np_statusToReason(status));
}

extern XP_Bool
NPL_HandleURL(MWContext *cx, FO_Present_Types iFormatOut, URL_Struct *pURL, Net_GetUrlExitFunc *pExitFunc)
{
    /* check the cx for takers */
    return FALSE;
}


/*
 * This exit routine is called for embed streams (the
 * initial stream created when the plug-in is instantiated).
 * We use a special exit routine in this case because FE's
 * may want to take additional action when a plug-in stream
 * finishes (e.g. show special error status indicating why
 * the stream failed).
 */
/* This needs to have all the code in NPL_URLExit in it too! (notification) */
void
NPL_EmbedURLExit(URL_Struct *urls, int status, MWContext *cx)
{
	if (urls && status != MK_CHANGING_CONTEXT)
    {
#if defined(XP_WIN) || defined(XP_OS2)
		/* WinFE is responsible for deleting the URL_Struct */   
	    FE_EmbedURLExit(urls, status, cx);
#else
        NET_FreeURLStruct (urls);
#endif
    }
}

/* 
 * This exit routine is used for all streams requested by the
 * plug-in: byterange request streams, NPN_GetURL streams, and 
 * NPN_PostURL streams.  NOTE: If the exit routine gets called
 * in the course of a context switch, we must NOT delete the
 * URL_Struct.  Example: FTP post with result from server
 * displayed in new window -- the exit routine will be called
 * when the upload completes, but before the new context to
 * display the result is created, since the display of the
 * results in the new context gets its own completion routine.
 */
void
NPL_URLExit(URL_Struct *urls, int status, MWContext *cx)
{
    if (urls && status != MK_CHANGING_CONTEXT)
    {
		NPEmbeddedApp* app;
		np_stream* pstream;
		/* (part 2 of fix:  replace fe_data the way I expect it) */
		if ((urls->owner_data != NULL) &&
			(urls->owner_id == 0x0000BAC0))
				urls->fe_data = urls->owner_data;

        app = (NPEmbeddedApp*) urls->fe_data;
    	pstream = np_get_stream(urls);

    	if (pstream)
    	{
			/*
			 * MK_UNABLE_TO_CONVERT is the special status code we
			 * return from NPL_Write to cancel the original netlib
			 * stream when we get a byte-range request, so we don't
			 * want to destroy the plug-in stream in this case (we
			 * shouldn't get this status code any other time here).
			 */
    		if (!pstream->dontclose || (status < 0 && status != MK_UNABLE_TO_CONVERT))
	    		np_destroystream(pstream, np_statusToReason(status));
	    		
	    	/*
	    	 * If the initial URL_Struct is being deleted, break our
	    	 * reference to it (we might need to unlock it, too).
	    	 */
	    	if (pstream->initial_urls == urls)
	    	{
	    		np_unlock(pstream);
	    		pstream->initial_urls = NULL;
	    	}
	    }

		/*
		 * Check to see if the instance wants
		 * to be notified of the URL completion.
		 */
        if (app)
        {
            np_data* ndata = (np_data*) app->np_data;
            if (ndata && ndata->instance)
            	np_removeURLfromList(ndata->instance, urls, status);
		}
		if (urls->owner_data == NULL)
			NET_FreeURLStruct(urls);
		
    }
}
 
             

static URL_Struct*
np_makeurlstruct(np_instance* instance, const char* relativeURL,
                 const char* altHost, const char* referrer)
{
	History_entry* history;   
	URL_Struct* temp_urls = NULL;
    char* absoluteURL = NULL;
    URL_Struct* urls = NULL;   
	
	if (!instance || !relativeURL)
		return NULL;
		
    /*
     * Convert the (possibly) relative URL passed in by the plug-in
     * to a guaranteed absolute URL.  To do this we need the base
     * URL of the page we're on, which we can get from the history
     * info in the plug-in's context.
     */
    XP_ASSERT(instance->cx);
   	history = SHIST_GetCurrent(&instance->cx->hist);
   	if (history)
   	    temp_urls = SHIST_CreateURLStructFromHistoryEntry(instance->cx, history);
   	if (temp_urls)
   	{
   		absoluteURL = NET_MakeAbsoluteURL(temp_urls->address, (char*) relativeURL);
		NET_FreeURLStruct(temp_urls);
	}


	/*
	 * Now that we've got the absolute URL string, make a NetLib struct
	 * for it. If something went wrong making the absolute URL, fall back
	 * on the relative one.
	 */
	XP_ASSERT(absoluteURL);
   	if (absoluteURL)
   	{
    	urls = NET_CreateURLStruct(absoluteURL, NET_NORMAL_RELOAD);
	    XP_FREE(absoluteURL);
	}
    else
        urls = NET_CreateURLStruct(relativeURL, NET_NORMAL_RELOAD);

	urls->owner_data = NULL;
	urls->owner_id   = 0x0000BAC0;

    if (altHost && NET_SetURLIPAddressString(urls, altHost)) {
		NET_FreeURLStruct(urls);
        return NULL;
    }
    if (referrer) {
        urls->referer = XP_STRDUP((char*)referrer);
    }
	return urls;
}


static MWContext*
np_makecontext(np_instance* instance, const char* window)
{
	MWContext* cx;
	
    /* Figure out which context to do this on */
    if ((!strcmp(window, "_self")) || (!strcmp(window, "_current")))
        cx = instance->cx;
    else
        cx = XP_FindNamedContextInList(instance->cx, (char*) window);

    /* If we didn't find a context, make a new one */
    if (!cx)
        cx = FE_MakeNewWindow(instance->cx, NULL, (char*) window, NULL);
        
	return cx;
}

PR_STATIC_CALLBACK(void)
np_redisable_js(URL_Struct* url_s, int status, MWContext* context)
{
	context->forceJSEnabled = PR_FALSE;
}

NPError
np_geturlinternal(NPP npp, const char* relativeURL, const char* target, 
                  const char* altHost, const char* referrer, PRBool forceJSEnabled,
                  NPBool notify, void* notifyData)
{
    URL_Struct* urls = NULL;   
    MWContext* cx;
    np_instance* instance;
    np_urlsnode* node = NULL;
    NPError err = NPERR_NO_ERROR;
#ifdef XP_WIN32
	void*	pPrevState;
#endif
    
    if (!npp || !relativeURL)		/* OK for window to be NULL */
    	return NPERR_INVALID_PARAM;
    	
    instance = (np_instance*) npp->ndata;
    if (!instance)
        return NPERR_INVALID_PARAM;

    /* Make an abolute URL struct from the (possibly) relative URL passed in */
    urls = np_makeurlstruct(instance, relativeURL, altHost, referrer);
    if (!urls)
    {
    	err = NPERR_OUT_OF_MEMORY_ERROR;
    	goto error;
 	}
 
 	/*
 	 * Add this URL to the list of URLs for this instance,
 	 * and remember if the instance would like notification.
 	 */
	node = np_addURLtoList(instance);
	if (node)
	{
 	 	node->urls = urls;
 	 	if (notify)
 	 	{
			node->notify = TRUE;
			node->notifyData = notifyData;
		}
	}
	else
	{
    	err = NPERR_OUT_OF_MEMORY_ERROR;
    	goto error;
	}

	urls->fe_data = (void*) instance->app;
 	
    /*
     * If the plug-in passed NULL for the target, load the URL with a special stream
     * that will deliver the data to the plug-in; otherwise, convert the target name
     * they passed in to a context and load the URL into that context (possibly unloading
     * the plug-in in the process, if the target context is the plug-in's context).
     */
    if (!target)
	{
#ifdef XP_WIN32
		pPrevState = WFE_BeginSetModuleState();
#endif
        (void) NET_GetURL(urls, FO_CACHE_AND_PLUGIN, instance->cx, NPL_URLExit);
#ifdef XP_WIN32
		WFE_EndSetModuleState(pPrevState);
#endif
    }
	else
    {
    	cx = np_makecontext(instance, target);
	    if (!cx)
	    {
	    	err = NPERR_OUT_OF_MEMORY_ERROR;
	    	goto error;
	    }

		/* 
		 * Prevent loading "about:" URLs into the plug-in's context: NET_GetURL
		 * for these URLs will complete immediately, and the new layout thus
		 * generated will blow away the plug-in and possibly unload its code,
		 * causing us to crash when we return from this function.
		 */
		if (cx == instance->cx && NET_URL_Type(urls->address) == ABOUT_TYPE_URL)
		{
			err = NPERR_INVALID_URL;
			goto error;
		}

        if (forceJSEnabled && !cx->forceJSEnabled) {
            LM_ForceJSEnabled(cx);
            urls->pre_exit_fn = np_redisable_js;
        }
		
#ifdef XP_MAC
		/*
		 * One day the code below should call FE_GetURL, and this call will be
		 * unnecessary since the FE will do the right thing.  Right now (3.0b6)
		 * starting to use FE_GetURL is not an option so we'll just create 
		 * (yet another) FE callback for our purposes: we need to ask the FE
		 * to reset any timer it might have (for META REFRESH) so that the 
		 * timer doesn't go off after leaving the original page via plug-in
		 * request.
		 */
		FE_ResetRefreshURLTimer(cx);
#endif

        /* reentrancy matters for this case because it will cause the current
           stream to be unloaded which netlib can't deal with */
        if (instance->reentrant && (cx == instance->cx))
        {
        	XP_ASSERT(instance->delayedload == NULL);	/* We lose queued requests if this is non-NULL! */
            if (instance->delayedload)
                NET_FreeURLStruct(instance->delayedload);
            instance->delayedload = urls;
            instance->reentrant = 0;
        }
        else
		{
		if ((cx == instance->cx) || 
			(XP_IsChildContext(cx,instance->cx)) )
		{
			/* re-target:  use this until figure out why thread violation.
			this method obviates question of self-trouncing... */
			if   ((instance->cx->type == MWContextBrowser ||
			    instance->cx->type == MWContextPane) 
				&&  ((XP_STRNCMP(urls->address, "mailbox:", 8)==0)  
	            || (XP_STRNCMP(urls->address, "mailto:" , 7)==0)
	            || (XP_STRNCMP(urls->address, "news:"   , 5)==0)))
					cx = np_makecontext(instance,"_self");
			else{
				/* Since the previous stuff generally worked for this, keep using it */
 				urls->fe_data = NULL;
#ifdef XP_WIN32
				pPrevState = WFE_BeginSetModuleState();
#endif
				/* clear the exit routine, since the recipient may not be present! */
				(void) NET_GetURL(urls, FO_CACHE_AND_PRESENT, cx, NPL_URLExit);
#ifdef XP_WIN32
				WFE_EndSetModuleState(pPrevState);
#endif
				
				return NPERR_NO_ERROR;  
			}
			/* Eventually, we should shut down the current instance and
			startup a new one */
		}
#ifdef XP_WIN32
			pPrevState = WFE_BeginSetModuleState();
#endif

			(void) np_GetURL(urls, FO_CACHE_AND_PRESENT, cx, NPL_URLExit,notify); 

#ifdef XP_WIN32
			WFE_EndSetModuleState(pPrevState);
#endif
		}
    }

    return NPERR_NO_ERROR;
    
error:
	if (node)
	{
		node->notify = FALSE;		/* Remove node without notification */
		np_removeURLfromList(instance, urls, 0);
	}
	if (urls)
		NET_FreeURLStruct(urls);
	return err;
}


static NPError
np_parsepostbuffer(URL_Struct* urls, const char* buf, uint32 len)
{
	/*
	 * Search the buffer passed in for a /n/n. If we find it, break the
	 * buffer in half: the first part is the header, the rest is the body.
	 */
	uint32 index;
	for (index = 0; index < len; index++)
	{
		if (buf[index] == '\n' && ++index < len && buf[index] == '\n')
			break;
	}
	
	/*
	 * If we found '\n\n' somewhere in the middle of the string then we 
	 * have headers, so we need to allocate a new string for the headers,
	 * copy the header data from the plug-in's buffer into it, and put
	 * it in the appropriate place of the URL struct.
	 */
	if (index > 1 && index < len)
		{
		uint32 headerLength = index;
		char* headers = (char*) XP_ALLOC(headerLength + 1);
	    if (!headers)
	    	return NPERR_OUT_OF_MEMORY_ERROR;
		XP_MEMCPY(headers, buf, headerLength);
		headers[headerLength] = 0;
		urls->post_headers = headers;
		}
	
	/*
	 * If we didn't find '\n\n', then the body starts at the beginning;
	 * otherwise, it starts right after the second '\n'.  Make sure the
	 * body is non-emtpy, allocate a new string for it, copy the data
	 * from the plug-in's buffer, and put it in the URL struct.
	 */
	if (index >= len) 
		index = 0;								/* No '\n\n', start body from beginning */
	else
		index++;								/* Found '\n\n', start body after it */
		
	if (len - index > 0)						/* Non-empty body? */
	{
		uint32 bodyLength = len - index + 1;
		char* body = (char*) XP_ALLOC(bodyLength);
	    if (!body)
	    	return NPERR_OUT_OF_MEMORY_ERROR;
		XP_MEMCPY(body, &(buf[index]), bodyLength);
	    urls->post_data = body;
	    urls->post_data_size = bodyLength;
		urls->post_data_is_file = FALSE;
	}	
	else
	{
		/* Uh-oh, no data to post */
		return NPERR_NO_DATA;
	}
	
	return NPERR_NO_ERROR;
}


NPError
np_posturlinternal(NPP npp, const char* relativeURL, const char *target, 
                   const char* altHost, const char* referrer, PRBool forceJSEnabled,
                   uint32 len, const char *buf, NPBool file, NPBool notify, void* notifyData)
{
    np_instance* instance;
    URL_Struct* urls = NULL; 
    char* filename = NULL; 
    XP_Bool ftp;
    np_urlsnode* node = NULL;
    NPError err = NPERR_NO_ERROR;
#ifdef XP_WIN32
	void*	pPrevState;
#endif
    
    /* Validate paramters */
    if (!npp || !relativeURL)
    	return NPERR_INVALID_PARAM;

    instance = (np_instance*) npp->ndata;
    if (!instance)
        return NPERR_INVALID_PARAM;
   
    /* Make an absolute URL struct from the (possibly) relative URL passed in */
    urls = np_makeurlstruct(instance, relativeURL, altHost, referrer);
    if (!urls)
    	return NPERR_INVALID_URL;


 	/*
 	 * Add this URL to the list of URLs for this instance,
 	 * and remember if the instance would like notification.
 	 */
	node = np_addURLtoList(instance);
	if (node)
	{
 	 	node->urls = urls;
 	 	if (notify)
 	 	{
			node->notify = TRUE;
			node->notifyData = notifyData;
		}
	}
	else
		return NPERR_OUT_OF_MEMORY_ERROR;
 	
	/* 
	 * FTP protocol requires that the data be in a file.
	 * If we really wanted to, we could write code to dump the buffer to
	 * a temporary file, give the temp file to netlib, and delete it when
	 * the exit routine fires.
	 */
	ftp = (strncasecomp(urls->address, "ftp:", 4) == 0);
	if (ftp && !file)
	{
		err = NPERR_INVALID_URL;		
		goto error;				
	}
	
	if (file)
	{
		XP_StatStruct stat;
		
		/* If the plug-in passed a file URL, strip the 'file://' */
		if (!strncasecomp(buf, "file://", 7))
			filename = XP_STRDUP((char*) buf + 7);
		else
			filename = XP_STRDUP((char*) buf);

		if (!filename)
		{
			err = NPERR_OUT_OF_MEMORY_ERROR;
			goto error;
		}
		
		/* If the file doesn't exist, return an error NOW before netlib get it */
		if (XP_Stat(filename, &stat, xpURL))
		{
			err = NPERR_FILE_NOT_FOUND;
			goto error;
		}
	}
	
	/*
	 * NET_GetURL handles FTP posts differently: the post_data fields are
	 * ignored; instead, files_to_post contains an array of the files.
	 */
	if (ftp)
	{
		XP_ASSERT(filename);
		urls->files_to_post = (char**) XP_ALLOC(sizeof(char*) + sizeof(char*));
		if (!(urls->files_to_post))
		{
			err = NPERR_OUT_OF_MEMORY_ERROR;
			goto error;
		}
		urls->files_to_post[0] = filename;
		urls->files_to_post[1] = NULL;
	    urls->post_data = NULL;
	    urls->post_data_size = 0;
	    urls->post_data_is_file = FALSE;
	}
	else if (file)
	{
		XP_ASSERT(filename);
	    urls->post_data = filename;
	    urls->post_data_size = XP_STRLEN(filename);
	    urls->post_data_is_file = TRUE;
	}
	else
	{
		/* 
		 * There are two different sets of buffer-parsing code.
		 * The new code is contained within np_parsepostbuffer,
		 * and is used when the plug-in calls NPN_PostURLNotify.
		 * The old code, below, is preserved for compatibility
		 * for when the plug-in calls NPN_PostURL.
		 */
		if (notify)
		{
			NPError err = np_parsepostbuffer(urls, buf, len);
			if (err != NPERR_NO_ERROR)
				goto error;
		}
		else
		{
			urls->post_data = (char*)XP_ALLOC(len);
			if (!urls->post_data)
			{
				err = NPERR_OUT_OF_MEMORY_ERROR;
				goto error;
			}
			XP_MEMCPY(urls->post_data, buf, len);
		    urls->post_data_size = len;
			urls->post_data_is_file = FALSE;
		}
	}
	
    urls->method = URL_POST_METHOD;

    if (!target)
    {
        urls->fe_data = (void*) instance->app;
#ifdef XP_WIN32
		pPrevState = WFE_BeginSetModuleState();
#endif
        (void) NET_GetURL(urls, FO_CACHE_AND_PLUGIN, instance->cx, NPL_URLExit);
#ifdef XP_WIN32
		WFE_EndSetModuleState(pPrevState);
#endif
    }
    else
    {
    	MWContext* cx = np_makecontext(instance, target);
	    if (!cx)
		{
			err = NPERR_OUT_OF_MEMORY_ERROR;
			goto error;
		}
        urls->fe_data = (void*) instance->app;

        if (forceJSEnabled && !cx->forceJSEnabled) {
            LM_ForceJSEnabled(cx);
            urls->pre_exit_fn = np_redisable_js;
        }
		
#ifdef XP_MAC
		/*
		 * One day the code below should call FE_GetURL, and this call will be
		 * unnecessary since the FE will do the right thing.  Right now (3.0b6)
		 * starting to use FE_GetURL is not an option so we'll just create 
		 * (yet another) FE callback for our purposes: we need to ask the FE
		 * to reset any timer it might have (for META REFRESH) so that the 
		 * timer doesn't go off after leaving the original page via plug-in
		 * request.
		 */
		FE_ResetRefreshURLTimer(cx);
#endif

#ifdef XP_WIN32
		pPrevState = WFE_BeginSetModuleState();
#endif
			(void) np_GetURL(urls, FO_CACHE_AND_PRESENT, cx, NPL_URLExit,notify); 

#ifdef XP_WIN32
		WFE_EndSetModuleState(pPrevState);
#endif
	}
	
    return NPERR_NO_ERROR;
    
error:
	if (node)
	{
		node->notify = FALSE;		/* Remove node without notification */
		np_removeURLfromList(instance, urls, 0);
	}
	if (urls)
		NET_FreeURLStruct(urls);
	return err;
}



NPError NP_EXPORT
npn_geturlnotify(NPP npp, const char* relativeURL, const char* target, void* notifyData)
{
	return np_geturlinternal(npp, relativeURL, target, NULL, NULL, PR_FALSE, TRUE, notifyData);
}

NPError NP_EXPORT
npn_getvalue(NPP npp, NPNVariable variable, void *r_value)
{
    np_instance* instance;
	NPError ret = NPERR_NO_ERROR;

	if (r_value == NULL)
		return NPERR_INVALID_PARAM;

	/* Some of these variabled may be handled by backend. The rest is FE.
	 * So Handle all the backend variables and pass the rest over to FE.
	 */

	switch(variable) {
		case NPNVjavascriptEnabledBool : 
			ret = PREF_GetBoolPref("javascript.enabled", (XP_Bool*)r_value);	
			break;
		case NPNVasdEnabledBool :
			ret = PREF_GetBoolPref("autoupdate.enabled", (XP_Bool*)r_value);
			break;
#ifdef MOZ_OFFLINE        
		case NPNVisOfflineBool :{
			XP_Bool *bptr = (XP_Bool *)r_value; 
			*bptr = NET_IsOffline();
			ret = NPERR_NO_ERROR;
			break;		}
#endif /* MOZ_OFFLINE */
		default:
			instance = NULL;
			if (npp != NULL) {
	    		instance = (np_instance*) npp->ndata;
			}
#ifdef XP_UNIX
			ret = FE_PluginGetValue(instance?instance->handle->pdesc:NULL,
									variable, r_value);
#else
            ret = FE_PluginGetValue(instance->cx, instance->app, variable,
                                    r_value);
#endif /* XP_UNIX */
	}

	return(ret);
}

NPError NP_EXPORT
npn_setvalue(NPP npp, NPPVariable variable, void *r_value)
{
    np_instance* instance = NULL;
	NPError ret = NPERR_NO_ERROR;
    
    if (npp != NULL) {
        instance = (np_instance*) npp->ndata;
    }
    
    if (!instance)
        return NPERR_INVALID_INSTANCE_ERROR;
    
    switch(variable) {
        case NPPVpluginWindowBool:
        /* 
         * XXX On the Mac, a window has already been allocated by the time NPP_New
         * has been called - which is fine, since we'll still use the window. 
         * Unfortunately, we can't use the window's presence to determine whether
         * it's too late to set the windowed property.
         */
#ifndef XP_MAC
            /* 
             * If the window has already been allocated, it's too late
             * to tell us.
             */
            if (!instance->app->wdata->window)
                instance->windowed = (0 != r_value);
            else
                ret = NPERR_INVALID_PARAM;
#else
			instance->windowed = (0 != r_value);
#endif 
            break;
        case NPPVpluginTransparentBool:
            instance->transparent = (0 != r_value);
#ifdef LAYERS
            if (instance->layer && 
            	(instance->transparent != !(CL_GetLayerFlags(instance->layer) & CL_OPAQUE)))
            {
            	XP_Rect bbox;
            	
            	/* Get the bbox and convert it into its own coordinate space */
            	CL_GetLayerBbox(instance->layer, &bbox);
            	
                CL_ChangeLayerFlag(instance->layer, CL_OPAQUE, (PRBool)!instance->transparent);
                CL_ChangeLayerFlag(instance->layer, 
                                   CL_PREFER_DRAW_OFFSCREEN,
                                   (PRBool)instance->transparent);

				/* Force drawing of the entire transparent plug-in. */
                CL_UpdateLayerRect(CL_GetLayerCompositor(instance->layer),
                				   instance->layer, &bbox, PR_FALSE);
             }
#endif /* LAYERS */            
            break;
            
        case NPPVpluginTimerInterval:
        	np_SetTimerInterval(npp, *(uint32*)r_value);
        	break; 

        case NPPVpluginWindowSize:
          break;
	    default:
		  break;
    }

    return(ret);
}


NPError NP_EXPORT
npn_geturl(NPP npp, const char* relativeURL, const char* target)
{
	return np_geturlinternal(npp, relativeURL, target, NULL, NULL, PR_FALSE, FALSE, NULL);
}


NPError NP_EXPORT
npn_posturlnotify(NPP npp, const char* relativeURL, const char *target, uint32 len, const char *buf, NPBool file, void* notifyData)
{
	return np_posturlinternal(npp, relativeURL, target, NULL, NULL, PR_FALSE, len, buf, file, TRUE, notifyData);
}


NPError NP_EXPORT
npn_posturl(NPP npp, const char* relativeURL, const char *target, uint32 len, const char *buf, NPBool file)
{
	return np_posturlinternal(npp, relativeURL, target, NULL, NULL, PR_FALSE, len, buf, file, FALSE, NULL);
}



NPError NP_EXPORT
npn_newstream(NPP npp, NPMIMEType type, const char* window, NPStream** pstream)
{
    np_instance* instance;
	np_stream* stream;
    NET_StreamClass* netstream;
    URL_Struct* urls;
    MWContext* cx;
	*pstream = NULL;
	
    if (!npp || !type)
    	return NPERR_INVALID_PARAM;
    instance = (np_instance*) npp->ndata;
    if (!instance)
        return NPERR_INVALID_PARAM;

	/* Convert the window name to a context */
	cx = np_makecontext(instance, window);
    if (!cx)
    	return NPERR_OUT_OF_MEMORY_ERROR;


	/*
	 * Make a bogus URL struct.  The URL doesn't point to
	 * anything, but we need it to build the stream.
	 */
    urls = NET_CreateURLStruct("", NET_DONT_RELOAD);
    if (!urls)
    	return NPERR_OUT_OF_MEMORY_ERROR;
    StrAllocCopy(urls->content_type, type);
    
	/* Make a netlib stream */
    netstream = NET_StreamBuilder(FO_PRESENT, urls, cx);
    if (!netstream)
    {
    	NET_FreeURLStruct(urls);
    	return NPERR_OUT_OF_MEMORY_ERROR;
    }
    
    /* Make the plug-in stream objects */
    stream = np_makestreamobjects(instance, netstream, urls);
    if (!stream)
    {
    	XP_FREE(netstream);
    	NET_FreeURLStruct(urls);
    	return NPERR_OUT_OF_MEMORY_ERROR;
    }

	*pstream = stream->pstream;
    return NPERR_NO_ERROR;
}


int32 NP_EXPORT
npn_write(NPP npp, NPStream *pstream, int32 len, void *buffer)
{
    np_instance* instance;
    np_stream* stream;
    NET_StreamClass* netstream;
    
    if (!npp || !pstream || !buffer || len<0)
        return NPERR_INVALID_PARAM;

    instance = (np_instance*) npp->ndata;
    stream = (np_stream*) pstream->ndata;

    if (!instance || !stream)
        return NPERR_INVALID_PARAM;
    
    netstream = stream->nstream;
    if (!netstream)
    	return NPERR_INVALID_PARAM;
    	
    return (*netstream->put_block)(netstream, (const char*) buffer, len);
}

NPError NP_EXPORT
npn_destroystream(NPP npp, NPStream *pstream, NPError reason)
{
    np_instance* instance;
    np_stream* stream;
    NET_StreamClass* netstream;
    URL_Struct* urls = NULL;
    
    if (!npp || !pstream)
        return NPERR_INVALID_PARAM;

    instance = (np_instance*) npp->ndata;
    stream = (np_stream*) pstream->ndata;

    if (!instance || !stream)
        return NPERR_INVALID_PARAM;
    
    netstream = stream->nstream;
    if (netstream)
    	urls = (URL_Struct*) netstream->data_object;
    
    /*
     * If we still have a valid netlib stream, ask netlib
     * to destroy it (it will call us back to inform the
     * plug-in and delete the plug-in-specific objects).
     * If we don't have a netlib stream (possible if the
     * stream was in NP_SEEK mode: the netlib stream might
     * have been deleted but we would keep the plug-in 
     * stream around because stream->dontclose was TRUE),
     * just inform the plug-in and delete our objects.
     */
	stream->dontclose = FALSE;		/* Make sure we really delete */
    if (urls)
    {
		if (NET_InterruptStream(urls) < 0)
		{
			/* Netlib doesn't know about this stream; we must have made it */
			/*MWContext* cx = netstream->window_id;*/
		    switch (reason)
		    {
		    	case NPRES_DONE:
		    		(*netstream->complete)(netstream);
		    		break;
		    	case NPRES_USER_BREAK:
		    		(*netstream->abort)(netstream, MK_INTERRUPTED);
		    		break;
		    	case NPRES_NETWORK_ERR:
		    		(*netstream->abort)(netstream, MK_BAD_CONNECT);
		    		break;
		    	default:			/* Unknown reason code */
		    		(*netstream->abort)(netstream, -1);	
		    		break;
		    }
    		np_destroystream(stream, reason);
    		XP_FREE(netstream);
		}
	}
    else
    	np_destroystream(stream, reason);
	
/*
 * We still need a way to pass the right status code
 * through to NPL_Abort (NET_InterruptStream doesn't
 * take a status code, so the plug-in always gets
 * NPRES_USER_BREAK, not what they passed in here).
 */
    return NPERR_NO_ERROR;
}


void NP_EXPORT
npn_status(NPP npp, const char *message)
{
    if(npp)
    {
        np_instance *instance = (np_instance *)npp->ndata;
        if(instance && instance->cx)
#ifdef XP_MAC
			/* Special entry point so MacFE can save/restore port state */
			FE_PluginProgress(instance->cx, message);
#else
            FE_Progress(instance->cx, message);
#endif
    }
}

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
const char * NP_EXPORT
npn_useragent(NPP npp)
{
    static char *uabuf = 0;
    if(!uabuf)
        uabuf = PR_smprintf("%.100s/%.90s", XP_AppCodeName, XP_AppVersion);
    return (const char *)uabuf;
}
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif


#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
void * NP_EXPORT
npn_memalloc (uint32 size)
{
    return XP_ALLOC(size);
}
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif


void NP_EXPORT
npn_memfree (void *ptr)
{
    (void)XP_FREE(ptr);
}

#ifdef XP_MAC
/* For the definition of CallCacheFlushers() */
#ifndef NSPR20
#include "prmacos.h"
#else
#include "MacMemAllocator.h"
#endif
#endif

uint32 NP_EXPORT
npn_memflush(uint32 size)
{
#ifdef XP_MAC
    /* Try to free some memory and return the amount we freed. */
    if (CallCacheFlushers(size))
        return size;
    else
#endif
    return 0;
}




/*
 * Given an instance, switch its handler from whatever it 
 * currently is to the handler passed in.  Assuming the new
 * handler really is different, to do this we need to destroy
 * the current NPP instance (so the old plug-in's out of the
 * picture), then make a new NPP with the new handler
 */
NPError
np_switchHandlers(np_instance* instance,
				  np_handle* newHandle,
				  np_mimetype* newMimeType,
				  char* requestedType)
{
	NPEmbeddedApp* app = instance->app;
	MWContext* cx = instance->cx;
    np_data* ndata = (np_data*) app->np_data;
	
	if (app == NULL || cx == NULL || ndata == NULL)
		return NPERR_INVALID_PARAM;
		
	/*
	 * If it's a full-page plug-in, just reload the document.
	 * We have to reload the data anyway to send it to the
	 * new instance, and since the instance is the only thing
	 * on the page it's easier to just reload the whole thing.
	 * NOTE: This case shouldn't ever happen, since you can't
	 * have full-page Default plug-ins currently.
	 */
	XP_ASSERT(app->pagePluginType != NP_FullPage);
	if (app->pagePluginType == NP_FullPage)
	{
    	History_entry* history = SHIST_GetCurrent(&cx->hist);
        URL_Struct* urls = SHIST_CreateURLStructFromHistoryEntry(cx, history);
        if (urls != NULL)
        {
            urls->force_reload = NET_NORMAL_RELOAD;
            FE_GetURL(cx, urls);
            return NPERR_NO_ERROR;
        }
        else
        	return NPERR_GENERIC_ERROR;
	}
	
	/* Nuke the old instance */
	np_delete_instance(instance);
	if (ndata != NULL && ndata->instance == instance)
		ndata->instance = NULL;
	
	/* Make a new instance */
	ndata->instance = np_newinstance(newHandle, cx, app, newMimeType, requestedType);
	NPL_EmbedSize(app);

	if (ndata->instance == NULL)
        return NPERR_GENERIC_ERROR;
        
    /* Get the data stream for the new instance, if necessary */
    if (ndata->lo_struct->embed_src != NULL)
    {
		char* address;
        URL_Struct* urls;    
		
	    PA_LOCK(address, char*, ndata->lo_struct->embed_src);
	    XP_ASSERT(address);
 
        urls = NET_CreateURLStruct(address, NET_DONT_RELOAD);
 	    
	    PA_UNLOCK(ndata->lo_struct->embed_src); 
       
        if (urls != NULL)
        {
        	urls->fe_data = (void*) app;
        	(void) NET_GetURL(urls, FO_CACHE_AND_EMBED, cx, NPL_EmbedURLExit);
	    	return NPERR_NO_ERROR;
	    }
        else
        	return NPERR_GENERIC_ERROR;
    }
    
    return NPERR_NO_ERROR;
}



/*
 * Ask the FE to throw away its old plugin handlers and
 * re-scan the plugins folder to find new ones.  This function
 * is intended for use by the null plugin to signal that
 * some new plugin has been installed and we should make a
 * note of it.  If "reloadPages" is true, we should also
 * reload all open pages with plugins on them (since plugin
 * handlers could have come or gone as a result of the re-
 * registration).
 */
void NP_EXPORT
npn_reloadplugins(NPBool reloadPages)
{
	np_handle* oldHead = NULL;

	/*
	 * We won't unregister old plug-ins, we just register new ones.
	 * The new plug-ins will go on the front of the list, so to see
	 * if we got any new ones we just need to save a pointer to the
	 * current front of the list.
	 */
	if (reloadPages)
		oldHead = np_plist;
	
	/* Ask the FE to load new plug-ins */
    FE_RegisterPlugins();
     
    /*
     * At least one plug-in was added to the front of the list.
     * Now we need to find all instances of the default plug-in
     * to see if they can be handled by one of the new plug-ins.
     */
    if (reloadPages && oldHead != np_plist)
    {
    	np_handle* defaultPlugin = np_plist;
    	np_instance* instance;
    	
    	/* First look for the default plug-in */
    	while (defaultPlugin != NULL)
    	{
    		if (defaultPlugin->mimetypes != NULL &&
    			defaultPlugin->mimetypes->type &&
    			XP_STRCMP(defaultPlugin->mimetypes->type, "*") == 0)
    		{
    			break;
    		}
    		defaultPlugin = defaultPlugin->next;
    	}
    	
    	if (defaultPlugin == NULL)
    		return;
    	
    	/* Found the default plug-in; now check its instances */
    	instance = defaultPlugin->instances;
    	while (instance != NULL)
    	{
    		NPBool switchedHandler = FALSE;
    		char* type = instance->typeString;
    		XP_ASSERT(instance->mimetype == defaultPlugin->mimetypes);
    		
    		if (type != NULL)
    		{
    			/*
    			 * Try to match this instance's type against the
    			 * types of all new plug-ins to see if any of them
    			 * can handle it.  Since the new plug-is were added
    			 * to the front of the list, we only need to look
    			 * at plug-ins up to the old head of the list. 
    			 */
    			np_handle* handle = np_plist;
    			while (handle != NULL && handle != oldHead)
    			{
    				np_mimetype* mimeType;
					XP_ASSERT(handle != defaultPlugin);
					mimeType = np_getmimetype(handle, type, FALSE);
					
					/* 
					 * We found a new handler for this type! Now we
					 * can destroy the plug-in instance and make a
					 * new instance handled by the new plug-in.
					 * Note that we have to point "instance" to the
					 * next object NOW, because np_switchHandlers
					 * will remove it from the list.
					 */
					if (mimeType != NULL)
					{
						np_instance* switcher = instance;
						instance = instance->next;
						(void) np_switchHandlers(switcher, handle, mimeType, type);
						switchedHandler = TRUE;
						break;	/* Out of handle "while" loop */
					}
					
					handle = handle->next;
    			}
    		}
    		
    		/*
    		 * In the case where we switch the handler (above), 
    		 * "instance" already points to the next objTag.
    		 */
    		if (!switchedHandler)
    			instance = instance->next;
    	}
    }
}


NPError
NPL_RefreshPluginList(XP_Bool reloadPages)
{
	npn_reloadplugins(reloadPages);
	return NPERR_NO_ERROR;		/* Always succeeds for now */
}


void NP_EXPORT
npn_invalidaterect(NPP npp, NPRect *invalidRect)
{
    np_instance* instance = NULL;
    XP_Rect rect;

    if (npp != NULL) {
        instance = (np_instance*) npp->ndata;
    }
    
    if (instance && !instance->windowed) {
        rect.left = invalidRect->left;
        rect.top = invalidRect->top;
        rect.right = invalidRect->right;
        rect.bottom = invalidRect->bottom;
        
        CL_UpdateLayerRect(CL_GetLayerCompositor(instance->layer),
                           instance->layer, &rect, PR_FALSE);
    }
}

void NP_EXPORT
npn_invalidateregion(NPP npp, NPRegion invalidRegion)
{
    np_instance* instance = NULL;

    if (npp != NULL) {
        instance = (np_instance*) npp->ndata;
    }
    
    if (instance && !instance->windowed) {
        CL_UpdateLayerRegion(CL_GetLayerCompositor(instance->layer),
                             instance->layer, invalidRegion, PR_FALSE);
    }
}

void NP_EXPORT
npn_forceredraw(NPP npp)
{
    np_instance* instance = NULL;

    if (npp != NULL) {
        instance = (np_instance*) npp->ndata;
    }
    
    if (instance && !instance->windowed) {
        CL_CompositeNow(CL_GetLayerCompositor(instance->layer));
    }
}

/******************************************************************************/

#ifdef JAVA
#define JRI_NO_CPLUSPLUS
#define IMPLEMENT_netscape_plugin_Plugin
#include "netscape_plugin_Plugin.h"
#ifdef MOCHA
#include "libmocha.h"
#endif /* MOCHA */
#endif /* JAVA */

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
JRIEnv* NP_EXPORT
npn_getJavaEnv(void)
{
#ifdef JAVA
	JRIEnv* env;

#ifdef	XP_MAC
	short resNum1, resNum2;
	resNum1 = CurResFile();
#endif /* XP_MAC */ 

	env = LJ_EnsureJavaEnv(NULL); /* NULL means for the current thread */

#ifdef	XP_MAC
	/* if Java changed the res file, change it back to the plugin's res file */
	resNum2 = CurResFile();
	if(resNum1 != resNum2)
		UseResFile(resNum1);
#endif  /* XP_MAC */ 

    return env;
#else /* JAVA */
    return NULL;
#endif /* JAVA */
}
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

#ifdef JAVA
void
np_recover_mochaWindow(JRIEnv * env, np_instance * instance)
{
     netscape_plugin_Plugin* javaInstance = NULL;

	 if (env && instance && instance->mochaWindow && instance->javaInstance){
		 javaInstance = (struct netscape_plugin_Plugin *)
			  JRI_GetGlobalRef(env, instance->javaInstance);
		 if (javaInstance)  {
 			/* Store the JavaScript context as the window object: */
			set_netscape_plugin_Plugin_window(env, javaInstance, 
                                              (netscape_javascript_JSObject*)
                                              JRI_GetGlobalRef(env, instance->mochaWindow));
		 }		
	 }
}

#define NPN_NO_JAVA_INSTANCE	((jglobal)-1)

jglobal classPlugin = NULL;

#endif // JAVA

extern void
ET_SetPluginWindow(MWContext *cx, void *instance);

NS_DEFINE_IID(kLiveConnectPluginIID, NP_ILIVECONNECTPLUGIN_IID);

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif

#ifdef JAVA
java_lang_Class* NP_EXPORT
npn_getJavaClass(np_handle* handle)
{
    if (handle->userPlugin) {
        NPIPlugin* userPluginClass = (NPIPlugin*)handle->userPlugin;
        NPILiveConnectPlugin* lcPlugin;
        if (userPluginClass->QueryInterface(kLiveConnectPluginIID,
                                            (void**)&lcPlugin) != NS_NOINTERFACE) {
            java_lang_Class* clazz = (java_lang_Class*)lcPlugin->GetJavaClass();

            // Remember, QueryInterface increments the ref count;
            // since we're done with it in this scope, release it.
            lcPlugin->Release();

            return clazz;
        }
        return NULL;    // not a LiveConnected plugin
    }
    else if (handle && handle->f) {
        JRIEnv* env = npn_getJavaEnv();		/* may start up the java runtime */
        if (env == NULL) return NULL;
        return (java_lang_Class*)JRI_GetGlobalRef(env, handle->f->javaClass);
    }
    return NULL;
}
#endif

jref NP_EXPORT
npn_getJavaPeer(NPP npp)
{
#ifdef JAVA
    netscape_plugin_Plugin* javaInstance = NULL;
    np_instance* instance;
 
    if (npp == NULL)
		return NULL;
    instance = (np_instance*) npp->ndata;
	if (instance == NULL) return NULL;

	if (instance->javaInstance == NPN_NO_JAVA_INSTANCE) {
		/* Been there, done that. */
		return NULL;
	}
    else if (instance->javaInstance != NULL) {
		/*
		** It's ok to get the JRIEnv here -- it won't initialize the
		** runtime because it would have already been initialized to
		** create the instance that we're just about to return.
		*/

	    /* But first, see if we need to recover the mochaWindow... */
		np_recover_mochaWindow(npn_getJavaEnv(),instance);

		return (jref)JRI_GetGlobalRef(npn_getJavaEnv(), instance->javaInstance);
	}
    else {
		struct java_lang_Class* clazz = npn_getJavaClass(instance->handle);
        if (clazz) {
            JRIEnv* env = npn_getJavaEnv();		/* may start up the java runtime */
			if (classPlugin == NULL) {
				/*
				** Make sure we never unload the Plugin class. Why? Because
				** the method and field IDs we're using below have the same
				** lifetime as the class (theoretically):
				*/
				classPlugin = JRI_NewGlobalRef(env, use_netscape_plugin_Plugin(env));
			}

			/* instantiate the plugin's class: */
			javaInstance = netscape_plugin_Plugin_new(env, clazz);
			if (javaInstance) {

 				instance->javaInstance = JRI_NewGlobalRef(env, javaInstance);

				np_recover_mochaWindow(env,instance);

				/* Store the plugin as the peer: */
				set_netscape_plugin_Plugin_peer(env, javaInstance, (jint)instance->npp); 


				netscape_plugin_Plugin_init(env, javaInstance);
			}
		}
		else {
			instance->javaInstance = NPN_NO_JAVA_INSTANCE;		/* prevent trying this every time around */
			return NULL;
		}
	}
    return (jref)javaInstance;
#else
	return NULL;
#endif
}
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

static XP_Bool
np_IsLiveConnected(np_handle* handle)
{
    if (handle->userPlugin) {
        NPIPlugin* userPluginClass = (NPIPlugin*)handle->userPlugin;
        NPILiveConnectPlugin* lcPlugin;

        if (userPluginClass->QueryInterface(kLiveConnectPluginIID,
                                            (void**)&lcPlugin) != NS_NOINTERFACE) {
            // Remember, QueryInterface increments the ref count;
            // since we're done with it in this scope, release it.
            lcPlugin->Release();

            return TRUE;
        } else {
            return FALSE;
        }
    }
    else {
#ifdef JAVA
        return npn_getJavaClass(handle) != NULL;
#else
        return FALSE;
#endif
    }
}

/* Is the plugin associated with this embedStruct liveconnected? */
XP_Bool NPL_IsLiveConnected(LO_EmbedStruct *embed)
{
#ifdef JAVA
	NPEmbeddedApp* app;
	np_data* ndata;

	if (embed == NULL)
		return FALSE;

	app = (NPEmbeddedApp*) embed->objTag.FE_Data;
	if (app == NULL)
		return FALSE;

	ndata = (np_data*) app->np_data;
	XP_ASSERT(ndata);
    return np_IsLiveConnected(ndata->instance->handle);
#else
	return FALSE; 
#endif
}



/******************************************************************************/

/* Returns false if there was an error: */
static PRBool
np_setwindow(np_instance *instance, NPWindow *appWin)
{
	/*
	 * On Windows and UNIX, we don't want to give a window
	 * to hidden plug-ins.  To determine if we're hidden,
	 * we can look at the flag bit of the LO_EmbedStruct.
	 */
	NPEmbeddedApp* app;
	np_data* ndata;
	LO_EmbedStruct* lo_struct;
	
	if (instance)
	{
		app = instance->app;
		if (app)
		{
			ndata = (np_data*) app->np_data;
			lo_struct = ndata->lo_struct;
#ifndef XP_MAC
			if (lo_struct && lo_struct->objTag.ele_attrmask & LO_ELE_HIDDEN)
				return PR_TRUE;
#endif
		}
	}

    XP_ASSERT(instance);
    if (instance && appWin)
    {
        TRACEMSG(("npglue.c: CallNPP_SetWindowProc"));
        if (instance->handle->userPlugin) {
            nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*)instance->npp->pdata;
            NPIPluginInstance* userInst = peerInst->GetUserInstance();
            userInst->SetWindow((NPPluginWindow*)appWin);

            // If this is the first time we're drawing this, then call
            // the plugin's Start() method.
            if (lo_struct && ! (lo_struct->objTag.ele_attrmask & LO_ELE_DRAWN)) {
                NPPluginError err = userInst->Start();
                if (err != NPPluginError_NoError) {
                    np_delete_instance(instance);
                    return PR_FALSE;
                }
            }
        }
        else if (ISFUNCPTR(instance->handle->f->setwindow)) {
            CallNPP_SetWindowProc(instance->handle->f->setwindow, instance->npp, appWin);
        }
    }
    else
    {
        NPTRACE(0,("setwindow before appWin was valid"));
    }
    return PR_TRUE;
}

static void
np_UnloadPluginClass(np_handle *handle)
{
	/* only called when we truly want to dispose the plugin class */
	XP_ASSERT(handle && handle->refs == 0);

#ifdef JAVA
    if (handle->userPlugin == NULL && handle->f && handle->f->javaClass != NULL) {
		/* Don't get the environment unless there is a Java class,
		   because this would cause the java runtime to start up. */
		JRIEnv* env = npn_getJavaEnv();
		JRI_DisposeGlobalRef(env, handle->f->javaClass);
		handle->f->javaClass = NULL;
	}
#endif /* JAVA */ 

	FE_UnloadPlugin(handle->pdesc, handle);
	handle->f = NULL;

	XP_ASSERT(handle->instances == NULL);
	handle->instances = NULL;
}


/* this is called from the mocha thread to set the mocha window,
* in response to getJavaPeer */
PR_IMPLEMENT(void)
NPL_SetPluginWindow(void *data)
{
#ifdef JAVA
	 JRIEnv * env = NULL;
	 np_instance *instance = (np_instance *) data;
     struct netscape_javascript_JSObject *mochaWindow = NULL;

	 if (instance && instance->cx)
		mochaWindow = LJ_GetMochaWindow(instance->cx);

	 env = LJ_EnsureJavaEnv(PR_CurrentThread());

	 if (mochaWindow){
		 instance->mochaWindow = JRI_NewGlobalRef(env, (jref) mochaWindow);

		 /* That's done, now stuff it in */
 		 np_recover_mochaWindow(env,instance);
	 }
#endif
}
 

np_instance*
np_newinstance(np_handle *handle, MWContext *cx, NPEmbeddedApp *app,
			   np_mimetype *mimetype, char *requestedType)
{
    NPError err = NPERR_GENERIC_ERROR;
    np_instance* instance = NULL;
    NPP npp = NULL;
    void* tmp;
    np_data* ndata;
       
	XP_ASSERT(handle && app);
    if (!handle || !app)
        return NULL;

    /* make sure the plugin is loaded */
    if (!handle->refs)
    {
#ifdef JAVA
		JRIEnv* env = NULL;
#endif
		FE_Progress(cx, XP_GetString(XP_PLUGIN_LOADING_PLUGIN));               
        if (!(handle->f = FE_LoadPlugin(handle->pdesc, &npp_funcs, handle))) 
		{
			char* msg = PR_smprintf(XP_GetString(XP_PLUGIN_CANT_LOAD_PLUGIN), handle->name, mimetype->type);
			FE_Alert(cx, msg);
			XP_FREE(msg);
			return NULL;
		}
#ifdef JAVA
		/*
		** Don't use npn_getJavaEnv here. We don't want to start the
		** interpreter, just use env if it already exists.
		*/
		env = JRI_GetCurrentEnv();

		/*
		** An exception could have occurred when the plugin tried to load
		** it's class file. We'll print any exception to the console.
		*/
		if (env && JRI_ExceptionOccurred(env)) {
			JRI_ExceptionDescribe(env);
			JRI_ExceptionClear(env);
		}
#endif
    }

    ndata = (np_data*) app->np_data;
    NPSavedData* savedData = (NPSavedData*) (ndata ? ndata->sdata : NULL);
        
    if (handle->userPlugin == NULL || savedData == NULL) {
        // Then we're either an old style plugin that needs to get
        // (re)created, or a new style plugin that hasn't yet saved
        // its data, so it needs to get created the first time.

        /* make an instance */
        if (!(instance = XP_NEW_ZAP(np_instance)))
            goto error;

        instance->handle = handle;
        instance->cx = cx;
        instance->app = app;
        instance->mimetype = mimetype;
        instance->type = (app->pagePluginType == NP_FullPage) ? NP_FULL : NP_EMBED;
        instance->typeString = (char*) (requestedType ? XP_STRDUP(requestedType) : NULL);

        instance->mochaWindow = NULL;
        instance->javaInstance = NULL;
 
        app->type = NP_Plugin;
	
        /* make an NPP */
        if (!(tmp = XP_NEW_ZAP(NPP_t)))
            goto error;
        npp = (NPP) tmp;             /* make pc compiler happy */
        npp->ndata = instance;
        instance->npp = npp;
        instance->windowed = TRUE;
        instance->transparent = FALSE;
        
#ifdef PLUGIN_TIMER_EVENT
        instance->timeout = NULL;
        instance->interval = DEFAULT_TIMER_INTERVAL;
#endif

#ifdef LAYERS
        if (ndata)
            instance->layer = ndata->lo_struct->objTag.layer;
#endif /* LAYERS */

        /* invite the plugin */
        TRACEMSG(("npglue.c: CallNPP_NewProc"));
        if (handle->userPlugin) {
            NPIPlugin* userPluginClass = (NPIPlugin*)handle->userPlugin;
            nsPluginInstancePeer* peerInst = new nsPluginInstancePeer(npp);
            if (peerInst == NULL) {
                err = NPERR_OUT_OF_MEMORY_ERROR;
            }
            else {
                peerInst->AddRef();
                NPIPluginInstance* userInst;
                NPPluginError err2 = userPluginClass->NewInstance(peerInst, &userInst);
                if (err2 == NPPluginError_NoError && userInst != NULL) {
                    npp->pdata = peerInst;
                    peerInst->SetUserInstance(userInst);
                    ndata->sdata = (NPSavedData*)userInst;
                    err = NPERR_NO_ERROR;
                }
                else
                    err = NPERR_INVALID_INSTANCE_ERROR;
            }
        }
        else {
            if (ISFUNCPTR(handle->f->newp))
            {
                XP_ASSERT(ndata);
                if (instance->type == NP_EMBED)
                {
                    /* Embedded plug-ins get their attributes passed in from layout */
#ifdef OJI
                    int16 argc = (int16) ndata->lo_struct->attributes.n;
                    char** names = ndata->lo_struct->attributes.names;
                    char** values = ndata->lo_struct->attributes.values;
#else
                    int16 argc = (int16) ndata->lo_struct->attribute_cnt;
                    char** names = ndata->lo_struct->attribute_list;
                    char** values = ndata->lo_struct->value_list;
#endif
                    err = CallNPP_NewProc(handle->f->newp, requestedType, npp, 
                                          instance->type, argc, names, values, savedData);
                }
                else
                {
                    /* A full-page plugin must be told its palette may
                       be realized as a foreground palette */ 
                    char name[] = "PALETTE";
                    char value[] = "foreground";
                    char* names[1];
                    char* values[1];
                    int16 argc = 1;
                    names[0] = name;
                    values[0] = value;

                    err = CallNPP_NewProc(handle->f->newp, requestedType, npp, 
                                          instance->type, argc, names, values, savedData);
                }
            }
        }
        if (err != NPERR_NO_ERROR)
            goto error;
	
        /* add this to the handle chain */
        instance->next = handle->instances;
        handle->instances = instance;
        handle->refs++;

        /*
         * In the full-page case, FE_DisplayEmbed hasn't been called yet, 
         * so the window hasn't been created and wdata is still NULL.
         * We don't want to give the plug-in a NULL window.
         * N.B.: Actually, on the Mac, the window HAS been created (we
         * need it because even undisplayed/hidden plug-ins may need a
         * window), so wdata is not NULL; that's why we check the plug-in
         * type rather than wdata here.
         */
#ifndef LAYERS
        /* 
         * We don't know that layout has set the final position of the plug-in at this
         * point. The danger is that the plug-in will draw into the window incorrectly
         * with this call. With layers, we don't display the window until layout
         * is completely done - at that we can call NPP_SetWindow.
         */
        if (app->pagePluginType == NP_Embedded)
        {
            XP_ASSERT(app->wdata);
            PRBool success = np_setwindow(instance, app->wdata);
            if (!success) goto error;
        }
#endif
    }

    /* XXX This is _not_ where Start() should go (IMO). Start() should be
       called whenever we re-visit an applet

    // Finally, if it's a 5.0-style (C++) plugin, send it the Start message.
    // Do this before sending the mocha OnLoad message.
    if (handle->userPlugin && ndata->sdata) {
        NPIPluginInstance* userInst = (NPIPluginInstance*)ndata->sdata;
        NPPluginError err = userInst->Start();
        if (err != NPPluginError_NoError) goto error;
    }
    */

#ifdef MOCHA
    {
        /* only wait on applets if onload flag */
        lo_TopState *top_state = lo_FetchTopState(XP_DOCID(cx));
        if (top_state != NULL && top_state->mocha_loading_embeds_count)
        {
            top_state->mocha_loading_embeds_count--;
            ET_SendLoadEvent(cx, EVENT_XFER_DONE, NULL, NULL, 
                             LO_DOCUMENT_LAYER_ID, FALSE);
        }

        /* tell the mocha thread to set us up with the window when it can */
        if (
#if 0
            // XXX This is what we really want here, because it doesn't actually
            // start up the jvm, it just checks that the plugin is LiveConnected.
            // The problem is that by deferring the jvm startup, we cause it to 
            // happen later on the wrong thread. 
            np_IsLiveConnected(handle)
#elif defined(JAVA)
            npn_getJavaClass(handle)
#else
            FALSE
#endif
            ) { /*  for liveconnected plugins only */
            ET_SetPluginWindow(cx, (void *)instance);
        }
    }
#endif /* MOCHA */

    return instance;
    
error:
    /* Unload the plugin if there are no other instances */
    if (handle->refs == 0)
    {
		np_UnloadPluginClass(handle);
    }

	if (instance)
    	XP_FREE(instance);
    if (npp)
    	XP_FREE(npp);
    return NULL;
}



NET_StreamClass *
np_newstream(URL_Struct *urls, np_handle *handle, np_instance *instance)
{
    NET_StreamClass *nstream = nil;
    NPStream *pstream = nil;
    np_stream *stream = nil;
    uint16 stype;
	XP_Bool alreadyLocal;
    XP_Bool b1;
	XP_Bool b2;

    /* make a netlib stream */
    if (!(nstream = XP_NEW_ZAP(NET_StreamClass))) 
        return 0;

	/* make the plugin stream data structures */
	stream = np_makestreamobjects(instance, nstream, urls);
	if (!stream)
	{
        XP_FREE(nstream);
        return 0;
	}
	pstream = stream->pstream;

	stream->prev_stream = NULL;
	
    /* Let us treat mailbox as remote too
       Not doing so causes problems with some attachments (Adobe)
    */
    b1 = NET_IsURLInDiskCache(stream->initial_urls);
    b2 = (XP_STRNCASECMP(urls->address, "mailbox:", 8) == 0) ? 0 : NET_IsLocalFileURL(urls->address);

    alreadyLocal = b1 || b2;
  
    /* determine if the stream is seekable */
    if (urls->server_can_do_byteranges || alreadyLocal)
    {
    	/*
    	 * Zero-length streams are never seekable.
    	 * This will force us to download the entire
    	 * stream if a byterange request is made.
    	 */
    	if (urls->content_length > 0)
        	stream->seekable = 1;
    }

    /* and call the plugin */
    instance->reentrant = 1;
    stype = NP_NORMAL;
    TRACEMSG(("npglue.c: CallNPP_NewStreamProc"));
    if (handle->userPlugin) {
        nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*)instance->npp->pdata;
        NPIPluginInstance* userInst = peerInst->GetUserInstance();
        nsPluginStreamPeer* peerStream = new nsPluginStreamPeer(urls, stream);
        if (peerStream == NULL) {
            /* XXX where's the error go? */
        }
        else {
            peerStream->AddRef();
            NPIPluginStream* userStream;
            NPPluginError err = userInst->NewStream(peerStream, &userStream);
            if (err == NPPluginError_NoError && userStream != NULL) {
                peerStream->SetUserStream(userStream);
                pstream->pdata = peerStream;

                stype = userStream->GetStreamType();
            }
            else {
                /* XXX where's the error go? */
            }
        }
    }
    else if (ISFUNCPTR(handle->f->newstream))
    {
        /*XXX*/CallNPP_NewStreamProc(handle->f->newstream, instance->npp, urls->content_type, 
                                     pstream, stream->seekable, &stype);
    }
    if(!instance->reentrant)
    {
        urls->pre_exit_fn = np_dofetch;
        XP_FREE(nstream);       /* will not call abort */
        return 0;
    }
    instance->reentrant = 0;

    /* see if its hard */
    if(stype == NP_SEEK)
    {
        if(!stream->seekable)
        {
            NPTRACE(0,("stream is dumb, force caching"));
            stream->seek = 2;
        }
		/* for a seekable stream that doesn't require caching, in the SSL case, don't cache, because that 
		will leave the supposedly secure file laying around in the cache! */
		if (   !alreadyLocal 
			&& !(XP_STRNCASECMP(urls->address, "https:", 6)==0))
        	urls->must_cache = TRUE; 
		stream->dontclose++;
    }
	else if (stype == NP_ASFILE || stype == NP_ASFILEONLY)
    {   
        NPTRACE(0,("stream as file"));
        if (!alreadyLocal)
        	urls->must_cache = TRUE;
        stream->asfile = stype;
    }
	
	/*
	 * If they want just the file, and the file is local, there's 
	 * no need to continue with the netlib stream: just give them
	 * the file and we're done.
	 */
	if (stype == NP_ASFILEONLY)	
	{
		if (urls->cache_file || NET_IsLocalFileURL(urls->address))	
		{
			np_streamAsFile(stream);
			np_destroystream(stream, NPRES_DONE);
			XP_FREE(nstream);
			return NULL;
		}
    }

    /* and populate the netlib stream */
    nstream->name           = "plug-in";
    nstream->complete       = NPL_Complete;
    nstream->abort          = NPL_Abort;
    nstream->is_write_ready = NPL_WriteReady;
    nstream->put_block      = (MKStreamWriteFunc)NPL_Write;
    nstream->data_object    = (void *)urls;
    nstream->window_id      = instance->cx;

	/* In case of Mailbox->StreamAsFile, use cache code to store, handle... */
	if ( ((stype == NP_ASFILE) || (stype == NP_ASFILEONLY)) && 
	     ((XP_STRNCASECMP(urls->address, "mailbox:", 8)==0)  
	   || (XP_STRNCASECMP(urls->address, "news:"   , 5)==0)
	   || (XP_STRNCASECMP(urls->address, "snews:"  , 6)==0))		 
		 && 
	     (stream != NULL) && 
		 (urls->cache_file == NULL)) /* if already cached, is well-handled */
	   {
    	   urls->must_cache = TRUE;
	       stream->prev_stream = NET_StreamBuilder(FO_CACHE_ONLY,urls,instance->cx);
	   }

    return nstream;
}

XP_Bool np_FakeHTMLStream(URL_Struct* urls, MWContext* cx, char * fakehtml)
{
	NET_StreamClass* viewstream;
	char* org_content_type = urls->content_type; 
	XP_Bool ret = FALSE;
	
	urls->content_type = NULL;

	StrAllocCopy(urls->content_type, TEXT_HTML);
	if(urls->content_type == NULL) /* StrAllocCopy failed */
		goto Exit;

	urls->is_binary = 1;    						/* flag for mailto and saveas */

	if ((viewstream = NET_StreamBuilder(FO_PRESENT, urls, cx)) != 0)
	{
		(*viewstream->put_block)(viewstream, fakehtml, XP_STRLEN(fakehtml));
		(*viewstream->complete)(viewstream);

		XP_FREEIF(viewstream);
		viewstream = NULL;
		ret = TRUE;
	}

	XP_FREE(urls->content_type);

Exit:
	urls->content_type = org_content_type;
	return ret;
}

NET_StreamClass*
NPL_NewPresentStream(FO_Present_Types format_out, void* type, URL_Struct* urls, MWContext* cx)
{
    np_handle* handle = (np_handle*) type;
    np_instance* instance = NULL;
	np_data* ndata = NULL;
	np_mimetype* mimetype = NULL;
	np_reconnect* reconnect;
    NPEmbeddedApp *app = NULL;

#ifdef	ANTHRAX
	char* fileName;
	char* newTag;
	uint32 strLen;
#endif	/* ANTHRAX */

    XP_ASSERT(type && urls && cx);
	if (!type || !urls || !cx)
		return NULL;

	/* fe_data is set by EmbedCreate, which hasn't happed yet for PRESENT streams */
	XP_ASSERT(urls->fe_data == NULL);	

#ifdef	ANTHRAX
	if((fileName = NPL_FindAppletEnabledForMimetype(handle->name)) != NULL)
	{
		XP_FREE(fileName);	/* we don't need the applet name here, so discard it */
		fileName = strrchr(urls->address, '/')+1;
		
		strLen = XP_STRLEN(fileName);
		
		newTag = XP_ALLOC((36+strLen)*sizeof(char));
		newTag[0] = 0;
		
		XP_STRCAT(newTag, "<embed src=");
		XP_STRCAT(newTag, fileName);
		XP_STRCAT(newTag, " width=100% height=100%>");
		
		np_FakeHTMLStream(urls,cx,newTag);
		XP_FREE(newTag);
		return NULL;				
    }
#endif	/* ANTHRAX */
				
	mimetype = np_getmimetype(handle, urls->content_type, TRUE);
	if (!mimetype)
		return NULL;
 
 	/*
 	 * The following code special-cases the LiveAudio plug-in to open 
 	 * a new chromeless window.  A new window is only opened if there's
 	 * history information for the current context; that prevents us from
 	 * opening ANOTHER new window if the FE has already made one (for
 	 * example, if the user chose "New window for this link" from the
 	 * popup).
 	 */
	if (handle->name && (XP_STRCASECMP(handle->name, "LiveAudio") == 0))
	{
		History_entry* history = SHIST_GetCurrent(&cx->hist);
		if (history)
		{
			MWContext* oldContext = cx;
			Chrome* customChrome = XP_NEW_ZAP(Chrome);
			if (customChrome == NULL)
				return NULL;
			customChrome->w_hint = 144 + 1;
			customChrome->h_hint = 60 + 1;
			customChrome->allow_close = TRUE;
			
			/* Make a new window with no URL or window name, but special chrome */
			cx = FE_MakeNewWindow(oldContext, NULL, NULL, customChrome);
			if (cx == NULL)
			{
				XP_FREE(customChrome);
				return NULL;
			}
			/* Insert some HTML to notify of Java delay: */
			{
				JRIEnv* env = NULL;
				/* Has Java already been started? */
#ifdef JAVA
				env = JRI_GetCurrentEnv();
#endif
				if (env == NULL){ 
					/* nope, java not yet started */
					static char fakehtml[255] = "";

					XP_SPRINTF(fakehtml,"<HTML><p><CENTER>%s</CENTER></HTML>",XP_GetString(XP_PROGRESS_STARTING_JAVA));
					np_FakeHTMLStream(urls,cx,fakehtml);
				}
			}
			
			/* Switch to the new context, but don't change the exit routine */
			NET_SetNewContext(urls, cx, NULL);
		}
	}

	 
 	/*
 	 * Set up the "reconnect" data, which is used to communicate between this
 	 * function and the call to EmbedCreate that will result from pushing the
 	 * data into the stream below.  EmbedCreate needs to know from us the 
 	 * np_mimetype and requestedtype for this stream, and we need to know from
 	 * it the NPEmbeddedApp that it created.
 	 */
    XP_ASSERT(cx->pluginReconnect == NULL);
	reconnect = XP_NEW_ZAP(np_reconnect);
	if (!reconnect)
		return NULL;
    cx->pluginReconnect = (void*) reconnect;
    reconnect->mimetype = mimetype;
    reconnect->requestedtype = XP_STRDUP(urls->content_type);

	/*
	 * To actually create the instance we need to create a stream of
	 * fake HTML to cause layout to create a new embedded objTag.
	 * EmbedCreate will be called, which will created the NPEmbeddedApp
	 * and put it into urls->fe_data, where we can retrieve it.
	 */
	{
		static char fakehtml[] = "<embed src=internal-external-plugin width=1 height=1>";
		np_FakeHTMLStream(urls,cx,fakehtml);				
    }
    
    /*
     * Retrieve the app created by EmbedCreate and stashed in the reconnect data.
     * From the app we can get the np_data, which in turn holds the handle and
     * instance, which we need to create the streams.
     */
    app = reconnect->app;
    XP_FREE(reconnect);
    cx->pluginReconnect = NULL;
    
    if (!app)
    	return NULL;  /* will be NULL if the plugin failed to initialize */
    XP_ASSERT(app->pagePluginType == NP_FullPage);
    
	urls->fe_data = (void*) app;		/* fe_data of plug-in URLs always holds NPEmbeddedApp */
	
    ndata = (np_data*) app->np_data;
    XP_ASSERT(ndata);
    if (!ndata)
    	return NULL;
    	
    handle = ndata->handle;
    instance = ndata->instance;
    XP_ASSERT(handle && instance);
	if (!handle || !instance)
		return NULL;
		
    /* now actually make a plugin and netlib stream */
    return np_newstream(urls, handle, instance);
}



NET_StreamClass*
NPL_NewEmbedStream(FO_Present_Types format_out, void* type, URL_Struct* urls, MWContext* cx)
{
    np_handle* handle = (np_handle*) type;
 	np_data* ndata = NULL;
    NPEmbeddedApp* app = NULL;

    XP_ASSERT(type && urls && cx);
	if (!type || !urls || !cx)
		return NULL;
		
	/* fe_data is set by EmbedCreate, which has already happened for EMBED streams */
    app = (NPEmbeddedApp*) urls->fe_data;
    XP_ASSERT(app);
    if (!app)
    	return NULL;
    XP_ASSERT(app->pagePluginType == NP_Embedded);

    ndata = (np_data*) app->np_data;
	XP_ASSERT(ndata && ndata->lo_struct);
	if (!ndata)
		return NULL;
		
	if (ndata->instance == NULL)
	{
		np_instance* instance;
		np_mimetype* mimetype;

		/* Map the stream's MIME type to a np_mimetype object */
		mimetype = np_getmimetype(handle, urls->content_type, TRUE);
		if (!mimetype)
			return NULL;
	 
	    /* Now that we have the MIME type and the layout data, we can create an instance */
	    instance = np_newinstance(handle, cx, app, mimetype, urls->content_type);
	    if (!instance)
	        return NULL;

	    ndata->instance = instance;
	    ndata->handle = handle;
#ifdef LAYERS
        LO_SetEmbedType(ndata->lo_struct, (PRBool) ndata->instance->windowed);
#endif
    }
    
    /* now actually make a plugin and netlib stream */
    return np_newstream(urls, ndata->instance->handle, ndata->instance);
}


static NET_StreamClass *
np_newbyterangestream(FO_Present_Types format_out, void *type, URL_Struct *urls, MWContext *cx)
{
    NET_StreamClass *nstream = nil;

    /* make a netlib stream */
    if (!(nstream = XP_NEW_ZAP(NET_StreamClass))) 
        return 0;

    urls->position = 0;         /* single threaded for now */

    /* populate netlib stream */
    nstream->name           = "plug-in byterange";
    nstream->complete       = NPL_Complete;
    nstream->abort          = NPL_Abort;
    nstream->is_write_ready = NPL_WriteReady;
    nstream->put_block      = (MKStreamWriteFunc)NPL_Write;
    nstream->data_object    = (void *)urls;
    nstream->window_id      = cx;

    return nstream;
}

static NET_StreamClass *
np_newpluginstream(FO_Present_Types format_out, void *type, URL_Struct *urls, MWContext *cx)
{
    NPEmbeddedApp* app = (NPEmbeddedApp*) urls->fe_data;

    if (app)
    {
        np_data *ndata = (np_data *)app->np_data;
        if(ndata && ndata->instance)
        {
            XP_ASSERT(ndata->instance->app == app);
            return np_newstream(urls, ndata->instance->handle, ndata->instance);
        }
    }
    return 0;
}

NPError
NPL_RegisterPluginFile(const char* pluginname, const char* filename, const char* description,
                       void *pdesc)
{
    np_handle* handle;

    NPTRACE(0,("np: register file %s", filename));

#ifdef DEBUG
	/* Ensure uniqueness of pdesc values! */
	for (handle = np_plist; handle; handle = handle->next)
		XP_ASSERT(handle->pdesc != pdesc);
#endif
	
	handle = XP_NEW_ZAP(np_handle);
	if (!handle)
		return NPERR_OUT_OF_MEMORY_ERROR;
	
	StrAllocCopy(handle->name, pluginname);
	StrAllocCopy(handle->filename, filename);
	StrAllocCopy(handle->description, description);
	
	handle->pdesc = pdesc;
	handle->next = np_plist;
    handle->userPlugin = NULL;
   	np_plist = handle;
   	
   	return NPERR_NO_ERROR;
}

/*
 * Given a pluginName and a mimetype, this will enable the plugin for
 * the mimetype and disable anyother plugin that had been enabled for
 * this mimetype.
 *
 * pluginName and type cannot be NULL.
 *
 * WARNING: If enable is FALSE, this doesn't unregister the converters yet.
 */
NPError
NPL_EnablePlugin(NPMIMEType type, const char *pluginName, XP_Bool enabled)
{
    np_handle* handle;
    np_mimetype* mimetype;
    NPTRACE(0,("np: enable plugin %s for type %s", pluginName, type));

	if (!pluginName || !*pluginName || !type || !*type)
		return(NPERR_INVALID_PARAM);

	for (handle = np_plist; handle; handle = handle->next)
	{
		if (!strcmp(handle->name, pluginName))
			break;
	}

	if (!handle)
		/* Plugin with the specified name not found */
		return(NPERR_INVALID_INSTANCE_ERROR);
	
	/* Look for an existing MIME type object for the specified type */
	/* We can't use np_getmimetype, because it respects enabledness and
	   here we don't care */
	for (mimetype = handle->mimetypes; mimetype; mimetype = mimetype->next)
	{
		if (strcasecomp(mimetype->type, type) == 0)
			break;
	}

	if (!mimetype)
		/* This plugin cannot handler the specified mimetype */
		return(NPERR_INVALID_PLUGIN_ERROR);

	/* Find the plug-in that was previously enabled for this type and
	   disable it */
	if (enabled)
	{
		XP_Bool foundType = FALSE;
		np_handle* temphandle;
		np_mimetype* temptype;
		
		for (temphandle = np_plist; temphandle && !foundType; temphandle = temphandle->next)
		{
			for (temptype = temphandle->mimetypes; temptype && !foundType; temptype = temptype->next)
			{
				if (temptype->enabled && strcasecomp(temptype->type, type) == 0)
				{
					temptype->enabled = FALSE;
					foundType = TRUE;
				}
			}
		}
	}
	
	mimetype->enabled = enabled;
	
	if (mimetype->enabled)
	{
		/*
		 * Is this plugin the wildcard (a.k.a. null) plugin?
		 * If so, we don't want to register it for FO_PRESENT
		 * or it will interfere with our normal unknown-mime-
		 * type handling.
		 */
		XP_Bool wildtype = (strcmp(type, "*") == 0);
		
#if defined(XP_WIN) || defined(XP_OS2)
	    /* EmbedStream does some Windows FE work and then calls NPL_NewStream */
		if (!wildtype)
	        NET_RegisterContentTypeConverter(type, FO_PRESENT, handle, EmbedStream);
	    NET_RegisterContentTypeConverter(type, FO_EMBED, handle, EmbedStream); /* XXX I dont think this does anything useful */
#else
		if (!wildtype)
		  {
	        NET_RegisterContentTypeConverter(type, FO_PRESENT, handle, NPL_NewPresentStream);
#ifdef XP_UNIX
			/* The following three lines should be outside the ifdef someday */
			NET_RegisterAllEncodingConverters(type, FO_PRESENT);
			NET_RegisterAllEncodingConverters(type, FO_EMBED);
			NET_RegisterAllEncodingConverters(type, FO_PLUGIN);

			/* While printing we use the FO_SAVE_AS_POSTSCRIPT format type. We want
			 * plugin to possibly handle that case too. Hence this.
			 */
	        NET_RegisterContentTypeConverter(type, FO_SAVE_AS_POSTSCRIPT, handle,
											 NPL_NewPresentStream);
#endif /* XP_UNIX */
		}
	    NET_RegisterContentTypeConverter(type, FO_EMBED, handle, NPL_NewEmbedStream);
#endif
	    NET_RegisterContentTypeConverter(type, FO_PLUGIN, handle, np_newpluginstream);
	    NET_RegisterContentTypeConverter(type, FO_BYTERANGE, handle, np_newbyterangestream);
	}

    return(NPERR_NO_ERROR);
}


/* 
 * Look up the handle and mimetype objects given
 * the pdesc value and the mime type string.
 * Return TRUE if found successfully.
 */
void
np_findPluginType(NPMIMEType type, void* pdesc, np_handle** outHandle, np_mimetype** outMimetype)
{
    np_handle* handle;
    np_mimetype* mimetype;

	*outHandle = NULL;
	*outMimetype = NULL;
	
	/* Look for an existing handle */
	for (handle = np_plist; handle; handle = handle->next)
	{
		if (handle->pdesc == pdesc)
			break;
	}

	if (!handle)
		return;
	*outHandle = handle;
		
	/* Look for an existing MIME type object for the specified type */
	/* We can't use np_getmimetype, because it respects enabledness and here we don't care */
	for (mimetype = handle->mimetypes; mimetype; mimetype = mimetype->next)
	{
		if (strcasecomp(mimetype->type, type) == 0)
			break;
	}
	
	if (!mimetype)
		return;
	*outMimetype = mimetype;
}


void 
np_enablePluginType(np_handle* handle, np_mimetype* mimetype, XP_Bool enabled)
{
	char* type = mimetype->type;
	
	/*
	 * Find the plug-in that was previously
	 * enabled for this type and disable it.
	 */
	if (enabled)
	{
		XP_Bool foundType = FALSE;
		np_handle* temphandle;
		np_mimetype* temptype;
		
		for (temphandle = np_plist; temphandle && !foundType; temphandle = temphandle->next)
		{
			for (temptype = temphandle->mimetypes; temptype && !foundType; temptype = temptype->next)
			{
				if (temptype->enabled && strcasecomp(temptype->type, type) == 0)
				{
					temptype->enabled = FALSE;
					foundType = TRUE;
				}
			}
		}
	}
	
	mimetype->enabled = enabled;

	if (enabled)
	{
		/*
		 * Is this plugin the wildcard (a.k.a. null) plugin?
		 * If so, we don't want to register it for FO_PRESENT
		 * or it will interfere with our normal unknown-mime-
		 * type handling.
		 */
		XP_Bool wildtype = (strcmp(type, "*") == 0);
		
#ifdef XP_WIN
	    /* EmbedStream does some Windows FE work and then calls NPL_NewStream */
		if (!wildtype)
	        NET_RegisterContentTypeConverter(type, FO_PRESENT, handle, EmbedStream);
	    NET_RegisterContentTypeConverter(type, FO_EMBED, handle, EmbedStream); /* XXX I dont think this does anything useful */
#else
		if (!wildtype)
		  {
	        NET_RegisterContentTypeConverter(type, FO_PRESENT, handle, NPL_NewPresentStream);
#ifdef XP_UNIX
			/* While printing we use the FO_SAVE_AS_POSTSCRIPT format type. We want
			 * plugin to possibly handle that case too. Hence this.
			 */
	        NET_RegisterContentTypeConverter(type, FO_SAVE_AS_POSTSCRIPT, handle,
											 NPL_NewPresentStream);
#endif /* XP_UNIX */
		  }
	    NET_RegisterContentTypeConverter(type, FO_EMBED, handle, NPL_NewEmbedStream);
#endif
	    NET_RegisterContentTypeConverter(type, FO_PLUGIN, handle, np_newpluginstream);
	    NET_RegisterContentTypeConverter(type, FO_BYTERANGE, handle, np_newbyterangestream);
	}
}


NPError
NPL_EnablePluginType(NPMIMEType type, void* pdesc, XP_Bool enabled)
{
    np_handle* handle;
    np_mimetype* mimetype;
	
	if (!type)
		return NPERR_INVALID_PARAM;
		
	np_findPluginType(type, pdesc, &handle, &mimetype);
	
	if (!handle || !mimetype)
		return NPERR_INVALID_PARAM;

	np_enablePluginType(handle, mimetype, enabled);
		return NPERR_NO_ERROR;
}


/* XXX currently there is no unregister */
NPError
NPL_RegisterPluginType(NPMIMEType type, const char *extensions, const char* description,
		void* fileType, void *pdesc, XP_Bool enabled)
{
    np_handle* handle = NULL;
    np_mimetype* mimetype = NULL;
    NPTRACE(0,("np: register type %s", type));

	np_findPluginType(type, pdesc, &handle, &mimetype);

	/* We have to find the handle to do anything */
	XP_ASSERT(handle);
	if (!handle)
		return NPERR_INVALID_PARAM;
		
	/*If no existing mime type, add a new type to this handle */
	if (!mimetype)
	{
		mimetype = XP_NEW_ZAP(np_mimetype);
		if (!mimetype)
			return NPERR_OUT_OF_MEMORY_ERROR;
		mimetype->next = handle->mimetypes;
		handle->mimetypes = mimetype;
		mimetype->handle = handle;
		StrAllocCopy(mimetype->type, type);
	}

	/* Enable this plug-in for this type and disable any others */
	np_enablePluginType(handle, mimetype, enabled);

	/* Get rid of old file association info, if any */
	if (mimetype->fassoc)
	{
		void* fileType;
		fileType = NPL_DeleteFileAssociation(mimetype->fassoc);
#if 0
		/* Any FE that needs to free this, implement FE_FreeNPFileType */
		if (fileType)
			FE_FreeNPFileType(fileType);
#endif
		mimetype->fassoc = NULL;
	}
			
	/* Make a new file association and register it with netlib if enabled */
	XP_ASSERT(extensions && description);
    mimetype->fassoc = NPL_NewFileAssociation(type, extensions, description, fileType);
    if (mimetype->fassoc && enabled)
        NPL_RegisterFileAssociation(mimetype->fassoc);
    
    return NPERR_NO_ERROR;
}


/*
 * Add a NPEmbeddedApp to the list of plugins for the specified context.
 * We need to append items, because the FEs depend on them appearing
 * in the list in the same order in which they were created.
 */
void
np_bindContext(NPEmbeddedApp* app, MWContext* cx)
{
	np_data* ndata;
	
	XP_ASSERT(app && cx);
	XP_ASSERT(app->next == NULL);
	
    if (cx->pluginList == NULL)  					/* no list yet, just insert item */
        cx->pluginList = app;
    else                  							/* the list has at least one item in it, append to it */
    {
        NPEmbeddedApp* pItem = cx->pluginList;  	/* the first element */
        while(pItem->next) pItem = pItem->next; 	/* find the last element */
        pItem->next = app;  						/* append */
    }
    
    /* If there's an instance, set the instance's context */
    ndata = (np_data*) app->np_data;
    if (ndata)
    {
    	np_instance* instance = (np_instance*) ndata->instance;
    	if (instance)
    		instance->cx = cx;
    }
    	
}

void
np_unbindContext(NPEmbeddedApp* app, MWContext* cx)
{
	np_data* ndata;

	XP_ASSERT(app && cx);

    if (app == cx->pluginList)
        cx->pluginList = app->next;
    else
    {
        NPEmbeddedApp *ax;
        for (ax=cx->pluginList; ax; ax=ax->next)
            if (ax->next == app)
            {
                ax->next = ax->next->next;
                break;
            }
    }

	app->next = NULL;
	
    /* If there's an instance, clear the instance's context */
    ndata = (np_data*) app->np_data;
    if (ndata)
    {
    	np_instance* instance = (np_instance*) ndata->instance;
    	if (instance)
    		instance->cx = NULL;
    }
}


void
np_delete_instance(np_instance *instance)
{
    if(instance)
    {
        np_handle *handle = instance->handle;
        np_stream *stream;

        /* nuke all open streams */
        for(stream=instance->streams; stream;)
        {
	    	np_stream *next = stream->next;
            stream->dontclose = 0;
            if (stream->nstream)
            {
            	/* Make sure the urls doesn't still point to us */
            	URL_Struct* urls = (URL_Struct*) stream->nstream->data_object;
            	if (urls)
            		urls->fe_data = NULL;
            }
            np_destroystream(stream, NPRES_USER_BREAK);
	    	stream = next;
        }
        instance->streams = 0;

        if (handle) {
            NPSavedData *save = NULL;

            TRACEMSG(("npglue.c: CallNPP_DestroyProc"));
            if (np_is50StylePlugin(instance->handle)) {
                nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*)instance->npp->pdata;
                NPIPluginInstance* userInst = peerInst->GetUserInstance();

                userInst->SetWindow(NULL);

                nsrefcnt cnt;
                cnt = userInst->Release();
                XP_ASSERT(cnt == 0);

                // XXX Is this the right place to be releasing the
                // peer?
                cnt = peerInst->Release();
                XP_ASSERT(cnt == 0);

                // XXX Any other bookkeeping we need to do here?

                // Since this is a 5.0-style (C++) plugin, we know we've
                // been called from NPL_DeleteSessionData (at best),
                // or other similarly terminal functions. So there is
                // no chance that the plugin will be able to save any
                // of its state for later...
            } else if (handle->f && ISFUNCPTR(handle->f->destroy)) {
                CallNPP_DestroyProc(handle->f->destroy, instance->npp, &save);
            }
            if (instance->app && instance->app->np_data) {
                np_data* pnp = (np_data*)instance->app->np_data;
                pnp->sdata = save;
            }
#ifdef JAVA		
			/*
			** Break any association we have made between this instance and
			** its corresponding Java objTag. That way other java objects
			** still referring to it will be able to detect that the plugin
			** went away (by calling isActive).
			*/
			if (instance->javaInstance != NULL &&
				instance->javaInstance != NPN_NO_JAVA_INSTANCE)
			{
				/* Don't get the environment unless there is a Java instance,
				   because this would cause the java runtime to start up. */
 				JRIEnv* env = npn_getJavaEnv();
				netscape_plugin_Plugin* javaInstance = (netscape_plugin_Plugin*)
					JRI_GetGlobalRef(env, instance->javaInstance);

				/* upcall to the user's code */
				netscape_plugin_Plugin_destroy(env, javaInstance);

				set_netscape_plugin_Plugin_peer(env, javaInstance, 0);
				JRI_DisposeGlobalRef(env, instance->javaInstance);
				instance->javaInstance = NULL;
			}
#endif /* JAVA */

            /* If we come through here after having been unbound from
               a context, then we need to make one up to call into the
               front-end */
            MWContext *context = (instance->cx != NULL)
                ? instance->cx : XP_FindSomeContext();

            if (XP_OK_ASSERT(context != NULL)) {

#ifdef XP_MAC
                /* turn scrollbars back on */
                if(instance->type == NP_FULL)
                    FE_ShowScrollBars(context, TRUE);
#endif                

                /* Tell the front end to blow away the plugin window */
                FE_DestroyEmbedWindow(context, instance->app);
            }

            /* remove it from the handle list */
            if(instance == handle->instances)
                handle->instances = instance->next;
            else
            {
                np_instance *ix;
                for(ix=handle->instances; ix; ix=ix->next)
                    if(ix->next == instance)
                    {
                        ix->next = ix->next->next;
                        break;
                    }
            }

            handle->refs--;
            XP_ASSERT(handle->refs>=0);
            if(!handle->refs)
            {
				np_UnloadPluginClass(handle);
            }
        }

		np_removeAllURLsFromList(instance);
		
		if (instance->typeString)
			XP_FREE(instance->typeString);
			
#ifdef PLUGIN_TIMER_EVENT			
		if(instance->timeout)
			FE_ClearTimeout(instance->timeout);
#endif			

        XP_FREE(instance);
    }
}

#ifdef XP_UNIX
static NET_StreamClass *
np_noembedfound (FO_Present_Types format_out, 
				 void *type, 
				 URL_Struct *urls, MWContext *cx)
{
	char *msg = PR_smprintf(XP_GetString(XP_PLUGIN_NOT_FOUND),
							urls->content_type);
	if(msg)
	  {
		FE_Alert(cx, msg);
		XP_FREE(msg);
	  }

	return(NULL);
}
#endif /* XP_UNIX */

void
NPL_RegisterDefaultConverters()
{
    /* get netlib to deal with our content streams */
    NET_RegisterContentTypeConverter("*", FO_CACHE_AND_EMBED, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter("*", FO_CACHE_AND_PLUGIN, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter("*", FO_CACHE_AND_BYTERANGE, NULL, NET_CacheConverter);

    NET_RegisterContentTypeConverter("multipart/x-byteranges", FO_CACHE_AND_BYTERANGE, NULL, CV_MakeMultipleDocumentStream);

    NET_RegisterContentTypeConverter("*", FO_PLUGIN, NULL, np_newpluginstream);
    NET_RegisterContentTypeConverter("*", FO_BYTERANGE, NULL, np_newbyterangestream);
#ifdef XP_UNIX
    NET_RegisterContentTypeConverter("*", FO_EMBED, NULL, np_noembedfound);
#endif /* XP_UNIX */
}

/* called from netscape main */
void
NPL_Init()
{

#if defined(XP_UNIX) && defined(DEBUG)
    {
        char *str;
        str = getenv("NPD");
        if(str)
            np_debug=atoi(str);
    }
#endif

    /* Register all default plugin converters. Do this before
	 * FE_RegisterPlugins() because this registers a not found converter
	 * for "*" and FE can override that with the nullplugin if one is
	 * available.
	 */
    NPL_RegisterDefaultConverters();

    /* call the platform specific FE code to enumerate and register plugins */
    FE_RegisterPlugins();

    /* construct the function table for calls back into netscape.
       no plugin sees this until its actually loaded */
    npp_funcs.size = sizeof(npp_funcs);
    npp_funcs.version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

    npp_funcs.geturl = NewNPN_GetURLProc(npn_geturl);
    npp_funcs.posturl = NewNPN_PostURLProc(npn_posturl);
    npp_funcs.requestread = NewNPN_RequestReadProc(npn_requestread);
    npp_funcs.newstream = NewNPN_NewStreamProc(npn_newstream);
    npp_funcs.write = NewNPN_WriteProc(npn_write);
    npp_funcs.destroystream = NewNPN_DestroyStreamProc(npn_destroystream);
    npp_funcs.status = NewNPN_StatusProc(npn_status);
    npp_funcs.uagent = NewNPN_UserAgentProc(npn_useragent);
    npp_funcs.memalloc = NewNPN_MemAllocProc(npn_memalloc);
    npp_funcs.memfree = NewNPN_MemFreeProc(npn_memfree);
    npp_funcs.memflush = NewNPN_MemFlushProc(npn_memflush);
    npp_funcs.reloadplugins = NewNPN_ReloadPluginsProc(npn_reloadplugins);
    npp_funcs.getJavaEnv = NewNPN_GetJavaEnvProc(npn_getJavaEnv);
    npp_funcs.getJavaPeer = NewNPN_GetJavaPeerProc(npn_getJavaPeer);
    npp_funcs.geturlnotify = NewNPN_GetURLNotifyProc(npn_geturlnotify);
    npp_funcs.posturlnotify = NewNPN_PostURLNotifyProc(npn_posturlnotify);
    npp_funcs.getvalue = NewNPN_GetValueProc(npn_getvalue);
    npp_funcs.setvalue = NewNPN_SetValueProc(npn_setvalue);
    npp_funcs.invalidaterect = NewNPN_InvalidateRectProc(npn_invalidaterect);
    npp_funcs.invalidateregion = NewNPN_InvalidateRegionProc(npn_invalidateregion);
    npp_funcs.forceredraw = NewNPN_ForceRedrawProc(npn_forceredraw);
}


void
NPL_Shutdown()
{
    np_handle *handle, *dh;
    np_instance *instance, *di;

    for(handle=np_plist; handle;)
    {
        dh=handle;
        handle = handle->next;

        /* delete handle */
        for(instance=dh->instances; instance;)
        {
            di = instance;
            instance=instance->next;
            np_delete_instance(di);
        }
    }
}



/*
 * Like NPL_SamePage, but for an individual element.
 * It is called by laytable.c when relaying out table cells. 
 */
void
NPL_SameElement(LO_EmbedStruct* embed_struct)
{
	if (embed_struct)
	{
		NPEmbeddedApp* app = (NPEmbeddedApp*) embed_struct->objTag.FE_Data;
		if (app)
		{
			np_data* ndata = (np_data*) app->np_data;
			XP_ASSERT(ndata);
			if (ndata && ndata->state != NPDataSaved)
				ndata->state = NPDataCache;
		}
	}
}



/*
 * This function is called by the FE's when they're resizing a page.
 * We take advantage of this information to mark all our instances
 * so we know not to delete them when the layout information is torn
 * down so we can keep using the same instances with the new, resized,
 * layout structures.
 */
void
NPL_SamePage(MWContext* resizedContext)
{
	MWContext* cx;
	XP_List* children;
	NPEmbeddedApp* app;
	
	if (!resizedContext)
		return;
		
	/* Mark all plug-ins in this context */
	app = resizedContext->pluginList;
	while (app)
	{
		np_data* ndata = (np_data*) app->np_data;
		XP_ASSERT(ndata);
		if (ndata && ndata->state != NPDataSaved)
			ndata->state = NPDataCache;
		app = app->next;
	}
	
	/* Recursively traverse child contexts */
	children = resizedContext->grid_children;
	while ((cx = (MWContext*)XP_ListNextObject(children)) != NULL)
		NPL_SamePage(cx);
}

int
NPL_HandleEvent(NPEmbeddedApp *app, void *event, void* window)
{
    if (app)
    {
        np_data *ndata = (np_data *)app->np_data;
        if(ndata && ndata->instance)
        {
            np_handle *handle = ndata->instance->handle;
            if (handle) {
                TRACEMSG(("npglue.c: CallNPP_HandleEventProc"));
                if (handle->userPlugin) {
                    nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*)ndata->instance->npp->pdata;
                    NPIPluginInstance* userInst = peerInst->GetUserInstance();

                    // Note that the new NPPluginEvent struct is different from the
                    // old NPEvent (which is the argument passed in) so we have to
                    // translate. (Later we might fix the front end code to pass us 
                    // the new thing instead.)
                    NPEvent* oldEvent = (NPEvent*)event;
                    NPPluginEvent newEvent;
#if defined(XP_MAC)
                    newEvent.event = oldEvent;
                    newEvent.window = window;
#elif defined(XP_WIN)
                    newEvent.event = oldEvent->event;
                    newEvent.wParam = oldEvent->wParam;
                    newEvent.lParam = oldEvent->lParam;
#elif defined(XP_OS2)
                    newEvent.event = oldEvent->event;
                    newEvent.wParam = oldEvent->wParam;
                    newEvent.lParam = oldEvent->lParam;
#elif defined(XP_UNIX)
                    XP_MEMCPY(&newEvent.event, event, sizeof(XEvent));
                    // we don't need window for unix -- it's already in the event
#endif
                    return userInst->HandleEvent(&newEvent);
                }
                else if (handle->f && ISFUNCPTR(handle->f->event)) {
                    // window is not passed through to old-style plugins
                    // XXX shouldn't this check for windowless plugins only?
                    return CallNPP_HandleEventProc(handle->f->event, ndata->instance->npp, event);
                }
            }
        }
    }
    return 0;
}

void npn_registerwindow(NPP npp, void* window)
{
#ifdef XP_MAC
    if(npp) {
        np_instance* instance = (np_instance*) npp->ndata;
        FE_RegisterWindow(instance->app->fe_data, window);
    }
#endif
}

void npn_unregisterwindow(NPP npp, void* window)
{
#ifdef XP_MAC
    if(npp) {
        np_instance* instance = (np_instance*) npp->ndata;
        FE_UnregisterWindow(instance->app->fe_data, window);
    }
#endif
}

int16 npn_allocateMenuID(NPP npp, XP_Bool isSubmenu)
{
#ifdef XP_MAC
    if (npp) {
	np_instance* instance = (np_instance*) npp->ndata;
	return FE_AllocateMenuID(instance->app->fe_data, isSubmenu);
    }
#endif
    return 0;
}

XP_Bool
npn_IsWindowless(np_handle* handle)
{
    if (handle->userPlugin) {
        // XXX anybody using the new plugin api supports windowless, right?
        return TRUE;
    }
    else {
        return handle->f->version >= NPVERS_HAS_WINDOWLESS;
    }
}

void
NPL_Print(NPEmbeddedApp *app, void *pdata)
{
    if (app)
    {
        np_data *ndata = (np_data *)app->np_data;
        if(ndata && ndata->instance)
        {
            np_handle *handle = ndata->instance->handle;
            if(handle)
            {
				NPPrint * nppr = (NPPrint *)pdata;
				if (nppr && (nppr->mode == NP_EMBED) && !npn_IsWindowless(handle)) 
				{
					/*
					   If old (pre-4.0) plugin version, have to the "platformPrint" void * up,
					   because window.type didn't previously exist.
					  
						check plugin version:  
						Major,Minor version = 0,11 or greater => 4.0, 
						else < 4.0
					*/

					NPWindowType old_type = nppr->print.embedPrint.window.type;
					void * addr1 = (void *)&nppr->print.embedPrint.window.type;

					void * old_plat_print = nppr->print.embedPrint.platformPrint;
					void * addr2 = (void *)&nppr->print.embedPrint.platformPrint;

					XP_MEMCPY(addr1,addr2,sizeof(nppr->print.embedPrint.platformPrint));

					TRACEMSG(("npglue.c: CallNPP_PrintProc(1)"));
                    if (handle->userPlugin) {
                        nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*)ndata->instance->npp->pdata;
                        NPIPluginInstance* userInst = peerInst->GetUserInstance();
                        userInst->Print((NPPluginPrint*)pdata);

                        
                    }
                    else if (handle->f && ISFUNCPTR(handle->f->print)) {
                        CallNPP_PrintProc(handle->f->print, ndata->instance->npp, (NPPrint*)pdata);
                    }

					/* Now restore for downstream dependencies */
				    nppr->print.embedPrint.window.type   = old_type;
				    nppr->print.embedPrint.platformPrint = old_plat_print;

				}
				else{
					TRACEMSG(("npglue.c: CallNPP_PrintProc(2)"));
                    if (handle->userPlugin) {
                        nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*)ndata->instance->npp->pdata;
                        NPIPluginInstance* userInst = peerInst->GetUserInstance();
                        userInst->Print((NPPluginPrint*)pdata);
                    }
                    else if (handle->f && ISFUNCPTR(handle->f->print)) {
                        CallNPP_PrintProc(handle->f->print, ndata->instance->npp, (NPPrint*)pdata);
                    }
				}

            }
        }
    }
}

void
np_deleteapp(MWContext* cx, NPEmbeddedApp* app)
{
	if (app)
	{
		if (cx)
			np_unbindContext(app, cx);
		
    	XP_FREE(app);
	}
}

/*
 * This is called by the front-end via layout when the plug-in's
 * context is going away.  Based on what kind of plugin this is, what
 * the context looks like, etc., we decided whether to destroy the
 * plug-in's window or to make the front-end store it somewhere safe
 * for us.
 */
extern void
NPL_EmbedDelete(MWContext* cx, LO_EmbedStruct* embed_struct)
{
	NPEmbeddedApp* app;
	np_data* ndata;
	
	if (!cx || !embed_struct || !embed_struct->objTag.FE_Data)
		return;
		
	app = (NPEmbeddedApp*) embed_struct->objTag.FE_Data;
	embed_struct->objTag.FE_Data = NULL;

    ndata = (np_data*) app->np_data;

    if (ndata)
    {
        embed_struct->objTag.session_data = (void*) ndata;
        	
    	ndata->refs--;

		/* -1 case is added. It happens when this is fake object */
        // XXX I think that "fake objects" should no longer be
        // required now that we actually have a front-end callback to
        // destroy the window.
		XP_ASSERT(/* ndata->refs == -1 || */ ndata->refs == 0 || ndata->refs == 1);

        if (ndata->refs > 0) {
        	/* When done printing, don't delete and don't save session data */
        	XP_ASSERT(cx->type == MWContextPrint || cx->type == MWContextMetaFile ||
					  cx->type == MWContextPostScript);
        	ndata->state = NPDataCached;

            /* Tell the front-end to save the embedded window for us */
            FE_SaveEmbedWindow(cx, app);
            return;
   		}
   		else if (ndata->state == NPDataCache)
        {
            /* Someone is telling us to cache the window; e.g., during
               a nasty resize */
        	XP_ASSERT(ndata->app);
            ndata->state = NPDataCached;
            ndata->lo_struct = NULL;     		/* Remove ref to layout structure since it's about to be deleted */
            np_unbindContext(app, cx);			/* Remove ref to context since it may change */

            /* Tell the front-end to save the embedded window for us */
            FE_SaveEmbedWindow(cx, app);
            return;
        }
        else if (ndata->instance)
        {
            /* Otherwise, the context is just plain and simple getting
               blown away */
            XP_ASSERT(ndata->instance->handle != NULL);
            if (ndata->instance->handle->userPlugin) {
                /* This is a 5.0-style (C++) plugin. We'll simply stop the plugin. */

                /* XXX We could just get the _real_ instance by
                   traversing ndata->sdata, but that scares me for
                   some reason. */
                nsPluginInstancePeer* peerInst = (nsPluginInstancePeer*) ndata->instance->npp->pdata;
                NPIPluginInstance* userInst = peerInst->GetUserInstance();

                NPPluginError err = userInst->Stop();
                if (err == NPPluginError_NoError) {
                    /* XXX So I'm going out on a limb here and saying that
                       by keeping the plugin in a "cached" state, we
                       should pretty much not need to perturb much
                       else... */
                    ndata->state = NPDataCached;
                    ndata->lo_struct = NULL;
                    np_unbindContext(app, cx);

                    /* Tell the front-end to save the embedded window for us */
                    FE_SaveEmbedWindow(cx, app);
                    return;
                }
                else {
                    // the plugin failed to stop properly
                    // XXX is the following right?...
                    np_delete_instance(ndata->instance);
                    embed_struct->objTag.session_data = NULL;
                    app->np_data = NULL;
                    XP_FREE(ndata);
                }
            } else {
                /* It's a normal old-fashioned plugin. Destroy the instance */
                np_delete_instance(ndata->instance);

                ndata->app = NULL;
                ndata->instance = NULL;
                ndata->lo_struct = NULL;
                ndata->streamStarted = FALSE;
                ndata->state = NPDataSaved; 		/* ndata gets freed later when history goes away */
            }
        }
        else
        {
        	/* If there's no instance, there's no need to save session data */
        	embed_struct->objTag.session_data = NULL;
        	app->np_data = NULL;
        	XP_FREE(ndata);
        }
    }

    /* XXX This is pretty convoluted how this just all falls through
       to here. Clean it up sometime... */
    np_deleteapp(cx, app);						/* unlink app from context and delete app */
}



/*
 * Get all the embeds in this context to save themselves in the
 * designated saved data list so we can reuse them when printing.
 * (Except hidden ones!)
 */
void
NPL_PreparePrint(MWContext* context, SHIST_SavedData* savedData)
{
	NPEmbeddedApp* app;
	
	XP_ASSERT(context && savedData);
	if (!context || !savedData)
		return;
	
	for (app = context->pluginList; app != NULL; app = app->next)
	{
		np_data* ndata = (np_data*)app->np_data;
		XP_ASSERT(ndata);
		if (ndata && ndata->lo_struct)
		{	
			/* ignore this assert if the plugin is hidden */
			XP_ASSERT(ndata->state == NPDataNormal);
			ndata->state = NPDataCached;
			LO_AddEmbedData(context, ndata->lo_struct, ndata);
		}	
	}
	
	LO_CopySavedEmbedData(context, savedData);
}



/*
 * This function should be moved to layembed.c so the TYPE
 * attribute is pulled out the same way as the SRC attribute.
 */
static char*
np_findTypeAttribute(LO_EmbedStruct* embed_struct)
{
	char* typeAttribute = NULL;
	int i;

	/* Look for the TYPE attribute */
#ifdef OJI
	for (i = 0; i < embed_struct->attributes.n; i++)
	{
		if (XP_STRCASECMP(embed_struct->attributes.names[i], "TYPE") == 0)
		{
			typeAttribute = embed_struct->attributes.values[i];
			break;
		}
	}	
#else
	for (i = 0; i < embed_struct->attribute_cnt; i++)
	{
		if (XP_STRCASECMP(embed_struct->attribute_list[i], "TYPE") == 0)
		{
			typeAttribute = embed_struct->value_list[i];
			break;
		}
	}
#endif

	return typeAttribute;
}

void
np_FindHandleByType(const char* typeAttribute, np_handle* *resultingHandle,
                    np_mimetype* *resultingMimetype)
{
    np_handle* handle = NULL;
    np_mimetype* mimetype = NULL;

    for (handle = np_plist; handle; handle = handle->next) {
        mimetype = np_getmimetype(handle, typeAttribute, FALSE);
        if (mimetype) break;
    }

    /* No handler with an exactly-matching name, so check for a wildcard */ 
    if (!mimetype) 
    {
        for (handle = np_plist; handle; handle = handle->next) {
            mimetype = np_getmimetype(handle, typeAttribute, TRUE);
            if (mimetype) break;
        }
    }

    *resultingHandle = handle;
    *resultingMimetype = mimetype;
}

// Used by OJI to load the Java VM plugin
PR_IMPLEMENT(struct NPIPlugin*)
NPL_LoadPluginByType(const char* typeAttribute)
{
    np_handle* handle = NULL;
    np_mimetype* mimetype = NULL;
    np_FindHandleByType(typeAttribute, &handle, &mimetype);
    if (mimetype == NULL)
        return NULL;
    PR_ASSERT(handle);

    PRBool loaded = PR_FALSE;
    if (handle->refs == 0) {
//        FE_Progress(cx, XP_GetString(XP_PLUGIN_LOADING_PLUGIN));               
        if (!(handle->f = FE_LoadPlugin(handle->pdesc, &npp_funcs, handle))) 
        {
//            char* msg = PR_smprintf(XP_GetString(XP_PLUGIN_CANT_LOAD_PLUGIN), handle->name, mimetype->type);
//            FE_Alert(cx, msg);
//            XP_FREE(msg);
            return NULL;
        }
        loaded = PR_TRUE;
#ifdef JAVA
        /*
        ** Don't use npn_getJavaEnv here. We don't want to start the
        ** interpreter, just use env if it already exists.
        */
        JRIEnv* env = JRI_GetCurrentEnv();

        /*
        ** An exception could have occurred when the plugin tried to load
        ** it's class file. We'll print any exception to the console.
        */
        if (env && JRI_ExceptionOccurred(env)) {
            JRI_ExceptionDescribe(env);
            JRI_ExceptionClear(env);
        }
#endif
    }
    if (handle->userPlugin) {
        // refcount was incremented
        return handle->userPlugin;
    }
    else {
        // old style plugin -- we failed so unload it
        if (loaded)
            FE_UnloadPlugin(handle->pdesc, handle);
        return NULL;
    }
}

/*
 * This is called by the front-end to create a new plug-in. It will
 * fill in the FE_Data member of the embed_struct with a pointer to
 * the NPEmbeddedApp that gets created.
 */
NPEmbeddedApp*
NPL_EmbedCreate(MWContext* cx, LO_EmbedStruct* embed_struct)
{
    NPEmbeddedApp* app = NULL;
    np_data* ndata = NULL;
    
    XP_ASSERT(cx && embed_struct);
    if (!cx || !embed_struct)
    	goto error;
    	
    /*
     * Check the contents of the session data. If we have a cached
     * app in the session data, we can short-circuit this function
     * and just return the app we cached earlier.  If we have saved
     * data in the session data, keep that np_data object but 
     * attach it to a new app.  If there is nothing in the session
     * data, then we must create both a np_data object and an app.
     */
    if (embed_struct->objTag.session_data)
    {
        ndata = (np_data*) embed_struct->objTag.session_data;
        embed_struct->objTag.session_data = NULL;

        if (ndata->state == NPDataCached)			/* We cached this app, so don't create another */
        {
	        XP_ASSERT(ndata->app);
	        if (cx->type == MWContextPrint ||
                cx->type == MWContextMetaFile ||
				cx->type == MWContextPostScript)
	        {
                /* This is a printing "instance" that we're restoring
                   from the session data */
	        	if (ndata->app->pagePluginType == NP_FullPage)
	        	{
	        		np_reconnect* reconnect;
	        		if (!cx->pluginReconnect)
						cx->pluginReconnect = XP_NEW_ZAP(np_reconnect);
					reconnect = (np_reconnect*) cx->pluginReconnect;
					if (reconnect)
						reconnect->app = ndata->app;
	        	}

#ifdef LAYERS
                if ((cx->compositor) && ndata->instance) {
                    LO_SetEmbedType(embed_struct, (PRBool)ndata->instance->windowed);
                }
#endif /* LAYERS */
	        }
	        else
	        {
                /* It's a real instance that we're restoring from the
                   session data */
	   			ndata->lo_struct = embed_struct;		/* Set reference to new layout structure */
	   			np_bindContext(ndata->app, cx);			/* Set reference to (potentially) new context */
	            ndata->state = NPDataNormal;
#ifdef LAYERS
                if (ndata->instance) {
                    ndata->instance->layer = embed_struct->objTag.layer;
                    LO_SetEmbedType(ndata->lo_struct, 
                                    (PRBool)ndata->instance->windowed);
                }
#endif /* LAYERS */
            }
            
            /*
             * For full-page/frame plug-ins, make sure scroll
             * bars are off in the (potentially) new context.
             */
            if (ndata->app->pagePluginType == NP_FullPage)
            	FE_ShowScrollBars(cx, FALSE);
            	
            /*
             * Increment the refcount since this app is now in use.
             * Currently the most refs we can have will be 2, if
             * this app is being displayed in one context and 
             * printed in another.
             */
            ndata->refs++;
            XP_ASSERT(ndata->refs == 1 || ndata->refs == 2);

            /* Make the front-end restore the embedded window from
               it's saved context. */
            if (! (embed_struct->objTag.ele_attrmask & LO_ELE_HIDDEN))
                FE_RestoreEmbedWindow(cx, ndata->app);

            /* Tie the app to the layout struct. */
            embed_struct->objTag.FE_Data = ndata->app;
	        return ndata->app;
	    }

        /* If not cached, it's just saved data. (XXX NPL_StartPlugin
           will take care of re-constituting it?) */
	    XP_ASSERT(ndata->state == NPDataSaved);
	    XP_ASSERT(ndata->app == NULL);
	    XP_ASSERT(ndata->instance == NULL);
	    XP_ASSERT(ndata->lo_struct == NULL);
	    XP_ASSERT(ndata->streamStarted == FALSE);
	    XP_ASSERT(ndata->refs == 0);
    }

    /* So now we either have a "saved" pre-5.0 style plugin, or a
       brand new plugin. */
	if (!ndata)
    {
        ndata = XP_NEW_ZAP(np_data);
        if (!ndata)
        	goto error;
    }
	ndata->state = NPDataNormal;
    ndata->lo_struct = embed_struct;

    /*
     * Create the NPEmbeddedApp and attach it to its context.
     */
    app = XP_NEW_ZAP(NPEmbeddedApp);
    if (!app)
    	goto error;
    app->np_data = (void*) ndata;
    app->type = NP_Untyped;
    ndata->refs = 1;
	ndata->app = app;
	np_bindContext(app, cx);

    /* Tell the front-end to create the plugin window for us. */
    if (! (embed_struct->objTag.ele_attrmask & LO_ELE_HIDDEN))
        FE_CreateEmbedWindow(cx, app);
    
    /* Attach the app to the layout info */
    embed_struct->objTag.FE_Data = ndata->app;
    return app;
	
error:	
	if (app)
		np_deleteapp(cx, app);			/* Unlink app from context and delete app */
	if (ndata)
		XP_FREE(ndata);
	return NULL;
}


NPError
NPL_EmbedStart(MWContext* cx, LO_EmbedStruct* embed_struct, NPEmbeddedApp* app)
{
	np_handle* handle;
	np_mimetype* mimetype;
	char* typeAttribute;
    np_data* ndata = NULL;
    
    if (!cx || !embed_struct || !app)
    	goto error;
    	
    ndata = (np_data*) app->np_data;
    if (!ndata)
    	goto error;
    
    /*
     * Don't do all the work in this function multiple times for the
     * same NPEmbeddedApp.  For example, we could be reusing this 
     * app in a new context (when resizing or printing) or just re-
     * creating the app multiple times (when laying out complex tables),
     * so we don't want to be creating another stream, etc. each
     * time. 
     */
    if (ndata->streamStarted)
    	return NPERR_NO_ERROR;
    ndata->streamStarted = TRUE;			/* Remember that we've been here */
    
	/*
	 * First check for a TYPE attribute.  The type specified
	 * will override the MIME type of the SRC (if present).
	 */
	typeAttribute = np_findTypeAttribute(embed_struct);
	if (typeAttribute)
	{
		/* Only embedded plug-ins can have a TYPE attribute */
		app->pagePluginType = NP_Embedded;

		/* Found the TYPE attribute, so look for a matching handler */
        np_FindHandleByType(typeAttribute, &handle, &mimetype);

		/*
		 * If we found a handler, now we can create an instance.
		 * If we didn't find a handler, we have to use the SRC
		 * to determine the MIME type later (so there better be
		 * SRC, or it's an error).
		 */
		if (mimetype)
		{
			ndata->instance = np_newinstance(handle, cx, app, mimetype, typeAttribute);
			if (ndata->instance == NULL)
		        goto error;

#ifdef LAYERS
            LO_SetEmbedType(ndata->lo_struct, (PRBool) ndata->instance->windowed);
#endif
		}
	}
	 
	/*
	 * Now check for the SRC attribute.
	 * - If it's full-page, create a instance now since we already 
	 *	 know the MIME type (NPL_NewStream has already happened).
	 * - If it's embedded, create a stream for the URL (we'll create 
	 *	 the instance when we get the stream in NPL_NewStream).
	 */
    if (embed_struct->embed_src)
    {
		char* theURL;
	    PA_LOCK(theURL, char*, embed_struct->embed_src);
	    XP_ASSERT(theURL);
	    if (XP_STRCMP(theURL, "internal-external-plugin") == 0)
	    {
	    	/*
	    	 * Full-page case: Stream already exists, so now
	    	 * we can create the instance.
	    	 */
	    	np_reconnect* reconnect;
	    	np_mimetype* mimetype;
	    	np_handle* handle;
	    	np_instance* instance;
	    	char* requestedtype;
	    	
	        app->pagePluginType = NP_FullPage;

			reconnect = (np_reconnect*) cx->pluginReconnect;
	    	XP_ASSERT(reconnect); 
			if (!reconnect)
			{
				PA_UNLOCK(embed_struct->embed_src); 
		        goto error;
			}
			
	    	mimetype = reconnect->mimetype;
	    	requestedtype = reconnect->requestedtype;
			handle = mimetype->handle;
			
			/* Now we can create the instance */
			XP_ASSERT(ndata->instance == NULL);
		    instance = np_newinstance(handle, cx, app, mimetype, requestedtype);
		    if (!instance)
		    {
	    		PA_UNLOCK(embed_struct->embed_src); 
		        goto error;
		    }
	    
			reconnect->app = app;
	        ndata->instance =  instance;
	        ndata->handle = handle;
	        ndata->instance->app = app;
	        FE_ShowScrollBars(cx, FALSE);
#ifdef LAYERS
            LO_SetEmbedType(ndata->lo_struct, (PRBool) ndata->instance->windowed);
#endif
	    }
	    else
	    {
	    	/*
	    	 * Embedded case: Stream doesn't exist yet, so
	    	 * we need to create it before we can make the
	    	 * instance (exception: if there was a TYPE tag,
	    	 * we already know the MIME type so the instance
	    	 * already exists).
	    	 */
	        app->pagePluginType = NP_Embedded;
	        
	        if ((embed_struct->objTag.ele_attrmask & LO_ELE_STREAM_STARTED) == 0)
	        {
		        URL_Struct* pURL;    
		        pURL = NET_CreateURLStruct(theURL, NET_DONT_RELOAD);
		        pURL->fe_data = (void*) app;
		
		        /* start a stream */
		        (void) NET_GetURL(pURL, FO_CACHE_AND_EMBED, cx, NPL_EmbedURLExit);
		    }
	    }
	    
	    PA_UNLOCK(embed_struct->embed_src); 
	}
	
	return NPERR_NO_ERROR;

error:
	if (cx && app)
		np_deleteapp(cx, app);			/* Unlink app from context and delete app */
	if (ndata)
		XP_FREE(ndata);
	return NPERR_GENERIC_ERROR;
}


/*
 * Called by the front-end via layout whenever layout information changes,
 * including size, visibility status, etc.
 */
void
NPL_EmbedSize(NPEmbeddedApp *app)
{
    if (app) {
        np_data *ndata = (np_data *)app->np_data;
        if (ndata && ndata->instance && app->wdata) {
            PRBool success = np_setwindow(ndata->instance, app->wdata);
            if (!success) return;       // XXX deal with the error
        }
    }
}

/* the following is used in CGenericDoc::FreeEmbedElement */
int32
NPL_GetEmbedReferenceCount(NPEmbeddedApp *app)
{
  np_data *ndata = (np_data *)app->np_data;
  int32 iRet = ndata->refs;
  return iRet;
}

XP_Bool          
NPL_IsEmbedWindowed(NPEmbeddedApp *app)
{
    if(app)
    {
        np_data *ndata = (np_data *)app->np_data;
        if (ndata && ndata->instance)
            return ndata->instance->windowed;
        else
            return FALSE;
    }
    else
        return FALSE;
}


/*
 * This is called by layout when the context in which the embedded
 * object lives is finally decimated. We need to decide:
 *
 * 1. Exactly what kind of context is getting destroyed (e.g., we
 * could care less if it's a printing context, because it was a
 * "dummy" context, anyway).
 *
 * 2. whether or not we have left an embedded window lying around in
 * the front-end somewhere.
 */
void
NPL_DeleteSessionData(MWContext* context, void* sdata)
{
    XP_Bool bFreeSessionData = TRUE;
    np_data* ndata = (np_data*) sdata;

    /*
     * Printing Case
     *
     * Don't delete the data if we're printing, since
     * data really belongs to the original context (a
     * MWContextBrowser).  A more generic way to make
     * this check might be 'if (context != ndata->
     * instance->cx)'.
     */
    // XXX Ugh. Now the front-end thinks we've freed our session data, but
    // in reality we just leaked some memory and maybe a widget or two...
    if (ndata == NULL ||
        context->type == MWContextPrint ||
        context->type == MWContextMetaFile ||
        context->type == MWContextPostScript)
    	return;
    	

    if (ndata->state == NPDataCached) {
        if (ndata->instance != NULL && np_is50StylePlugin(ndata->instance->handle)) {
            /*
             * 5.0-style (C++) plugin case
             *
             * Since 5.0-style plugins aren't deleted and unloaded as
             * soon as the current context goes away, they lie around
             * in the history for a while with the data state as
             * NPDataCached. In this case, we've been called back to
             * let us know that it's time to clean up our act.
             */
            
            np_delete_instance(ndata->instance);
            return;
        }

        /*
         * Resize case 
         *
         * This case happens when we resize a page but must
         * delete the session history of the old page before
         * creating the new (resized) one.  Typically this
         * happens because the resized page isn't in the cache;
         * thus we must retrieve the document from the net and
         * can't use its session data because we don't know if
         * the document has changed or not.  In this case,
         * NPL_EmbedDelete was already called for the instances
         * of the original document, but we didn't delete the
         * instances because we were expecting to re-use them
         * in the resized document.  Since the session data is
         * going away, we won't have that opportunity, so we
         * must delete them here, or else the instances will be
         * left dangling, which is really bad.
         */

        /* Shouldn't have instance data when resizing */
        XP_ASSERT(ndata->sdata == NULL);
        XP_ASSERT(ndata->app != NULL);

        /*
         * Fall through to saved instance case so ndata and
         * plug-in's saved data and will be deleted.
         */
    }


    /* ndata could be already freed by this point in NPL_EmbedDelete call
     * so check session_data which is zeroed in this case
     */
    if(bFreeSessionData)
        {
            /*
             * Saved instance data case
             *
             * This case occurs when session history is deleted for
             * a document that previously contained a plug-in. 
             * The plug-in may have given us a NPSavedData structure
             * to store for it, which we must delete here.
             */
            XP_ASSERT(ndata->state == NPDataSaved);
            XP_ASSERT(ndata->app == NULL);
            XP_ASSERT(ndata->streamStarted == FALSE);

            // XXX I don't think we should ever get here with a
            // 5.0-style (C++) plugin since, by default, they cache
            // themselves. Throw in an assert just to make sure...
            XP_ASSERT(ndata->instance == NULL || !np_is50StylePlugin(ndata->instance->handle));

            if (ndata->sdata) {
                if (ndata->sdata->buf)
                    XP_FREE(ndata->sdata->buf);
                XP_FREE(ndata->sdata);
                ndata->sdata = 0;
            }

            ndata->handle = NULL;
            XP_FREE(ndata);
        }

    // XXX Err...what's the point?
    ndata = NULL;
}


NPBool
NPL_IteratePluginFiles(NPReference* ref, char** name, char** filename, char** description)
{
	np_handle* handle;
	
	if (*ref == NPRefFromStart)
		handle = np_plist;
	else
		handle = ((np_handle*) *ref)->next;

	if (handle)
	{
		if (name)
			*name = handle->name;
		if (filename)
			*filename = handle->filename;
		if (description)
			*description = handle->description;
	}
	
	*ref = handle;
	return (handle != NULL);
}


NPBool
NPL_IteratePluginTypes(NPReference* ref, NPReference plugin, NPMIMEType* type, char*** extents,
		 char** description, void** fileType)
{
	np_handle* handle = (np_handle*) plugin;
	np_mimetype* mimetype;
	
	if (*ref == NPRefFromStart)
		mimetype = handle->mimetypes;
	else	
		mimetype = ((np_mimetype*) *ref)->next;
		
	if (mimetype)
	{
		if (type)
			*type = mimetype->type;
		if (description)
			*description = mimetype->fassoc->description;
		if (extents)
			*extents = mimetype->fassoc->extentlist;
		if (fileType)
			*fileType = mimetype->fassoc->fileType;
	}
	
	*ref = mimetype;
	return (mimetype != NULL);
	
}




/*
 * Returns a null-terminated array of plug-in names that support the specified MIME type.
 * This function is called by the FEs to implement their MIME handler controls (they need
 * to know which plug-ins can handle a particular type to build their popup menu).
 * The caller is responsible for deleting the strings and the array itself. 
 */
char**
NPL_FindPluginsForType(const char* typeToFind)
{
	char** result;
	uint32 count = 0;

	/* First count plug-ins that support this type */
	{
		NPReference plugin = NPRefFromStart;
		while (NPL_IteratePluginFiles(&plugin, NULL, NULL, NULL))
		{
			char* type;
			NPReference mimetype = NPRefFromStart;
			while (NPL_IteratePluginTypes(&mimetype, plugin, &type, NULL, NULL, NULL))
			{
				if (strcmp(type, typeToFind) == 0)
					count++;
			}
		}
	}

	/* Bail if no plug-ins match this type */
	if (count == 0)
		return NULL;
		
	/* Make an array big enough to hold the plug-ins */
	result = (char**) XP_ALLOC((count + 1) * sizeof(char*));
	if (!result)
		return NULL;

	/* Look for plug-ins that support this type and put them in the array */
	count = 0;
	{
		char* name;
		NPReference plugin = NPRefFromStart;
		while (NPL_IteratePluginFiles(&plugin, &name, NULL, NULL))
		{
			char* type;
			NPReference mimetype = NPRefFromStart;
			while (NPL_IteratePluginTypes(&mimetype, plugin, &type, NULL, NULL, NULL))
			{
				if (strcmp(type, typeToFind) == 0)
					result[count++] = XP_STRDUP(name);
			}
		}
	}
	
	/* Null-terminate the array and return it */
	result[count] = NULL;
	return result;
}


/*
 * Returns the name of the plug-in enabled for the 
 * specified type, of NULL if no plug-in is enabled.
 * The caller is responsible for deleting the string. 
 */
char*
NPL_FindPluginEnabledForType(const char* typeToFind)
{
	np_handle* handle = np_plist;
	while (handle)
	{
		np_mimetype* mimetype = handle->mimetypes;
		while (mimetype)
		{
			if ((strcmp(mimetype->type, typeToFind) == 0) && mimetype->enabled)
				return XP_STRDUP(handle->name);
			mimetype = mimetype->next;
		}
		
		handle = handle->next;
	}
	
	return NULL;
}



#if !defined(XP_MAC) && !defined(XP_UNIX)		/* plugins change */

/* put_block modifies input buffer. We cannot pass static data to it.
 * And hence the strdup.
 */
#define PUT(string) if(string) { \
	char *s = XP_STRDUP(string); \
	int ret; \
	ret = (*stream->put_block)(stream,s, XP_STRLEN(s)); \
   	XP_FREE(s); \
	if (ret < 0) \
		return; \
}

void
NPL_DisplayPluginsAsHTML(FO_Present_Types format_out, URL_Struct *urls, MWContext *cx)
{
    NET_StreamClass * stream;
    np_handle *handle;

    StrAllocCopy(urls->content_type, TEXT_HTML);
    format_out = CLEAR_CACHE_BIT(format_out);

    stream = NET_StreamBuilder(format_out, urls, cx);
    if (!stream)
        return;

	if (np_plist)
	{
		PUT("<b><font size=+3>Installed plug-ins</font></b><br>");
	}
	else
	{
		PUT("<b><font size=+2>No plug-ins are installed.</font></b><br>");
	}
	
	PUT("For more information on plug-ins, <A HREF=http://home.netscape.com/comprod/products/navigator/version_2.0/plugins/index.html>click here</A>.<p><hr>");
	
    for (handle = np_plist; handle; handle = handle->next)
    {
    	np_mimetype* mimetype;
    	
    	PUT("<b><font size=+2>");
    	PUT(handle->name);
    	PUT("</font></b><br>");

		PUT("File name: ");
		PUT(handle->filename);
		PUT("<br>");
		
		PUT("MIME types: <br>");
		PUT("<ul>");
    	for (mimetype = handle->mimetypes; mimetype; mimetype = mimetype->next)
    	{
    		int index = 0;
    		char** extents = mimetype->fassoc->extentlist;
    		
    		PUT("Type: ");
	        PUT(mimetype->type);
	        if (!mimetype->enabled)
	        	PUT(" (disabled)");
	        PUT("<br>");
	        
	        PUT("File extensions: ");
	        while (extents[index])
	        {
	        	PUT(extents[index]);
	        	index++;
	        	if (extents[index])
	        		PUT(", ");
	        }
	        PUT("<br>");
	    }
		PUT("</ul>");
	    
        PUT("<p>");
        PUT("<hr>");
    }
    
    (*stream->complete)(stream);
}

#endif

/* ANTHRAX PUBLIC FUNCTIONS */

/*
	NPL_FindAppletEnabledForMimetype()
	----------------------------------
 	Returns the name of the applet associated AND enabled for the particular mimetype.
	Returns NULL if no applet has been set to handle this mimetype.  This does not mean
	that no Applets have been _installed_ for this mimetype - just that none are enabled.
	
	mimetype : should be of the form "audio/basic", ect.
	
	NOTE: Caller must free returned string.
	
	11.15.97
*/

#ifdef ANTHRAX
char* NPL_FindAppletEnabledForMimetype(const char* mimetype)
{
	char* prefName;
	char* temp;
	char* applet;
	uint32 len;
	int32 loadAction;
	
	prefName = np_CreateMimePref(mimetype, "applet");

	if(PREF_CopyCharPref(prefName, &applet) == PREF_OK)
		{
		/* also check the load action on Mac - this may be XP in the future */
		XP_FREE(prefName);
		prefName = np_CreateMimePref(mimetype, "load_action");
		if(PREF_GetIntPref(prefName, &loadAction) == PREF_OK)
			if(loadAction == 5)
				{
				XP_FREE(prefName);
				return applet;
				}
		}
		
	XP_FREE(prefName);
	return NULL;

}

/* 
	NPL_FindAppletsForType()
	------------------------
	Returns an array of strings specifying the installed Applets for a particular
	mimetype.
	
	mimetype : should be of the form "audio/basic", ect.

	NOTE: Caller must free returned array and strings.

	11.10.97 
*/

char**
NPL_FindAppletsForType(const char* mimetype)
{
	char** result;
	char* appletName;
	int32 numApplets;
	int32 i;
	
	numApplets = np_GetNumberOfInstalledApplets(mimetype);
	if(numApplets == 0)
		return NULL;
	
	result = (char**) XP_ALLOC(numApplets * sizeof(char*));
	if (!result)
		return NULL;
	
	for(i=1; i<=numApplets; i++)
		{
		if((appletName = np_FindAppletNForMimeType(mimetype, (char)i)) == NULL)
			return NULL;
		result[i-1] = appletName;
		}
	result[i-1] = NULL;
		
	return result;
}

/*
	NPL_RegisterAppletType()
	------------------------
	Lets NetLib know that a particular mimetype should be handled by an Applet.
	
	11.10.97
*/

NPError
NPL_RegisterAppletType(NPMIMEType type)
{
	/*
	 * Is this Applet the wildcard (a.k.a. null) plugin?
	 * If so, we don't want to register it for FO_PRESENT
	 * or it will interfere with our normal unknown-mime-
	 * type handling.
	 */
	XP_Bool wildtype = (strcmp(type, "*") == 0);
	np_handle* handle = NULL;
	
	for(handle = np_alist; handle != NULL; handle = handle->next)
		{
		if(!XP_STRCMP(handle->name, type))
			break;
		}

	if(handle == NULL)
		{
		handle = XP_NEW_ZAP(np_handle);
		if (!handle)
			return NPERR_OUT_OF_MEMORY_ERROR;
		
		StrAllocCopy(handle->name, type);
		
		handle->pdesc = NULL;
		handle->next = np_alist;
	   	np_alist = handle;
		}
		
#ifdef XP_WIN
    /* EmbedStream does some Windows FE work and then calls NPL_NewStream */
	if (!wildtype)
        NET_RegisterContentTypeConverter(type, FO_PRESENT, handle, EmbedStream);
    NET_RegisterContentTypeConverter(type, FO_EMBED, handle, EmbedStream); /* XXX I dont think this does anything useful */
#else
	if (!wildtype)
	  {
        NET_RegisterContentTypeConverter(type, FO_PRESENT, handle, NPL_NewPresentStream);
#ifdef XP_UNIX
		/* While printing we use the FO_SAVE_AS_POSTSCRIPT format type. We want
		 * plugin to possibly handle that case too. Hence this.
		 */
        NET_RegisterContentTypeConverter(type, FO_SAVE_AS_POSTSCRIPT, handle,
										 NPL_NewPresentStream);
#endif /* XP_UNIX */
	  }
    NET_RegisterContentTypeConverter(type, FO_EMBED, handle, NPL_NewEmbedStream);
#endif
    NET_RegisterContentTypeConverter(type, FO_PLUGIN, handle, np_newpluginstream);
    NET_RegisterContentTypeConverter(type, FO_BYTERANGE, handle, np_newbyterangestream);
    
    return NPERR_NO_ERROR;
}

/* ANTHRAX STATIC FUNCTIONS */

/*
	np_FindAppletNForMimeType()
	---------------------------
	Returns a string to the Nth installed applet as specified by index.
	Returns NULL if no applet is installed for that type.

	mimetype : should be of the form "audio/basic", ect.

	NOTE: Caller must free returned string.
*/

static char* np_FindAppletNForMimeType(const char* mimetype, char index)
{
	char* prefName;
	char pref[] = { 'a', 'p', 'p', 'l', 'e', 't', '1', '\0'};
	char* applet;
	uint32 len;

	pref[6] = (index+48);
	prefName = np_CreateMimePref(mimetype, pref);

	if(PREF_CopyCharPref(prefName, &applet) == PREF_OK)
		{
		XP_FREE(prefName);
		return applet;
		}
	else
		{
		XP_FREE(prefName);
		return NULL;
		}
}

/*
	np_GetNumberOfInstalledApplets()
	--------------------------------
	Returns the number of Applets that have been installed for a particular
	mimetype.
	
	mimetype : should be of the form "audio/basic", ect.
*/

static int32 np_GetNumberOfInstalledApplets(const char* mimetype)
{
	char* prefName;
	char* temp;
	uint32 len;
	int32 numApplets;
	
	numApplets = 0;
	
	prefName = np_CreateMimePref(mimetype, "num_applets");
	
	PREF_GetIntPref(prefName, &numApplets);

	XP_FREE(prefName);
	return numApplets;
}

/*
	np_CreateMimePref()
	-------------------
	Returns a string formatted in the following way:
	
	"mime.<mimetype>.<pref>"
	
	All '/' and '-' characters in <mimetype> and <pref> are converted to '_'

	Returns NULL if there's a faliure on the allocation of memory.
	
	12.8.97
*/

static char* np_CreateMimePref(const char* mimetype, const char* pref)
{
	uint32 len;
	char* prefName;
	
	len = XP_STRLEN("mime..") + XP_STRLEN(mimetype) + XP_STRLEN(pref);
	
	prefName = XP_ALLOC((len+1)*sizeof(char));
	XP_ASSERT(prefName);
	if(!prefName)
		return NULL;
		
	prefName[0] = 0;
	
	XP_STRCAT(prefName, "mime.");
	XP_STRCAT(prefName, mimetype);
	XP_STRCAT(prefName, ".");
	XP_STRCAT(prefName, pref);

	np_ReplaceChars(prefName, '-', '_');
	np_ReplaceChars(prefName, '/', '_');
	
	return prefName;
}

/*
	np_ReplaceChars()
	-----------------
	Swaps all occurances of oldChar with newChar in word.
	Should this be inline?
*/

static void np_ReplaceChars(char* word, char oldChar, char newChar)
{
	char* index;
	XP_ASSERT(word);
	for(index = word; *index != 0; ++index)
		if(*index == oldChar)
			*index = newChar;
}

#endif	/* ANTHRAX */

#ifdef PLUGIN_TIMER_EVENT

static void np_SetTimerInterval(NPP npp, uint32 msecs)
{
    if(npp) {
        np_instance* instance = (np_instance*) npp->ndata;
        if(instance)
        	{
			instance->interval = msecs;
			instance->timeout = FE_SetTimeout(np_TimerCallback, (void*)instance, instance->interval);
			}
    }	
}

static void np_TimerCallback(void* data)
{
	NPEvent event;
	np_instance* instance = (np_instance*) data;

#ifdef XP_MAC
	((EventRecord)event).what = nullEvent;
#elif defined(XP_WIN)
	event.event = 0; // ?
#elif defined(XP_OS2)
	event.event = 0; // ?
#elif defined(XP_UNIX)
	// not sure what to do here
#endif

	instance->timeout = FE_SetTimeout(np_TimerCallback, (void*)instance, instance->interval);
	NPL_HandleEvent(instance->app, &event, NULL);
}
#endif

