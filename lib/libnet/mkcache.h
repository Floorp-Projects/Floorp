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

#ifndef MKCACHE_H
#define MKCACHE_H

#include "mkgeturl.h"

#ifndef EXT_CACHE_H
#include "extcache.h"
#endif

XP_BEGIN_PROTOS

extern void   NET_CleanupCache (char * filename);
extern int    NET_FindURLInCache(URL_Struct * URL_s, MWContext *ctxt);
extern void   NET_RefreshCacheFileExpiration(URL_Struct * URL_s);

/* read the Cache File allocation table.
 */
extern void NET_ReadCacheFAT(char * cachefatfile, Bool stat_files);

/* remove a URL from the cache
 */
extern void NET_RemoveURLFromCache(URL_Struct *URL_s);

/* create an HTML stream and push a bunch of HTML about
 * the cache
 */
extern void NET_DisplayCacheInfoAsHTML(ActiveEntry * cur_entry);

/* trace variable for cache testing */
extern XP_Bool NET_CacheTraceOn;

/* public accessor function for netcaster */
extern Bool NET_CacheStore(net_CacheObject *cacheObject, URL_Struct *url_s, Bool accept_partial_files);

/* return TRUE if the URL is in the cache and
 * is a partial cache file
 */ 
extern Bool NET_IsPartialCacheFile(URL_Struct *URL_s);

/* encapsulated access to the first object in cache_database */
extern int NET_FirstCacheObject(DBT *key, DBT *data);

/* encapsulated access to the next object in the cache_database */
extern int NET_NextCacheObject(DBT *key, DBT *data);

/* Max size for displaying in the cache browser */
extern int32 NET_GetMaxDiskCacheSize();

XP_END_PROTOS

#endif /* MKCACHE_H */
