/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 /* -*- Mode: C; tab-width: 4 -*-
   mimeenc.c --- MIME encoders and decoders, version 2 (see mimei.h)
   Copyright © 1996 Netscape Communications Corporation, all rights reserved.
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#ifndef _MIMEENC_H_
#define _MIMEENC_H_

#include "prtypes.h"
#include "nsError.h"

/* This file defines interfaces to generic implementations of Base64, 
   Quoted-Printable, and UU decoders; and of Base64 and Quoted-Printable
   encoders.
 */


/* Opaque objects used by the encoder/decoder to store state. */
typedef struct MimeDecoderData MimeDecoderData;
typedef struct MimeEncoderData MimeEncoderData;


/* functions for creating that opaque data.
 */
MimeDecoderData *MimeB64DecoderInit(nsresult (*output_fn) (const char *buf,
													  PRInt32 size,
													  void *closure),
									void *closure);
MimeDecoderData *MimeQPDecoderInit (nsresult (*output_fn) (const char *buf,
													  PRInt32 size,
													  void *closure),
									void *closure);
MimeDecoderData *MimeUUDecoderInit (nsresult (*output_fn) (const char *buf,
													  PRInt32 size,
													  void *closure),
									void *closure);

MimeEncoderData *MimeB64EncoderInit(nsresult (*output_fn) (const char *buf,
													  PRInt32 size,
													  void *closure),
									void *closure);
MimeEncoderData *MimeQPEncoderInit (nsresult (*output_fn) (const char *buf,
													  PRInt32 size,
													  void *closure),
									void *closure);
MimeEncoderData *MimeUUEncoderInit (char *filename,
									nsresult (*output_fn) (const char *buf,
													  PRInt32 size,
													  void *closure),
									void *closure);

/* Push data through the encoder/decoder, causing the above-provided write_fn
   to be called with encoded/decoded data. */
int MimeDecoderWrite (MimeDecoderData *data, const char *buffer, PRInt32 size);
int MimeEncoderWrite (MimeEncoderData *data, const char *buffer, PRInt32 size);

/* When you're done encoding/decoding, call this to free the data.  If
   abort_p is PR_FALSE, then calling this may cause the write_fn to be called
   one last time (as the last buffered data is flushed out.)
 */
int MimeDecoderDestroy(MimeDecoderData *data, PRBool abort_p);
int MimeEncoderDestroy(MimeEncoderData *data, PRBool abort_p);

#endif /* _MODMIMEE_H_ */
