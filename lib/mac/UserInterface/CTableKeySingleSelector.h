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

//	A CTableKeySingleSelector is designed to be attached to an LTableView.
//	When the table (which must also derived from LCommander) is in the
//	target chain, this attachment will handle the following key/command
//	behaviors:
//
//	Keystrokes
//	----------
//		Up arrow:		Select the next cell above the current selection.
//		Down arrow:		Select the next cell below the current selection.

#ifndef CTableKeySingleSelector_H
#define CTableKeySingleSelector_H
#pragma once

// Includes

#include <LAttachment.h>
#include <UTables.h>

// Forward declarations

class LTableView;
struct SCommandStatus;

// CTableKeySingleSelector declaration

class CTableKeySingleSelector : public LAttachment
{
public:
	enum { class_ID = 'Tkss' };
	
							CTableKeySingleSelector(
									LTableView*			inOutlineTable);
							CTableKeySingleSelector(
									LStream*			inStream);
	virtual					~CTableKeySingleSelector();

protected:
	virtual void			ExecuteSelf(
									MessageT			inMessage,
									void*				ioParam);

	virtual void			HandleKeyEvent(
									const EventRecord*	inEvent);

	virtual void			UpArrow();
	virtual void			DownArrow();

	virtual void			ScrollRowIntoFrame(
									TableIndexT			inRow);

	LTableView*				mTable;

};

#endif