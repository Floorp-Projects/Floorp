/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsMsgFilterCore_H_
#define _nsMsgFilterCore_H_

#include "MailNewsTypes.h"
#include "nsString2.h"


typedef enum
 {
	 nsMsgFilterActionNone,				/* uninitialized state */
     nsMsgFilterActionMoveToFolder,
     nsMsgFilterActionChangePriority,
     nsMsgFilterActionDelete,
     nsMsgFilterActionMarkRead,
     nsMsgFilterActionKillThread,
	 nsMsgFilterActionWatchThread
 } nsMsgRuleActionType;

typedef enum 
{
	nsMsgFilterInboxRule = 0x1,
	nsMsgFilterInboxJavaScript = 0x2,
	nsMsgFilterInbox = 0x3,
	nsMsgFilterNewsRule = 0x4,
	nsMsgFilterNewsJavaScript = 0x8,
	nsMsgFilterNews=0xb,
	nsMsgFilterAll=0xf
} nsMsgFilterType;

typedef enum
{
	nsMsgFilterUp,
	nsMsgFilterDown
} nsMsgFilterMotion;

typedef PRInt32 nsMsgFilterIndex;

// shouldn't need these...probably should be interfaces.
struct nsMsgFilter;
struct nsMsgRule;
struct nsMsgRuleAction;
struct nsMsgFilterList;

#endif

