/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef CSelectionBroadcastingList_H
#define CSelectionBroadcastingList_H
#pragma once

#include <LListBox.h>

class CSelectionBroadcastingList : public LListBox
{
public:

	enum	{ class_ID = 'Brlb' };

	typedef LListBox super;

	static	void* CreateFromStream(LStream* inStream);
	
						CSelectionBroadcastingList(LStream* inStream);
						~CSelectionBroadcastingList();

	virtual void		SetBroadcastMessage(MessageT inMessage);
	virtual MessageT	GetBroadcastMessage();
	
	virtual void		DontAllowSelectionChange();
	virtual void		AllowSelectionChange();
	
		// PP overrides
	
	virtual void		SetValue(Int32 inValue);

	virtual void		DoNavigationKey(const EventRecord& inKeyEvent);
	virtual void		DoTypeSelection(const EventRecord& inKeyEvent);

	virtual void		SelectOneCell(Cell inCell);

protected:

	virtual void		ClickSelf(const SMouseDownEvent	&inMouseDown);
	
	int					NumCellsSelected();
	
private:
	MessageT	fMessage;
	Boolean		fAllowSelectionChange;
};

#endif