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

/*   msgtypes.h --- types for the mail/news reader module.
 */

#ifndef _MSGTYPES_H_
#define _MSGTYPES_H_

/* This file defines types that are used by libmsg.  Actually, it's rather
   underpopulated right now; much more should be moved here from msgcom.h. */




/* Instances of MSG_Pane are used to represent the various panes in the user
   interfaces.  The FolderPanes and MessagePanes must have a context associated
   with them, but the ThreadPane generally does not.  MSG_Pane is deliberately
   an opaque type; FE's can't manipulate them except via the calls defined
   here. */

#ifdef XP_CPLUSPLUS
class MSG_Pane;
#else
typedef struct MSG_Pane MSG_Pane;
#endif



#endif /* _MSGTYPES_H_ */
