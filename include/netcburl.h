/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
   This file defines the way of getting a URL that you can put into generated
   HTML.  If the user accesses that URL, then the callback function you
   provided will get called with the data you provided.

   Currently, this is all implemented in lib/libnet/mkcburl.c.
*/

/* make sure we only include this once */
#ifndef _NET_CBURL_H_
#define _NET_CBURL_H_

XP_BEGIN_PROTOS

typedef void (*NET_CallbackURLFunc)(void* closure, const char* url);


/* Creates a callback URL.  Returns a string which is a valid URL that you
   can emit into an HTML stream.  (Returns NULL on out-of-memory error.)
   The returned string must be freed using XP_FREE().  If the returned URL
   is ever processed, then the given function will be called with the given
   closure.  The actual URL string is also passed, so that the function
   can process any form data and the like. */

char*
NET_CallbackURLCreate(NET_CallbackURLFunc func, void* closure);



/* Informs the callback system that the given function and closure are no
   longer valid.  This frees all storage associated with the above call,
   and must be called at some point or a memory leak will result. */

int
NET_CallbackURLFree(NET_CallbackURLFunc func, void* closure);

XP_END_PROTOS

#endif /* _NET_CBURL_H_ */
