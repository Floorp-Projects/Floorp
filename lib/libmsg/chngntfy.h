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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _ChngNtfy_H_
#define _ChngNtfy_H_

#include "ptrarray.h"

class ChangeListener;
class MessageDBView;

class ChangeAnnouncer : public XPPtrArray  // array of ChangeListeners
{
public:
	ChangeAnnouncer();
	virtual ~ChangeAnnouncer();
	virtual XP_Bool AddListener(ChangeListener *listener);
	virtual XP_Bool RemoveListener(ChangeListener *listener);

	// convenience routines to notify all our ChangeListeners
	void NotifyViewChangeAll(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	void NotifyViewStartChangeAll(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	void NotifyViewEndChangeAll(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	void NotifyKeyChangeAll(MessageKey keyChanged, int32 flags, 
		ChangeListener *instigator);
	void NotifyAnnouncerGoingAway(ChangeAnnouncer *instigator);
	void NotifyAnnouncerChangingView(ChangeAnnouncer *instigator, MessageDBView *newView);
};

class ChangeListener
{
public:
	ChangeListener();
	virtual ~ChangeListener();
	// do nothing notification routines - subclass to do something.
	// New notification routines need to be added here as do nothing routines
	virtual void OnViewChange(MSG_ViewIndex /*startInd*/, int32 /*numChanged*/, 
		MSG_NOTIFY_CODE /* changeType */, ChangeListener * /*instigator*/);
	virtual void OnViewStartChange(MSG_ViewIndex /*startInd*/, int32 /*numChanged*/, 
		MSG_NOTIFY_CODE /* changeType */, ChangeListener * /*instigator*/) ;
	virtual void OnViewEndChange(MSG_ViewIndex /*startInd*/, int32 /*numChanged*/, 
		MSG_NOTIFY_CODE /* changeType */, ChangeListener * /*instigator*/) ;
	virtual void OnKeyChange(MessageKey /*keyChanged*/, int32 /*flags*/, 
		ChangeListener * /*instigator*/) ;
	virtual void OnAnnouncerGoingAway(ChangeAnnouncer * /*instigator*/) ;
	virtual void OnAnnouncerChangingView(ChangeAnnouncer * /* instigator */, MessageDBView * /* view */) ;
};

#endif
