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


#include "UMailSelection.h"

#include "msgcom.h"

//----------------------------------------------------------------------------------------
CMailSelection::CMailSelection()
//----------------------------------------------------------------------------------------
:	selectionList(NULL)
,	xpPane(NULL)
,	selectionSize(0)
,	singleIndex(MSG_VIEWINDEXNONE)
{
}

//----------------------------------------------------------------------------------------
void /*sic*/ CMailSelection::operator=(const CMailSelection& original)
//----------------------------------------------------------------------------------------
{
	xpPane = original.xpPane;
	singleIndex = original.singleIndex;
	if (singleIndex != MSG_VIEWINDEXNONE)
		SetSingleSelection(singleIndex);
	else
	{
		selectionSize = original.selectionSize;
		selectionList = const_cast<MSG_ViewIndex*>(original.GetSelectionList());
	}
}

//----------------------------------------------------------------------------------------
const MSG_ViewIndex* CMailSelection::GetSelectionList() const
//----------------------------------------------------------------------------------------
{
	return selectionList;
} // CMailSelection::GetSelectionList

//----------------------------------------------------------------------------------------
void CMailSelection::Normalize()
//----------------------------------------------------------------------------------------
{
	// If singleIndex is a real index, then fix selectionList to
	// point to it (eg, after assignment or fetching the bits out of
	// a drag object)
	if (singleIndex != MSG_VIEWINDEXNONE)
		SetSingleSelection(singleIndex);
}

//----------------------------------------------------------------------------------------
CPersistentMailSelection::~CPersistentMailSelection()
// Can't be implemented inline, because it's a virtual function in a shared header file.
//----------------------------------------------------------------------------------------
{
	DestroyData();
} // CPersistentMailSelection::~CPersistentMailSelection

//----------------------------------------------------------------------------------------
const MSG_ViewIndex* CPersistentMailSelection::GetSelectionList() const
//----------------------------------------------------------------------------------------
{
	CPersistentMailSelection* s = const_cast<CPersistentMailSelection*>(this);
	Assert_(s);
	s->MakeAllVolatile();
	return selectionList;
} // CPersistentMailSelection::GetSelectionList

//----------------------------------------------------------------------------------------
void CPersistentMailSelection::CloneData()
//----------------------------------------------------------------------------------------
{
	if (selectionSize == 0 || singleIndex != MSG_VIEWINDEXNONE)
		return; // nothing to do
	MSG_ViewIndex* src = selectionList; // owned by somebody else
	selectionList = new MSG_ViewIndex[selectionSize];
	MSG_ViewIndex* dst = selectionList;
	for (int i = 0; i < selectionSize; i++,dst++)
		*dst = *src++;
}

//----------------------------------------------------------------------------------------
void CPersistentMailSelection::DestroyData()
//----------------------------------------------------------------------------------------
{
	// You must call this if you called CloneData.
	if (selectionSize != 0 && singleIndex == MSG_VIEWINDEXNONE)
		delete [] selectionList;
}
//----------------------------------------------------------------------------------------
void CPersistentFolderSelection::MakePersistent(MSG_ViewIndex& ioKey) const
//----------------------------------------------------------------------------------------
{
	ioKey = (MSG_ViewIndex)::MSG_GetFolderInfo(
				xpPane,
				ioKey);
}

//----------------------------------------------------------------------------------------
void CPersistentFolderSelection::MakeVolatile(MSG_ViewIndex& ioKey) const
//----------------------------------------------------------------------------------------
{
	ioKey = ::MSG_GetFolderIndexForInfo(
		xpPane, 
		(MSG_FolderInfo *)ioKey,
		true);
}

//----------------------------------------------------------------------------------------
void CPersistentMessageSelection::MakePersistent(MSG_ViewIndex& ioKey) const
//----------------------------------------------------------------------------------------
{
	ioKey = (MSG_ViewIndex)::MSG_GetMessageKey(
				xpPane,
				ioKey);
}

//----------------------------------------------------------------------------------------
void CPersistentMessageSelection::MakeVolatile(MSG_ViewIndex& ioKey) const
//----------------------------------------------------------------------------------------
{
	ioKey = ::MSG_GetMessageIndexForKey(
				xpPane,
				(MessageKey)ioKey,
				true);
}
