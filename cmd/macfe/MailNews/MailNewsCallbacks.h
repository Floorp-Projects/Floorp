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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// MailNewsCallbacks.h

#pragma once

#include <LBroadcaster.h>
#include <LSharable.h>

#include "msgcom.h"

//======================================
class CMailCallbackManager : public LBroadcaster, public LSharable
//======================================
{

public:

	enum
		{
			msg_PaneChanged		= 'PnCh'
		,	msg_ChangeFinished	= 'ChFn'
		,	msg_ChangeStarting	= 'ChSt'
		};
								CMailCallbackManager();
	virtual						~CMailCallbackManager();
	static CMailCallbackManager*	Get();

public:

	void						PaneChanged(
									MSG_Pane *inPane,
									XP_Bool asynchronous, 
									MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
									int32 value);
	void						ChangeStarting(
									MSG_Pane*		inPane, 
									XP_Bool 		inAsync,
									MSG_NOTIFY_CODE inNotifyCode, 
									MSG_ViewIndex 	inWhere,
									int32 			inCount);
	void						ChangeFinished(
									MSG_Pane*		inPane, 
									XP_Bool 		inAsync,
									MSG_NOTIFY_CODE inNotifyCode, 
									MSG_ViewIndex 	inWhere,
									int32 			inCount);
private:

	Boolean			ValidData(MSG_Pane *inPane);

protected:

	static CMailCallbackManager* sMailCallbackManager;
};

//-----------------------------------
struct SMailCallbackInfo
//-----------------------------------
{
	 MSG_Pane* pane;
	 XP_Bool async;

	 SMailCallbackInfo(MSG_Pane* inPane, XP_Bool inAsync) : pane(inPane), async(inAsync){}
};

//-----------------------------------
struct SPaneChangeInfo : public SMailCallbackInfo
//-----------------------------------
{
	 MSG_PANE_CHANGED_NOTIFY_CODE notifyCode;
	 int32 value;

	 SPaneChangeInfo(
	 	MSG_Pane* inPane,
	 	XP_Bool inAsync,
	 	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	 	int32 inValue)
	 	:	SMailCallbackInfo(inPane, inAsync)
	 	,	notifyCode(inNotifyCode)
	 	,	value(inValue) {}
};

typedef	Uint32 TableIndexT;

//-----------------------------------
struct SLineChangeInfo : public SMailCallbackInfo
//-----------------------------------
{
	MSG_NOTIFY_CODE changeCode;
	MSG_ViewIndex 	startRow;
	int32 			rowCount;

	 SLineChangeInfo(
	 	MSG_Pane* inPane,
	 	XP_Bool inAsync,
	 	MSG_NOTIFY_CODE inChangeCode,
	 	TableIndexT inStartRow,
	 	int32 inCount)
	 	:	SMailCallbackInfo(inPane, inAsync)
	 	,	changeCode(inChangeCode)
	 	,	startRow(inStartRow)
	 	,	rowCount(inCount) {}
};

//======================================
class CMailCallbackListener : public LListener
//======================================
{
protected:
					CMailCallbackListener();
public:
					~CMailCallbackListener();
	void			SetPane(MSG_Pane* inPane) { mPane = inPane; }
protected:
	virtual void	ListenToMessage(MessageT message, void* ioParam);
	virtual Boolean	IsMyPane(void* info) const; // info from a ListenToMessage...
private:
	void			PaneChanged(void* ioParam);
	void			ChangeStarting(void* ioParam);
	void			ChangeFinished(void* ioParam);

	virtual void	ChangeStarting(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount);
	virtual void	ChangeFinished(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount);
	virtual void 	PaneChanged(
		MSG_Pane*		inPane,
		MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
		int32 value) = 0; // must always be supported.
private:
	MSG_Pane* mPane;
};
