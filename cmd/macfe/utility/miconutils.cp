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
#include "xp_mcom.h"
#include "net.h"
#include "umimemap.h"
#include "uprefd.h"

#include "MoreDesktopMgr.h"

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


#pragma mark -


/*	------------------------------------------------------------------
	Find_desktop_database			Find the reference number of a
									desktop database containing icons
									for a specified creator code.
	The search begins on a specified volume, but covers all volumes.
	
	returns true if found
	
	------------------------------------------------------------------ */
Boolean CIconUtils::FindDesktopDatabase(short *ioVolumeRefNum, const OSType inFileCreator)
{
	VolumeParam		vpb = {0};
	short			dtRefNum = 0;
	
	if ( InOneDesktop( ioVolumeRefNum, inFileCreator, &dtRefNum ) )
		return true;
	
	for (vpb.ioVolIndex = 1; PBGetVInfoSync((ParmBlkPtr )&vpb) == noErr; ++vpb.ioVolIndex)
	{
		if (vpb.ioVRefNum == *ioVolumeRefNum)
			continue;
			
		short	foundVolRefNum = vpb.ioVRefNum;
		
		if ( InOneDesktop( &foundVolRefNum, inFileCreator, &dtRefNum) )
		{
			*ioVolumeRefNum = foundVolRefNum;
			return true;
		}
	}

	return false;
}


/*	------------------------------------------------------------------
	InOneDesktop			Determine whether the desktop database for
							one particular volume contains icons for
							a given creator code, and if so, return its
							reference number.
	------------------------------------------------------------------
*/
Boolean	CIconUtils::InOneDesktop(short *ioVolumeRefNum, const OSType inFileCreator, short *outDtRefNum)
{
	OSErr					err;
	DTPBRec					deskRec = {0};
	HParamBlockRec        	hParams = {0};
	GetVolParmsInfoBuffer  	volInfoBuffer = {0};
		
	// check to make sure we've got a database first:
	hParams.ioParam.ioNamePtr  = (StringPtr)nil;
	hParams.ioParam.ioVRefNum  = *ioVolumeRefNum;
	hParams.ioParam.ioBuffer   = (Ptr)&volInfoBuffer;
	hParams.ioParam.ioReqCount = sizeof(volInfoBuffer);
	
	if ( ((err = PBHGetVolParmsSync(&hParams))!=noErr) || ((volInfoBuffer.vMAttrib&(1L << bHasDesktopMgr))==0) )
		return false;
	
	deskRec.ioVRefNum = hParams.ioParam.ioVRefNum;
	err = PBDTGetPath( &deskRec );
	if (err != noErr)
		return false;

	/*	We want to ignore any non-icon data, such as the 'paul'
		item that is used for drag-and-drop. */
	deskRec.ioFileCreator = inFileCreator;
	deskRec.ioIndex = 1;
	do
	{
		deskRec.ioTagInfo = 0;
		err = PBDTGetIconInfoSync( &deskRec );
		deskRec.ioIndex += 1;
	} while( (err == noErr) && (deskRec.ioIconType <= 0) );

	if (err == noErr)
	{
		*ioVolumeRefNum = deskRec.ioVRefNum;
		*outDtRefNum = deskRec.ioDTRefNum;
		return true;
	}
	
	return false;
}



void CIconUtils::GetDesktopIconSuite( const OSType inFileCreator, const OSType inFileType, const short inSize, Handle* outHandle )
{
	Assert_( inSize == kLargeIcon || inSize == kSmallIcon  );
	Assert_( *outHandle == nil);		// check for possible memory leak
	
	*outHandle = nil;
	
	Handle		suiteHand = nil;
	OSErr		err = noErr;
	
	if (inFileCreator && inFileType )
	{
		Handle 	thisIconHandle = nil;
		short	theVolRefNum = 0;
		Boolean	gotOne = false;
		
		// first, find a desktop database containing the creator's data
		if ( FindDesktopDatabase( &theVolRefNum, inFileCreator) )
		{
			err = ::NewIconSuite(&suiteHand);
			if (err != noErr) return;
			
			short	i;
			short	iconSizeType = inSize;
			
			for ( i = 0; i < 3; i++ )		// iterate through the sizes. This is a nasty hack, hardly justified by the Assert above
			{
				err = ::DTGetIcon( nil /* vol name */, theVolRefNum, iconSizeType , inFileCreator, inFileType, &thisIconHandle);
			
				if ( err == noErr )
					err = ::AddIconToSuite(thisIconHandle, suiteHand, ::DTIconToResIcon( iconSizeType  ) );
					
				iconSizeType++;
				gotOne |= (err == noErr);
				
				// only break on really bad errors
				if ( err != noErr && err != afpItemNotFound )
					break;
			}
			
			if ( !gotOne ) // Didn't find at least the B&W icon
			{
				::DisposeIconSuite(suiteHand, true);
				suiteHand = nil;
			}					
		}
	}
	
	if ( suiteHand == nil ) 
	{
		ResIDT 	iconID = (inFileType == 'APPL') ? kGenericApplicationIconResource : kGenericDocumentIconResource;
		err = ::GetIconSuite(&suiteHand, iconID, inSize == kLargeIcon ? kSelectorAllLargeData : kSelectorAllSmallData );
		if (err != noErr)
			suiteHand = nil;
	}
	
	*outHandle = suiteHand;
}

#pragma mark -


CAttachmentIcon::CAttachmentIcon()
:	mIconSuiteHandle(nil)
{
}


CAttachmentIcon::CAttachmentIcon(const OSType inFileCreator, const OSType inFileType, const Int16 inIconSize)
:	mIconSuiteHandle(nil)
{
	CIconUtils::GetDesktopIconSuite(inFileCreator, inFileType, inIconSize, &mIconSuiteHandle);
}


CAttachmentIcon::CAttachmentIcon(const ResID inIconResID, const Int16 inIconSize)
:	mIconSuiteHandle(nil)
{
	IconSelectorValue	theSelector = (inIconSize == kIconSizeLarge) ? kSelectorAllLargeData : kSelectorAllSmallData;
	
	OSErr	err = ::GetIconSuite(&mIconSuiteHandle, inIconResID, theSelector);
	if (err != noErr)
		mIconSuiteHandle = nil;
}

// attachmentRealType can be NULL
CAttachmentIcon::CAttachmentIcon(const char* attachmentRealType, const Int16 inIconSize)
:	mIconSuiteHandle(nil)
{
	ResID		iconID = kGenericDocumentIconResource;

	if (attachmentRealType != NULL)
	{
		// get an icon ID based on the real type (text/html etc)
		if (XP_STRNCMP(TEXT_HTML, attachmentRealType, XP_STRLEN(TEXT_HTML)) == 0)
		{
			iconID = 129;		//netscape html
		}
		else if (XP_STRNCMP(TEXT_PLAIN, attachmentRealType, XP_STRLEN(TEXT_PLAIN)) == 0)
		{
			iconID = 133;		//netscape text
		}
		else if (XP_STRNCMP(MESSAGE_RFC822, attachmentRealType, XP_STRLEN(MESSAGE_RFC822)) == 0)
		{
			iconID = 15474;		//message
		}
		else if (XP_STRNCMP(TEXT_VCARD, attachmentRealType, XP_STRLEN(attachmentRealType)) == 0)
		{
			iconID = 15476;		//vcard icon
		}
		else if (XP_STRNCMP("application/", attachmentRealType, 12) == 0)
		{
			CMimeMapper		*thisMapper = CPrefs::sMimeTypes.FindMimeType( (char *)attachmentRealType );
			
			if (thisMapper)
			{
				CIconUtils::GetDesktopIconSuite(thisMapper->GetAppSig(), thisMapper->GetDocType(),
						(inIconSize == kIconSizeLarge) ? kLargeIcon : kSmallIcon, &mIconSuiteHandle);
				if (mIconSuiteHandle)
					return;		//our work here is done
			}
		}
		// add more if you can find the icons
	}
	
	IconSelectorValue	theSelector = (inIconSize == kIconSizeLarge) ? kSelectorAllLargeData : kSelectorAllSmallData;
	
	OSErr	err = ::GetIconSuite(&mIconSuiteHandle, iconID, theSelector);
	if (err != noErr)
		mIconSuiteHandle = nil;
}


static pascal OSErr IconDuplicateAction(ResType theType, Handle theIconData, void *refCon)
{
	Handle	newIconSuite = (Handle)refCon;
	OSErr	err = HandToHand(&theIconData);		// copy the data
	if (err != noErr) return err;
	return ::AddIconToSuite(theIconData, newIconSuite, theType);
}

// copy constructor
CAttachmentIcon::CAttachmentIcon( const CAttachmentIcon& inOriginal)
{
	if (inOriginal.mIconSuiteHandle == nil) {
		mIconSuiteHandle = nil;
		return;
	}
	
	OSErr		err = noErr;
	Handle		newIconSuite = nil;
	
	err = ::NewIconSuite(&newIconSuite);
	if (err != noErr) return;
	
	IconActionUPP	iconActionProc = NewIconActionProc(IconDuplicateAction);
	
	// Duplicate the icon data. This is quite horrid, but the only way (AFAIK)
	err = ::ForEachIconDo(inOriginal.mIconSuiteHandle, svAllAvailableData, iconActionProc, (void *)newIconSuite);
	Assert_(err == noErr);
	::DisposeRoutineDescriptor(iconActionProc);
	
	mIconSuiteHandle = newIconSuite;
}

CAttachmentIcon::~CAttachmentIcon()
{
	if (mIconSuiteHandle)
	{
		OSErr	err = ::DisposeIconSuite(	mIconSuiteHandle, true );
		Assert_(err == noErr);
	}
}


void CAttachmentIcon::PlotIcon(const Rect& inIconFrame, IconAlignmentType inIconAlign, IconTransformType inIconTransform)
{
	if (mIconSuiteHandle)
	{
		::PlotIconSuite(&inIconFrame, inIconAlign, inIconTransform, mIconSuiteHandle);
	}
}


// the RgnHandle must have already been allocated
void CAttachmentIcon::AddIconOutlineToRegion( RgnHandle inRgnHandle, const Int16 inIconSize )
{
	Rect		iconRect;
	StRegion	tempRgn;
	
	if (! mIconSuiteHandle) return;
	
	if (inIconSize == kIconSizeSmall)
		SetRect(&iconRect, 0, 0, 16, 16);
	else
		SetRect(&iconRect, 0, 0, 32, 32);
		
	OSErr	err = ::IconSuiteToRgn( tempRgn, &iconRect, kAlignNone, mIconSuiteHandle );
	Assert_(err == noErr);
	
	::UnionRgn(tempRgn, inRgnHandle, inRgnHandle);
}




