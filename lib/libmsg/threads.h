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
/*  
	This file is no longer used and should be removed.
 */

#ifndef _THREADS_H_
#define _THREADS_H_

#include "msg.h"
#include "xp_hash.h"

struct msg_thread_state; /* opaque */

/* When we need to create/destry a synthetic ThreadEntry, we call these. */
typedef MSG_ThreadEntry * (*msg_DummyThreadEntryCreator) (void *arg);
typedef void (*msg_DummyThreadEntryDestroyer) (struct MSG_ThreadEntry *dummy,
											   void *arg);

XP_BEGIN_PROTOS

/* Creates and initializes a state object to hold the thread data.
   The first argument should be the caller's best guess as to how
   many messages there will be; it is used for the initial size of
   the hash tables, so it's ok for it to be too small.  The other
   arguments specify the sorting behavior.
 */
extern struct msg_thread_state *
msg_BeginThreading (MWContext* context,
					uint32 message_count_guess,
					XP_Bool news_p,
					MSG_SORT_KEY sort_key,
					XP_Bool sort_forward_p,
					XP_Bool thread_p,
					msg_DummyThreadEntryCreator dummy_creator,
					void *dummy_creator_arg);

/* Add a message to the thread data.
   The msg->next slot will be ignored (and not modified.)
 */
extern int
msg_ThreadMessage (struct msg_thread_state *state,
				   char **string_table,
				   struct MSG_ThreadEntry *msg);

/* Threads the messages, discards the state object, and returns a
   tree of MSG_ThreadEntry objects.
   The msg->next and msg->first_child slots of all messages will
   be overwritten.
 */
extern struct MSG_ThreadEntry *
msg_DoneThreading (struct msg_thread_state *state,
				   char **string_table);

/* Given an existing tree of MSG_ThreadEntry structures, re-sorts them.
   This changes the `next' and `first_child' links, and may create or
   destroy "dummy" thread entries (those with the EXPIRED flag set.)
   The new root of the tree is returned.
 */
extern struct MSG_ThreadEntry *
msg_RethreadMessages (MWContext* context,
					  struct MSG_ThreadEntry *tree,
					  uint32 approximate_message_count,
					  char **string_table,
					  XP_Bool news_p,
					  MSG_SORT_KEY sort_key, XP_Bool sort_forward_p,
					  XP_Bool thread_p,
					  msg_DummyThreadEntryCreator dummy_creator,
					  msg_DummyThreadEntryDestroyer dummy_destroyer,
					  void *dummy_arg);

XP_END_PROTOS

#endif /* _THREADS_H_ */
