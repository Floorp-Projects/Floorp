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

#ifndef _SubLine_H_
#define _SubLine_H_

#include "ptrarray.h"

/* This is a class used only by the MSG_SubscribePane class. */

class msg_GroupRecord;

class msg_SubscribeLine {
public:
    msg_SubscribeLine(msg_GroupRecord* group, int16 depth,
					  XP_Bool issubscribed, int32 numsubgroups);


	msg_GroupRecord* GetGroup() {return m_group;}
	int16 GetDepth() {return m_depth;}
	int32 GetNumMessages() {return m_nummessages;}
	void SetNumMessages(int32 value) {m_nummessages = value;}
	int32 GetNumSubGroups() {return m_numsubgroups;}
	void AddNewSubGroup() { m_numsubgroups++; }

	XP_Bool CanExpand();

	XP_Bool GetSubscribed();
	void SetSubscribed(XP_Bool value);


protected:
    msg_GroupRecord* m_group;
    int32 m_nummessages;			// -1 means unknown.
	int16 m_depth;
    uint16 m_flags;
	int32 m_numsubgroups;
};



class msg_SubscribeLineArray : public XPPtrArray {
public:
	msg_SubscribeLineArray();

	msg_SubscribeLine* operator[](int nIndex) const {
		return (msg_SubscribeLine*) XPPtrArray::GetAt(nIndex);
	}
	msg_SubscribeLine*& operator[](int nIndex) {
		return (msg_SubscribeLine*&) XPPtrArray::ElementAt(nIndex);
	}
};


#endif /* _SubLine_H_ */
