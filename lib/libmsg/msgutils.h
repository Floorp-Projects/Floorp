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
/*   msgutils.h --- various and sundry
 */

XP_BEGIN_PROTOS

/* Interface to FE_GetURL, which should never be called directly by anything
   else in libmsg.  The "issafe" argument is TRUE if this is a URL which we
   consider very safe, and will never screw up or be screwed by any activity
   going on in the foreground.  Be very very sure of yourself before ever
   passing TRUE to the "issafe" argument. */

extern int msg_GetURL(MWContext* context, URL_Struct* url, XP_Bool issafe);


/* Interface to XP_InterruptContext(), which should never be called directly
   by anything else in libmsg.  The "safetoo" argument is TRUE if we really
   want to interrupt everything, even "safe" background streams.  In most
   cases, it should be False.*/

extern void msg_InterruptContext(MWContext* context, XP_Bool safetoo);



extern int msg_GrowBuffer (uint32 desired_size,
						   uint32 element_size, uint32 quantum,
						   char **buffer, uint32 *size);

extern int msg_LineBuffer (const char *net_buffer, int32 net_buffer_size,
						   char **bufferP, uint32 *buffer_sizeP,
						   uint32 *buffer_fpP,
						   XP_Bool convert_newlines_p,
						   int32 (*per_line_fn) (char *line, uint32
												 line_length, void *closure),
						   void *closure);
						   
extern int msg_ReBuffer (const char *net_buffer, int32 net_buffer_size,
						 uint32 desired_buffer_size,
						 char **bufferP, uint32 *buffer_sizeP,
						 uint32 *buffer_fpP,
						 int32 (*per_buffer_fn) (char *buffer,
												 uint32 buffer_size,
												 void *closure),
						 void *closure);

extern NET_StreamClass *msg_MakeRebufferingStream (NET_StreamClass *next,
												   URL_Struct *url,
												   MWContext *context);

/* Given a string and a length, removes any "Re:" strings from the front.
   (If the length is not given, then XP_STRLEN() is used on the string.)
   It also deals with that "Re[2]:" thing that some mailers do.

   Returns TRUE if it made a change, FALSE otherwise.

   The string is not altered: the pointer to its head is merely advanced,
   and the length correspondingly decreased.
 */
extern XP_Bool msg_StripRE(const char **stringP, uint32 *lengthP);

/* 
 * Does in-place modification of input param to conform with son-of-1036 rules
 */
extern void msg_MakeLegalNewsgroupComponent (char *name);

extern void MSG_SetPercentProgress(MWContext *context, int32 numerator, int32 denominator);

XP_END_PROTOS
