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

#ifndef MKEXTCACHE_H
#define MKEXTCACHE_H

/* lookup routine
 *
 * builds a key and looks for it in
 * the database.  Returns an access
 * method and sets a filename in the
 * URL struct if found
 */
extern int NET_FindURLInExtCache(URL_Struct * URL_s, MWContext *ctxt);

extern void
NET_OpenExtCacheFAT(MWContext *ctxt, char * cache_name, char * instructions);

extern void
CACHE_CloseAllOpenSARCache();

extern void
CACHE_OpenAllSARCache();

#endif /* MKEXTCACHE_H */
