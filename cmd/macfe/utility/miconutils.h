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

// miconutils.h

// Retreival/disposal of icons

// Holds the icons
// disposes if they are not referenced

#include <LArray.h>

class CIconList : public LArray	{
public:
// ее icon interface

	static CIconHandle		GetIcon(ResIDT iconID);
	static void			ReturnIcon(CIconHandle icon);
	static Handle		GetIconSuite(ResIDT iconID);
	static void			ReturnIconSuite(Handle icon);

private:
					CIconList();
	virtual			~CIconList();
	static CIconList sIconList;
	static CIconList sIconSuiteList;
};



class CIconUtils
{
public:

	static void 	GetDesktopIconSuite( const OSType inFileCreator, const OSType inFileType,
										const short inIconSize, Handle* outHandle );

private:
	static Boolean	FindDesktopDatabase(short *ioVolumeRefNum, const OSType inFileCreator);
	static Boolean	InOneDesktop(short *ioVolumeRefNum, const OSType inFileCreator, short *outDtRefNum);
};


class CAttachmentIcon
{
public:
	enum {
		kIconSizeSmall = kSmallIcon,
		kIconSizeLarge = kLargeIcon
	};
	
	CAttachmentIcon();
	CAttachmentIcon(const OSType inFileCreator, const OSType inFileType, const Int16 inIconSize = kIconSizeLarge);
	CAttachmentIcon(const ResID inIconResID, const Int16 inIconSize = kIconSizeLarge);
	CAttachmentIcon(const char* attachmentRealType, const Int16 inIconSize = kIconSizeLarge);
	CAttachmentIcon( const CAttachmentIcon& inOriginal);		//copy constructor
	
	~CAttachmentIcon();
	
	Handle GetSuiteHandle()	{ return mIconSuiteHandle; }
	
	void PlotIcon(const Rect& inIconFrame, IconAlignmentType inIconAlign = kAlignNone,
											IconTransformType inIconTransform = kTransformNone);
	
	void AddIconOutlineToRegion( RgnHandle inRgnHandle, const Int16 inIconSize = kIconSizeLarge);
	
protected:

private:
	Handle		mIconSuiteHandle;

};
