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

// miconutlis.cp

// icon utilities

#include "miconutils.h"
#include "prtypes.h"

struct IconRecord	{
	Int16	fId;
	Handle fIcon;	// CIconHandle for IconSuites
	Int16 fRefCount;
};

CIconList CIconList::sIconList;
CIconList CIconList::sIconSuiteList;

CIconList::CIconList() : LArray(sizeof(IconRecord)) {}

CIconList::~CIconList() {}

CIconHandle CIconList::GetIcon(Int16 iconID)
{	
	IconRecord theIcon;
	LArrayIterator iter(sIconList);
	while (iter.Next(&theIcon))
		if (theIcon.fId == iconID)
		{
			Int32 where = sIconList.FetchIndexOf(&theIcon);	// Increase the refcount by 
													// doing a replace
			sIconList.RemoveItemsAt(1, where);
			theIcon.fRefCount += 1;
			sIconList.InsertItemsAt(1, 1, &theIcon);
			return (CIconHandle)theIcon.fIcon;
		}
	// Did not find the icon. Create new one
	theIcon.fId = iconID;
	theIcon.fIcon = (Handle)::GetCIcon (iconID);
	if (theIcon.fIcon == nil)
		return nil;
	theIcon.fRefCount = 1;
	sIconList.InsertItemsAt(1, 1, &theIcon);
	return (CIconHandle)theIcon.fIcon;
}

void CIconList::ReturnIcon(CIconHandle iconH)
{
	IconRecord theIcon;
	LArrayIterator iter(sIconList);
	while (iter.Next(&theIcon))
		if (theIcon.fIcon == (Handle)iconH)
		{
			Int32 where = sIconList.FetchIndexOf(&theIcon);	// Increase the refcount by 
													// doing a replace
			theIcon.fRefCount -= 1;
			sIconList.RemoveItemsAt(1, where);
			if (theIcon.fRefCount == 0)
				DisposeCIcon(iconH);
			else
				sIconList.InsertItemsAt(1, 1, &theIcon);
		}
}

Handle CIconList::GetIconSuite(Int16 iconID)
{	
	IconRecord theIcon;
	LArrayIterator iter(sIconSuiteList);
	while (iter.Next(&theIcon))
		if (theIcon.fId == iconID)
		{
			Int32 where = sIconSuiteList.FetchIndexOf(&theIcon);	// Increase the refcount by 
													// doing a replace
			sIconSuiteList.RemoveItemsAt(1, where);
			theIcon.fRefCount += 1;
			sIconSuiteList.InsertItemsAt(1, 1, &theIcon);
			return theIcon.fIcon;
		}
	// Did not find the icon. Create new one
	theIcon.fId = iconID;
	OSErr err =	::GetIconSuite(&theIcon.fIcon, iconID, svAllAvailableData);
	if (err != noErr)
		return nil;
	theIcon.fRefCount = 1;
	sIconSuiteList.InsertItemsAt(1, 1, &theIcon);
	return theIcon.fIcon;
}

void CIconList::ReturnIconSuite(Handle iconH)
{
	IconRecord theIcon;
	LArrayIterator iter(sIconSuiteList);
	while (iter.Next(&theIcon))
		if (theIcon.fIcon == iconH)
		{
			Int32 where = sIconSuiteList.FetchIndexOf(&theIcon);	// Increase the refcount by 
													// doing a replace
			theIcon.fRefCount -= 1;
			sIconSuiteList.RemoveItemsAt(1, where);
			if (theIcon.fRefCount == 0)
				DisposeIconSuite(iconH, TRUE);
			else
				sIconSuiteList.InsertItemsAt(1, 1, &theIcon);
		}
}

