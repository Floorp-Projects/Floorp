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