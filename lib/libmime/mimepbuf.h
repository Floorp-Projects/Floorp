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

/* mimepbuf.h --- utilities for buffering a MIME part for later display.
   Created: Jamie Zawinski <jwz@netscape.com>, 24-Sep-96.
 */

#ifndef _MIMEPBUF_H_
#define _MIMEPBUF_H_

#include "mimei.h"

/* This file provides the ability to save up the entire contents of a MIME
   object (of arbitrary size), and then emit it all at once later.  The
   buffering is done in an efficient way that works well for both very large
   and very small objects.

   This is used in two places:

   = The implementation of multipart/alternative uses this code to do a
     one-part-lookahead.  As it traverses its children, it moves forward
     until it finds a part which cannot be displayed; and then it displays
     the *previous* part (the last which *could* be displayed.)  This code
     is used to hold the previous part until it is needed.

   = The S/MIME code (both MimeEncryptedPKCS7 and MimeMultipartSignedPKCS7)
     use this code to delay presenting an object to the user until its
     signature has been verified.  The signature cannot be completely verified
     by the underlying crypto code until the entire object has been read;
     however, it would be wrong to present a signed object to the user without
     first knowing whether the signature is correct (in other words, we want
     to present the "signature matches" or "signature does not match" blurb to
     the user *before* we show them the object which has been signed, rather
     than *after*.)
 */

/* An opaque object used to represent the buffered data.
 */
typedef struct MimePartBufferData MimePartBufferData;

/* Create an empty part buffer object.
 */
extern MimePartBufferData *MimePartBufferCreate (void);

/* Assert that the buffer is now full (EOF has been reached on the current
   part.)  This will free some resources, but leaves the part in the buffer.
   After calling MimePartBufferReset, the buffer may be used to store a
   different object.
 */
void MimePartBufferClose (MimePartBufferData *data);

/* Reset a part buffer object to the default state, discarding any currently-
   buffered data.
 */
extern void MimePartBufferReset (MimePartBufferData *data);

/* Free the part buffer itself, and discard any buffered data.
 */
extern void MimePartBufferDestroy (MimePartBufferData *data);

/* Push a chunk of a MIME object into the buffer.
 */
extern int MimePartBufferWrite (MimePartBufferData *data,
								const char *buf, int32 size);

/* Read the contents of the buffer back out.  This will invoke the provided
   read_fn with successive chunks of data until the buffer has been drained.
   The provided function may be called once, or multiple times.
 */
extern int
MimePartBufferRead (MimePartBufferData *data,
					int (*read_fn) (char *buf, int32 size, void *closure),
					void *closure);

#endif /* _MIMEPBUF_H_ */
