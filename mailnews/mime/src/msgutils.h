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

extern int msg_GrowBuffer (PRUint32 desired_size,
						   PRUint32 element_size, PRUint32 quantum,
						   char **buffer, PRUint32 *size);

extern int msg_LineBuffer (const char *net_buffer, PRInt32 net_buffer_size,
						   char **bufferP, PRUint32 *buffer_sizeP,
						   PRUint32 *buffer_fpP,
						   PRBool convert_newlines_p,
						   PRInt32 (*per_line_fn) (char *line, PRUint32
												 line_length, void *closure),
						   void *closure);
						   
extern int msg_ReBuffer (const char *net_buffer, PRInt32 net_buffer_size,
						 PRUint32 desired_buffer_size,
						 char **bufferP, PRUint32 *buffer_sizeP,
						 PRUint32 *buffer_fpP,
						 PRInt32 (*per_buffer_fn) (char *buffer,
												 PRUint32 buffer_size,
												 void *closure),
						 void *closure);

extern NET_StreamClass *msg_MakeRebufferingStream (NET_StreamClass *next,
												   URL_Struct *url,
												   MWContext *context);

/* Given a string and a length, removes any "Re:" strings from the front.
   (If the length is not given, then PL_strlen() is used on the string.)
   It also deals with that "Re[2]:" thing that some mailers do.

   Returns PR_TRUE if it made a change, PR_FALSE otherwise.

   The string is not altered: the pointer to its head is merely advanced,
   and the length correspondingly decreased.
 */
extern PRBool msg_StripRE(const char **stringP, PRUint32 *lengthP);

/* 
 * Does in-place modification of input param to conform with son-of-1036 rules
 */
extern void msg_MakeLegalNewsgroupComponent (char *name);

/*
 * UnHex a char hex value into an integer value.
 */
extern int msg_UnHex(char c);

XP_END_PROTOS
