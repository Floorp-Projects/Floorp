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

/* mimeenc.c --- MIME encoders and decoders, version 2 (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */


#ifndef _MIMEENC_H_
#define _MIMEENC_H_

#include "xp.h"

/* This file defines interfaces to generic implementations of Base64, 
   Quoted-Printable, and UU decoders; and of Base64 and Quoted-Printable
   encoders.
 */


/* Opaque objects used by the encoder/decoder to store state. */
typedef struct MimeDecoderData MimeDecoderData;
typedef struct MimeEncoderData MimeEncoderData;


XP_BEGIN_PROTOS

/* functions for creating that opaque data.
 */
MimeDecoderData *MimeB64DecoderInit(int (*output_fn) (const char *buf,
													  int32 size,
													  void *closure),
									void *closure);
MimeDecoderData *MimeQPDecoderInit (int (*output_fn) (const char *buf,
													  int32 size,
													  void *closure),
									void *closure);
MimeDecoderData *MimeUUDecoderInit (int (*output_fn) (const char *buf,
													  int32 size,
													  void *closure),
									void *closure);

MimeEncoderData *MimeB64EncoderInit(int (*output_fn) (const char *buf,
													  int32 size,
													  void *closure),
									void *closure);
MimeEncoderData *MimeQPEncoderInit (int (*output_fn) (const char *buf,
													  int32 size,
													  void *closure),
									void *closure);
MimeEncoderData *MimeUUEncoderInit (char *filename,
									int (*output_fn) (const char *buf,
													  int32 size,
													  void *closure),
									void *closure);

/* Push data through the encoder/decoder, causing the above-provided write_fn
   to be called with encoded/decoded data. */
int MimeDecoderWrite (MimeDecoderData *data, const char *buffer, int32 size);
int MimeEncoderWrite (MimeEncoderData *data, const char *buffer, int32 size);

/* When you're done encoding/decoding, call this to free the data.  If
   abort_p is FALSE, then calling this may cause the write_fn to be called
   one last time (as the last buffered data is flushed out.)
 */
int MimeDecoderDestroy(MimeDecoderData *data, XP_Bool abort_p);
int MimeEncoderDestroy(MimeEncoderData *data, XP_Bool abort_p);

XP_END_PROTOS

#endif /* _MIMEENC_H_ */
