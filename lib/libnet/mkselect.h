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

#ifndef MKSELECT_H
#define MKSELECT_H

XP_BEGIN_PROTOS

extern void NET_SetReadPoll(PRFileDesc *fd); 

extern void NET_ClearReadPoll(PRFileDesc *fd);

extern void NET_SetConnectPoll(PRFileDesc *fd);

extern void NET_ClearConnectPoll(PRFileDesc *fd); 

/* this function turns on and off a reasonably slow timer that will
 * push the netlib along even when it doesn't get any onIdle time.
 * this is unfortunately necessary on windows because when a modal
 * dialog is up it won't call the OnIdle loop which is currently the
 * source of our events.
 */
extern void NET_SetNetlibSlowKickTimer(XP_Bool set);

/* set and clear the callnetliballthetime busy poller.
 * all reference counting is done internally
 *
 * the caller string is used in debug builds to detect callers that
 * don't set and clear correctly
 */
extern void NET_SetCallNetlibAllTheTime(MWContext *context, char *caller);
extern void NET_ClearCallNetlibAllTheTime(MWContext *context, char *caller);
extern XP_Bool NET_IsCallNetlibAllTheTimeSet(MWContext *context, char *caller);

extern void NET_SetReadSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_ClearReadSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_SetFileReadSelect(MWContext *context, int file_desc);
extern void NET_ClearFileReadSelect(MWContext *context, int file_desc);
extern void NET_SetConnectSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_ClearConnectSelect(MWContext *context, PRFileDesc *file_desc);
extern void NET_ClearDNSSelect(MWContext *context, PRFileDesc *file_desc);

XP_END_PROTOS

#endif /* MKSELECT_H */
