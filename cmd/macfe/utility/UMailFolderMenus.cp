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

/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 12 DEC 96

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "UMailFolderMenus.h"
#include "CMailNewsContext.h"
#include "PascalString.h"
#include "CMessageFolderView.h"
#include "CMailFolderButtonPopup.h"

#include "StSetBroadcasting.h"
#include "miconutils.h"
#include "MercutioAPI.h"
#include "UMenuUtils.h"
#include "CMessageFolder.h"

#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

static const UInt16 cFolderPopupFlags = MSG_FOLDER_FLAG_MAIL;
static const UInt8 cSpaceFolderMenuChar = 0x09;
static const ResIDT cMenuIconIDDiff = 256;
static const Int16 cItemIdentWidth = 16;

#pragma mark -
/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

// Static class members

LArray CMailFolderMixin::sMailFolderMixinRegisterList;
Boolean CMailFolderMixin::sMustRebuild = true;
void* CMailFolderMixin::sMercutioCallback = nil;

//-----------------------------------
CMailFolderMixin::CMailFolderMixin(Boolean inIncludeFolderIcons)
:	mFolderArray(sizeof(CMessageFolder))
,	mDesiredFolderFlags((FolderChoices)(eWantPOP + eWantIMAP))
,	mInitialMenuCount(0)
//-----------------------------------
{

	mUseFolderIcons = inIncludeFolderIcons;
	RegisterMailFolderMixin(this);
} // CMailFolderMixin::CMailFolderMixin

//-----------------------------------
CMailFolderMixin::~CMailFolderMixin()
//-----------------------------------
{
	UnregisterMailFolderMixin(this);
	// Delete all the CMessageFolder objects, or else we'll leak the cache!
	for (ArrayIndexT i = 1; i <= mFolderArray.GetCount(); i++)
	{
		CMessageFolder* f = (CMessageFolder*)mFolderArray.GetItemPtr(i);
		if (f)
			(*(CMessageFolder*)mFolderArray.GetItemPtr(i)).CMessageFolder::~CMessageFolder();
	}
} // CMailFolderMixin::~CMailFolderMixin

//-----------------------------------
void CMailFolderMixin::UpdateMailFolderMixins()
//-----------------------------------
{
	sMustRebuild = true;
	LCommander::SetUpdateCommandStatus(true);
} // CMailFolderMixin::UpdateMailFolderMixins

//-----------------------------------
void CMailFolderMixin::UpdateMailFolderMixinsNow(CMailFolderMixin *inSingleMailFolderMixin)
//	Notify all registered CMailFolderMixin objects that the mail folder list has changed.
//	This method should be called whenever the list changes, fo example, when the user
//	adds, moves, or deletes mail folders in the Folder View window.
//	
//	If inSingleMailFolderMixin is not nil, only that specific CMailFolderMixin is 
//	notified of the change, NOT all CMailFolderMixin objects.
//	
//	IMPORTANT NOTE: CMailFolderMixin::UpdateMailFolderMixinsNow(this) needs to be called
//	for all subclasses after initialization is complete.
//-----------------------------------
{

	if (!sMustRebuild && inSingleMailFolderMixin == nil)
		return;
	if (sMailFolderMixinRegisterList.GetCount() == 0 && inSingleMailFolderMixin == nil)
		return;	// No reason to do anything!
	
	MSG_Master *master = CMailNewsContext::GetMailMaster();
	FailNIL_(master);
	
	// Get a list and count of all current mail folders. You can only ask for one flag
	// at a time (ALL bits must match for positive test in this call).	
	Int32 numFoldersMail = ::MSG_GetFoldersWithFlag(master, MSG_FOLDER_FLAG_MAIL, nil, 0);
	Assert_(numFoldersMail > 0);	// Should have at least some permanent mail folders!
	Int32 numNewsgroups = 0;
	Int32 numNewsHosts = 0;
	if (
		!inSingleMailFolderMixin
	|| (inSingleMailFolderMixin->mDesiredFolderFlags & (eWantNews|eWantHosts))
			== (eWantNews|eWantHosts)
		)
		numNewsHosts = ::MSG_GetFoldersWithFlag(master, MSG_FOLDER_FLAG_NEWS_HOST, nil, 0);
	if (!inSingleMailFolderMixin
	|| (inSingleMailFolderMixin->mDesiredFolderFlags & eWantNews) != 0)
		numNewsgroups = ::MSG_GetFoldersWithFlag(master, MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_SUBSCRIBED, nil, 0);

	Int32 numFolders = numFoldersMail + numNewsgroups + numNewsHosts;	
	StPointerBlock folderInfoData(sizeof(MSG_FolderInfo *) * numFolders);
	MSG_FolderInfo **folderInfo = (MSG_FolderInfo **) folderInfoData.mPtr;
	
	Int32 numFolders2 = ::MSG_GetFoldersWithFlag(master, MSG_FOLDER_FLAG_MAIL, folderInfo, numFoldersMail);
	Assert_(numFolders2 > 0);	// Should have at least some permanent mail folders!
	Assert_(numFolders2 == numFoldersMail);
	
	// Handle the news hosts and groups, filling in the remaining pointers in the array.
	MSG_FolderInfo** currentInfo = folderInfo + numFoldersMail;
	if (numNewsHosts && numNewsgroups)
	{
		// The difficult case.
		// Get the list of hosts and the list of groups into separate lists.
		StPointerBlock newsHostInfoData(sizeof(MSG_FolderInfo *) * numNewsHosts);
		MSG_FolderInfo **newsHostInfo = (MSG_FolderInfo **) newsHostInfoData.mPtr;
		numFolders2 = ::MSG_GetFoldersWithFlag(
			master,
			MSG_FOLDER_FLAG_NEWS_HOST,
			newsHostInfo,
			numNewsHosts);
		Assert_(numFolders2 == numNewsHosts);
		
		StPointerBlock newsgroupInfoData(sizeof(MSG_FolderInfo *) * numNewsgroups);
		MSG_FolderInfo **newsgroupInfo = (MSG_FolderInfo **) newsgroupInfoData.mPtr;
		numFolders2 = ::MSG_GetFoldersWithFlag(
			master,
			MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_SUBSCRIBED,
			newsgroupInfo,
			numNewsgroups);
		Assert_(numFolders2 == numNewsgroups);
		
		// Loop over the hosts, copying one host, then all its groups
		if (numNewsHosts)
		{
			for (int i = 0; i < numNewsHosts; i++)
			{
				// Copy over all the news groups for this host
				*currentInfo++ = newsHostInfo[i];
				for (int j = 0; j < numNewsgroups; j++)
				{
					// Are the two hosts the same?
					if (::MSG_GetHostForFolder(newsgroupInfo[j])
						== ::MSG_GetHostForFolder(newsHostInfo[i]))
					{
						*currentInfo++ = newsgroupInfo[j];
					}		
				} // inner loop
			} // outer loop
		}
	}
	else if (numNewsgroups)
	{
		numFolders2 = ::MSG_GetFoldersWithFlag(
			master,
			MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_SUBSCRIBED,
			currentInfo,
			numNewsgroups);
	}
	else if (numNewsHosts)
	{
		numFolders2 = ::MSG_GetFoldersWithFlag(
			master,
			MSG_FOLDER_FLAG_NEWS_HOST,
			currentInfo,
			numNewsHosts);
	}
	// Notify all registered CMailFolderMixin objects, or only the specified object
	if ( inSingleMailFolderMixin == nil )
	{
		LArrayIterator iterator(sMailFolderMixinRegisterList);
		while ( iterator.Next(&inSingleMailFolderMixin) )
			inSingleMailFolderMixin->MPopulateWithFolders(folderInfo, numFolders);
		sMustRebuild = false;
	}
	else
		inSingleMailFolderMixin->MPopulateWithFolders(folderInfo, numFolders);
} // CMailFolderMixin::UpdateMailFolderMixinsNow

//-----------------------------------
Boolean CMailFolderMixin::MSetSelectedFolderName(
	const char *inName,
	Boolean inDoBroadcast)
//	Set the currently selected item in the menu to the item represented by inName. If
//	inName is nil, empty, or cannot be found, the value of the menu is set to 0.
//	Return true if the specified folder name was found in the menu.
//-----------------------------------
{
	const ArrayIndexT numItems = mFolderArray.GetCount();
	ArrayIndexT curIndex;
	if ( !inName || !*inName)
		curIndex = numItems + 1;
	else
	{
		for (curIndex = 1; curIndex <= numItems; ++curIndex)
		{
			const char* itemPath = MGetFolderName(curIndex);
			if ( ::strcasecomp(inName, itemPath) == 0 )
				break;
		}
	}

	if ( curIndex <= numItems )
	{
		MSetSelectedMenuItem(curIndex, inDoBroadcast);
		return true;
	}
	else
	{
		MSetSelectedMenuItem(0, inDoBroadcast);
		return false;
	}
}

//-----------------------------------
Boolean CMailFolderMixin::MSetSelectedFolder(
	const CMessageFolder& inFolder,
	Boolean /*inDoBroadcast*/)
//-----------------------------------
{
	return MSetSelectedFolder(inFolder.GetFolderInfo());
}

//-----------------------------------
Boolean CMailFolderMixin::MSetSelectedFolder(
	const MSG_FolderInfo* inInfo,
	Boolean inDoBroadcast)
//-----------------------------------
{
	const ArrayIndexT numItems = mFolderArray.GetCount();
	Assert_(mFolderArray.GetCount() == (MGetNumMenuItems() - mInitialMenuCount));
		// Not synced otherwise	
	ArrayIndexT curIndex;
	if (inInfo == nil)
		curIndex = numItems + 1;
	else
	{
		mFolderArray.Lock();
		for (curIndex = 1; curIndex <= numItems; ++curIndex)
		{
			CMessageFolder* curFolder = (CMessageFolder*)mFolderArray.GetItemPtr(curIndex);
			if (inInfo == curFolder->GetFolderInfo())
				break;
		}
		mFolderArray.Unlock();
	}

	if ( curIndex <= numItems )
	{
		MSetSelectedMenuItem(curIndex, inDoBroadcast);
		return true;
	}
	else
	{
		MSetSelectedMenuItem(0, inDoBroadcast);
		return false;
	}
}

//-----------------------------------
const char* CMailFolderMixin::MGetFolderName(ArrayIndexT inItemNumber)
//	Get the currently selected folder name in the menu. Return a reference to head of 
//	the returned c-string.
//-----------------------------------
{
	CMessageFolder folder = MGetFolder(inItemNumber);
	MSG_FolderInfo* info = folder.GetFolderInfo();
	if (info)
	{
		const char * str = ::MSG_GetFolderNameFromID(info);

		// Make sure that the name is fully escaped
		static char escapedName[256];
		XP_STRCPY(escapedName, str);
		NET_UnEscape(escapedName);
		char * temp = NET_Escape(escapedName, URL_PATH);
		if (temp)
		{
			XP_STRCPY(escapedName, temp);
			XP_FREE(temp);
		}
		return escapedName;
	}
	return nil;
}


//-----------------------------------
const char* CMailFolderMixin::MGetSelectedFolderName()
//	Get the currently selected folder name in the menu. Return a reference to head of 
//	the returned c-string.
//-----------------------------------
{
	ArrayIndexT index = MGetSelectedMenuItem();
	if (index)
		return MGetFolderName(index);
	return nil;
}

//-----------------------------------
CMessageFolder CMailFolderMixin::MGetFolder(ArrayIndexT inItemNumber)
//-----------------------------------
{
	CMessageFolder result(nil);
	Assert_(mFolderArray.GetCount() == (MGetNumMenuItems() - mInitialMenuCount));	// Not synced otherwise
	if ( (mFolderArray.GetCount() > 0) && (inItemNumber > 0) )
	{
		if ( mFolderArray.ValidIndex(inItemNumber) )
		{
			mFolderArray.Lock();
			result = *(CMessageFolder*) mFolderArray.GetItemPtr(inItemNumber);
			mFolderArray.Unlock();
		}
	}
	return result;
}

//-----------------------------------
CMessageFolder CMailFolderMixin::MGetFolderFromMenuItem(ArrayIndexT inItemNumber)
//-----------------------------------
{
	// convert the menu item number in to an index into the folder array
	CMessageFolder result(nil);
	if (inItemNumber - mInitialMenuCount > 0)
	{	// If (inItemNumber - mInitialMenuCount) is 0 or less then we are trying to
		// find folder info on a previously existing menu item.
		result = MGetFolder(inItemNumber - mInitialMenuCount);
	}
	return result;
}


//-----------------------------------
CMessageFolder CMailFolderMixin::MGetSelectedFolder()
//-----------------------------------
{
	return MGetFolder(MGetSelectedMenuItem());
}

//-----------------------------------
ResIDT CMailFolderMixin::GetMailFolderIconID(UInt16 inFlags)
//-----------------------------------
{

	if ((inFlags & MSG_FOLDER_FLAG_MAIL) != 0)
		return 0;
	UInt16 folderType = inFlags & (
		MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_SENTMAIL | 
		MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE |
		MSG_FOLDER_FLAG_INBOX
		);
	switch ( folderType )
	{	
		case MSG_FOLDER_FLAG_TRASH:		return 286;
		case MSG_FOLDER_FLAG_SENTMAIL:	return 284;
		case MSG_FOLDER_FLAG_DRAFTS:	return 285;
		case MSG_FOLDER_FLAG_QUEUE:		return 283;
		case MSG_FOLDER_FLAG_INBOX:		return 282;
		default:
			if ( inFlags & MSG_FOLDER_FLAG_IMAPBOX )
				return 287;	// Online folder?
			return 281;
	}	
}

/*======================================================================================
	Register the specified inMailFolderMixin so that it is automatically notified
	when changes are made to the mail folder list. The specified inMailFolderMixin
	is only added to the list if it is not already there.
======================================================================================*/

void CMailFolderMixin::RegisterMailFolderMixin(CMailFolderMixin *inMailFolderMixin) {

	Assert_(inMailFolderMixin != nil);

	if ( sMailFolderMixinRegisterList.FetchIndexOf(&inMailFolderMixin) == LArray::index_Bad ) {
		sMailFolderMixinRegisterList.InsertItemsAt(1, LArray::index_Last, &inMailFolderMixin);
	}
}



/*======================================================================================
	Unregister the specified inMailFolderMixin so that it is no longer notified
	when changes are made to the mail folder list. If the specified inMailFolderMixin
	is not in the list, this method does nothing.
======================================================================================*/

void CMailFolderMixin::UnregisterMailFolderMixin(CMailFolderMixin *inMailFolderMixin) {

	sMailFolderMixinRegisterList.Remove(&inMailFolderMixin);
}

//-----------------------------------
void CMailFolderMixin::MGetCurrentMenuItemName(Str255 outItemName)
//	Get the name of the menu item that is currently selected, minus any indentation.
//-----------------------------------
{
	outItemName[0] = 0;
	MGetMenuItemName(MGetSelectedMenuItem(), outItemName);
	if ( outItemName[0] != 0 )
	{
		UInt8 *curChar = &outItemName[1], *endChar = curChar + outItemName[0];
		do
		{
			if ( *curChar != cSpaceFolderMenuChar ) break;
		} while ( ++curChar < endChar );

		Int8 numLeading = curChar - (&outItemName[1]);
		
		if ( numLeading != 0 )
		{
			outItemName[0] -= numLeading;
			if ( outItemName[0] != 0 )
			{
				::BlockMoveData(&outItemName[numLeading + 1], &outItemName[1], outItemName[0]);
			}
		}
	}
} // CMailFolderMixin::MGetCurrentMenuItemName

//-----------------------------------
Boolean CMailFolderMixin::FolderFilter(const CMessageFolder& folder)
//	Return true iff you want this folder included in the list. This function should
//	handle all known cases, but we may want to make this virtual and override it in the
//	future.
//-----------------------------------
{
	Boolean	result;

	if (folder.GetLevel() > 1)
	{
		UInt32	folderFlags = folder.GetFolderFlags();
		Boolean	isPOP, isIMAP, isNews;

		isNews = folderFlags & MSG_FOLDER_FLAG_NEWSGROUP ? true: false;
		isIMAP = folderFlags & MSG_FOLDER_FLAG_IMAPBOX ? true: false;
		isPOP = (!isIMAP && (folderFlags & MSG_FOLDER_FLAG_MAIL)) ? true: false;

		if (isPOP && (mDesiredFolderFlags & eWantPOP))
		{
			result = true;
		}
		else if (isIMAP && (mDesiredFolderFlags & eWantIMAP))
		{
			result = true;
		}
		else if (isNews && (mDesiredFolderFlags & eWantNews))
		{
			result = true;
		}
		else
		{
			result = false;
		}
	}
	else
	{
		// level <= 1, we are filtering a host/ server.
		result = (mDesiredFolderFlags & eWantHosts) != 0;
	}

	return result;
} // CMailFolderMixin::FolderFilter

//-----------------------------------
pascal void MyMercutioCallback(
	Int16					menuID,
	Int16					/*previousModifiers*/,
	RichItemDataYadaYada&	inItemData)
//-----------------------------------
{
	// This function is called continuously when the user clicks the 
	// folder popup in a Thread window. Static variables have been added
	// for performance reasons.

	RichItemData& itemData = (RichItemData&)inItemData;
	itemData.flags &= ~kChangedByCallback; // clear bit

	// Find the mixin object, if any, belonging to this menu
	CMailFolderMixin* mixin = nil;
	Boolean found = false;

	static Int16 prevMenuID = 0;
	static MenuRef prevMenuH = nil;
	static UInt32 prevTicks = 0;
	MenuRef menuh;

	Boolean sameMenuAsBefore = (menuID == prevMenuID
								&& prevMenuH != nil
								&& (::TickCount() - prevTicks < 30));
	// Don't reuse the same menu after a short delay because we can't tell
	// whether the user is still clicking the same popup as before: he may
	// have selected another window since the last time (and unfortunately,
	// all the folder popups in the different windows have the same ID).
	prevTicks = ::TickCount();

	if (sameMenuAsBefore)
		menuh = prevMenuH;
	else
	{
		menuh = ::GetMenuHandle(menuID);
		prevMenuH = menuh;
		prevMenuID = menuID;
	}

	if (menuh)
	{
		LArrayIterator iterator(CMailFolderMixin::sMailFolderMixinRegisterList);
		while (iterator.Next(&mixin))
		{
			if (mixin->MGetSystemMenuHandle() == menuh)
			{
				found = true;
				break;
			}
		}
	}
	if (!found || !mixin || !mixin->mUseFolderIcons)
		return;

	static ArrayIndexT prevItemID = 0;
	static CMessageFolder prevFolder(nil);
	CMessageFolder currentFolder(nil);
	if (sameMenuAsBefore && itemData.itemID == prevItemID && prevFolder.GetFolderInfo() != nil)
		currentFolder = prevFolder;
	else
	{
		currentFolder = mixin->MGetFolderFromMenuItem(itemData.itemID);
		prevFolder = currentFolder;
		prevItemID = itemData.itemID;
	}
	if (currentFolder.GetFolderInfo() == NULL)
		return;

	switch (itemData.cbMsg)
	{
		case cbBasicDataOnlyMsg:
			itemData.flags |=
				(ksameAlternateAsLastTime|kIconIsSmall|kHasIcon/*|kChangedByCallback*/|kdontDisposeIcon);
			itemData.flags &= ~(kIsDynamic);

			return;

		case cbIconOnlyMsg:
			break; // look out below!

		default:
			return;
	}

	// OK, calculate the icon and check whether it's changed.

	static ResIDT prevIconID = 0;
	static Handle prevIconSuite = nil;
	ResIDT iconID = currentFolder.GetIconID();
	Handle newIconSuite;
	if (iconID == prevIconID && prevIconSuite != nil)
		newIconSuite = prevIconSuite;
	else
	{
		newIconSuite = CIconList::GetIconSuite(iconID);
		prevIconSuite = newIconSuite;
		prevIconID = iconID;
	}
	if (newIconSuite && newIconSuite != itemData.hIcon)
	{
		// It's changed.  Modify the data record for Mercutio.
		itemData.hIcon = newIconSuite;
		itemData.iconType = 'suit';
		if (::LMGetSysFontFam() != systemFont) // no bold for Chicago - it sucketh
		{
			if (currentFolder.HasNewMessages())
				itemData.textStyle.s |= bold;
			else
				itemData.textStyle.s &= ~bold;
		}
//		itemData.flags |= kChangedByCallback; // set bit

		// indent the item by shifting the left side
		const Int16 baseLevel
			= (mixin->mDesiredFolderFlags & CMailFolderMixin::eWantHosts)
				 ? kRootLevel : kRootLevel + 1;
		itemData.itemRect.left
			= itemData.itemRect.right
			- (*menuh)->menuWidth
			+ ((currentFolder.GetLevel() - baseLevel) * cItemIdentWidth);
	}
} // MyMercutioCallback

//-----------------------------------
void CMailFolderMixin::GetFolderNameForDisplay(char * inName, CMessageFolder& folder)
//-----------------------------------
{
	const char* geekName = (char*)folder.GetName();
	const char* prettyName = (char*)folder.GetPrettyName();
	Boolean isNewsFolder = ((folder.GetFolderType() & (MSG_FOLDER_FLAG_NEWSGROUP | MSG_FOLDER_FLAG_NEWS_HOST)) != 0);

	if (isNewsFolder && prettyName != nil && *prettyName != '\0')
	{
#if 0
//		sprintf(inName, "%s (%s)", geekName, prettyName);
		Int16 strLen = ::strlen(geekName);
		::strcpy(inName, geekName);
		inName[strLen ++] = ' ';
		inName[strLen ++] = '(';
		::strcpy(&inName[strLen], prettyName);
		strLen += ::strlen(prettyName);
		inName[strLen ++] = ')';
		inName[strLen ++] = '\0';
#endif
		::strcpy(inName, prettyName);
	}
	else
		::strcpy(inName, geekName);
} // GetFolderNameForDisplay

inline void *operator new(size_t,void *p) { return p; }

//-----------------------------------
Boolean CMailFolderMixin::MPopulateWithFolders(const MSG_FolderInfo **inFolderInfoArray,
											   Int32 inNumFolders)
//	Populate the folder menu popup with the specified list of mail folders. This method
//	can be called at anytime after object creation; it does the right thing.
//	
//	The inFolderInfoArray contains a list of (MSG_FolderInfo *) pointer representing
//	the current list of mail folders. inNumFolders is the number of items in the
//	array, 0 if none.
//	
//	This method tries to keep the currently selected folder, but if changed, sets the 
//	menu value to 0.
//-----------------------------------
{
	MSG_Master *master = CMailNewsContext::GetMailMaster();
	FailNIL_(master);
	
	MenuRef menuh = MGetSystemMenuHandle();

	const Int16 startNumItems = MGetNumMenuItems() - mInitialMenuCount;
	Assert_(mFolderArray.GetCount() == startNumItems);	// Should be equal

	Boolean didChange = false;
	
	Int16 addedItems = 0;
	
	// Store the currently selected folder
	
	CMessageFolder selectedFolder = MGetSelectedFolder();
	
	// Set menu items
	MSG_FolderInfo** curInfo = (MSG_FolderInfo**)inFolderInfoArray;
	const Int16 baseLevel = (mDesiredFolderFlags & eWantHosts) ? kRootLevel : kRootLevel + 1;
	const Int16 spacesPerFolderLevel = (cItemIdentWidth / ::CharWidth(cSpaceFolderMenuChar)) + 1;
	CMessageFolder folder(nil);
	for (Int16 curItem = 0; curItem < inNumFolders; curItem++, curInfo++)
	{
		CMessageFolder previousFolder = folder;
		folder = CMessageFolder(*curInfo);
		if (FolderFilter(folder))
		{

			// Insert a gray line in the menu for host-level stuff we're not inserting.		
			if (addedItems && previousFolder.GetLevel() == 1 && !(mDesiredFolderFlags & eWantHosts))
			{
				if ( ++addedItems <= startNumItems )
				{
					if ( MUpdateMenuItem(addedItems + mInitialMenuCount, "\p-", /*iconID*/0) )
						didChange = true;
					new (mFolderArray.GetItemPtr(addedItems)) CMessageFolder(nil);
				}
				else
				{
					didChange = true;
					MAppendMenuItem("\p-", /*iconID*/0);
					mFolderArray.InsertItemsAt(1, addedItems, &folder);
					// Now we have to store it again, this time properly, using the copy
					// constructor.  Otherwise, the refcount of CCachedFolderLine is not incremented
					// because the previous line just does a bitwise copy.
					new (mFolderArray.GetItemPtr(addedItems)) CMessageFolder(nil);
				}
				// Ask for a callback so we can set the icon
				if (mUseFolderIcons && MDEF_IsCustomMenu(menuh))
					::SetItemStyle(menuh, addedItems + mInitialMenuCount, condense);
			}

			// add spaces at the end of the name to make the menu larger
			// and reserve some room for the identation	
			char tempString[cMaxMailFolderNameLength + 1];
			GetFolderNameForDisplay(tempString, folder);

			Int16 nameLen = ::strlen(tempString);
			Int16 numSpaces = ((folder.GetLevel() - baseLevel) * spacesPerFolderLevel);
			if ( numSpaces > (sizeof(tempString) - nameLen) )
				numSpaces = sizeof(tempString) - nameLen;
			if ( numSpaces > 0 )
				::memset(tempString + nameLen, cSpaceFolderMenuChar, numSpaces);
			tempString[nameLen + numSpaces] = '\0';
			NET_UnEscape(tempString);

			CStr255 menuString(tempString);
			
			// Update or append the current menu item			
			if ( ++addedItems <= startNumItems )
			{
				if ( MUpdateMenuItem(addedItems + mInitialMenuCount, menuString, /*iconID*/0) )
					didChange = true;
				(*(CMessageFolder*)mFolderArray.GetItemPtr(addedItems)).CMessageFolder::~CMessageFolder();
				new (mFolderArray.GetItemPtr(addedItems)) CMessageFolder(folder);
			}
			else
			{
				didChange = true;
				MAppendMenuItem(menuString, /*iconID*/0);
				mFolderArray.InsertItemsAt(1, addedItems, &folder);
				// Now we have to store it again, this time properly, using the copy
				// constructor.  Otherwise, the refcount of CCachedFolderLine is not incremented
				// because the previous line just does a bitwise copy.
				new (mFolderArray.GetItemPtr(addedItems)) CMessageFolder(folder);
			}
			// Ask for a callback so we can set the icon
			if (mUseFolderIcons && MDEF_IsCustomMenu(menuh))
				::SetItemStyle(menuh, addedItems + mInitialMenuCount, condense);
		} // if (FolderFilter(folder))
	} // for
	
	// Get rid of any remaining menu items
	if (addedItems < startNumItems)
	{
		didChange = true;
		MClipMenuItems(addedItems + 1 + mInitialMenuCount);
		for (ArrayIndexT i = addedItems + 1; i <= mFolderArray.GetCount(); i++)
		{
			CMessageFolder* f = (CMessageFolder*)mFolderArray.GetItemPtr(i);
			if (f)
				(*(CMessageFolder*)mFolderArray.GetItemPtr(i)).CMessageFolder::~CMessageFolder();
		}
		mFolderArray.RemoveItemsAt(LArray::index_Last, addedItems + 1);
	}
	
	// Set the originally selected item (try to keep original folder), 
	// or 0 if the folder was deleted
	if (didChange && inNumFolders > 0 )
	{
		// Don't broadcast anything here
		if ( !MSetSelectedFolder(selectedFolder, false) )
			MSetSelectedMenuItem(0, false);
	}

	// Set up for callback so we can set the icon
	if (mUseFolderIcons && MDEF_IsCustomMenu(menuh))
	{
		if (!sMercutioCallback)
			sMercutioCallback = NewMercutioCallback(MyMercutioCallback);
		FailNIL_(sMercutioCallback);
		MDEF_SetCallbackProc(menuh, (MercutioCallbackUPP)sMercutioCallback);
		MenuPrefsRec myPrefs;
		::memset(&myPrefs, 0, sizeof(myPrefs));
		myPrefs.useCallbackFlag.s = condense;
		MDEF_SetMenuPrefs(menuh, &myPrefs);
	}

	// Refresh on change
	if (didChange)
		MRefreshMenu();
	return didChange;
} // MPopulateWithFolders


#pragma mark -

/*======================================================================================
	Get the number of menu items.
======================================================================================*/

Int16 CMailFolderPopupMixin::MGetNumMenuItems(void)
{
	return MGetControl()->GetMaxValue();
}


/*======================================================================================
	Get the selected menu item.
======================================================================================*/

Int16 CMailFolderPopupMixin::MGetSelectedMenuItem(void)
{
	return MGetControl()->GetValue();
}


/*======================================================================================
	Set the selected menu item. Refresh the menu if the selection changes.
======================================================================================*/

void CMailFolderPopupMixin::MSetSelectedMenuItem(Int16 inIndex, Boolean inDoBroadcast)
{

	StSetBroadcasting setBroadcasting(
		MGetControl(),
		inDoBroadcast && MGetControl()->IsBroadcasting()
		);
	MGetControl()->SetValue(inIndex);
}


/*======================================================================================
	Update the current menu item. If any of the input parameters are different from
	the current menu item parameters, return true and reset the menu item; otherwise,
	return false and don't modify the menu. If inIconID is non-zero, set the icon for
	the menu item to that resource. Don't refresh the menu display in anyway.
======================================================================================*/

Boolean CMailFolderPopupMixin::MUpdateMenuItem(Int16 inMenuItem, ConstStr255Param inName, 
									   		   ResIDT inIconID)
{
	LControl *theControl = MGetControl();
//	StEmptyVisRgn emptyRgn(theControl->GetMacPort());	// Make changes invisible

	Boolean changed = false;
	MenuHandle menuH = MGetSystemMenuHandle();
	Assert_(menuH != nil);
	Assert_((inIconID > cMenuIconIDDiff) || (inIconID == 0));
	
	Assert_(::CountMItems(menuH) == MGetNumMenuItems());
	Assert_((inMenuItem > 0) && (inMenuItem <= MGetNumMenuItems()));
	
	Str255 currentText;
	::GetMenuItemText(menuH, inMenuItem, currentText);
	
	changed = !::EqualString(currentText, inName, true, true);
	if ( changed ) ::SetMenuItemText(menuH, inMenuItem, inName);
	
	if ( inIconID ) inIconID -= cMenuIconIDDiff;
	Int16 iconID;
	::GetItemIcon(menuH, inMenuItem, &iconID);
	if ( iconID != inIconID ) {
		changed = true;
		::SetItemIcon(menuH, inMenuItem, inIconID);
	}
	
	return changed;
}


/*======================================================================================
	Append the specified menu item to the end of the menu. If inIconID is non-zero, set 
	the icon for the menu item to that resource. Don't refresh the menu display in 
	anyway.
======================================================================================*/

void CMailFolderPopupMixin::MAppendMenuItem(ConstStr255Param inName, ResIDT inIconID) {

	LControl *theControl = MGetControl();
//	StEmptyVisRgn emptyRgn(theControl->GetMacPort());	// Make changes invisible

	MenuHandle menuH = MGetSystemMenuHandle();
	Assert_(menuH != nil);
	Assert_((inIconID > cMenuIconIDDiff) || (inIconID == 0));
	Int16 numMenuItems = MGetNumMenuItems() + 1;
	
	Assert_(::CountMItems(menuH) == MGetNumMenuItems());
	
	UMenuUtils::AppendMenuItem(menuH, inName);
	
	if ( inIconID ) {
		::SetItemIcon(menuH, numMenuItems, inIconID - cMenuIconIDDiff);
	}
	theControl->SetMaxValue(numMenuItems);
}


/*======================================================================================
	Remove menu items from the end of the current menu, beginning with the item at
	inStartClipItem. Don't refresh the menu display in anyway.
======================================================================================*/

void CMailFolderPopupMixin::MClipMenuItems(Int16 inStartClipItem) {

	LControl *theControl = MGetControl();
//	StEmptyVisRgn emptyRgn(theControl->GetMacPort());	// Make changes invisible

	MenuHandle menuH = MGetSystemMenuHandle();
	Assert_(menuH != nil);
	Int16 numMenuItems = MGetNumMenuItems();
	
	Assert_(::CountMItems(menuH) == numMenuItems);
	Assert_((inStartClipItem > 0) && (inStartClipItem <= numMenuItems));
	
	while ( numMenuItems >= inStartClipItem ) {
		::DeleteMenuItem(menuH, numMenuItems--);
	}
	inStartClipItem -= 1;

	if ( MGetSelectedMenuItem() > inStartClipItem ) theControl->SetValue(inStartClipItem);
	theControl->SetMaxValue(inStartClipItem);
}


/*======================================================================================
	Get the current menu text for the specified item.
======================================================================================*/

void CMailFolderPopupMixin::MGetMenuItemName(Int16 inMenuItem, Str255 outItemName) {

	MenuHandle menuH = MGetSystemMenuHandle();
	Assert_(menuH != nil);
	Assert_(::CountMItems(menuH) == MGetNumMenuItems());
	Assert_((inMenuItem > 0) && (inMenuItem <= MGetNumMenuItems()));

	::GetMenuItemText(menuH, inMenuItem, outItemName);
}


/*======================================================================================
	Refresh the menu display.
======================================================================================*/

void CMailFolderPopupMixin::MRefreshMenu(void) {

	MGetControl()->Refresh();
}


#pragma mark -

/*======================================================================================
	Get a handle to the Mac menu associated with this object.
======================================================================================*/

MenuHandle CGAIconPopupFolderMixin::MGetSystemMenuHandle(void) {

	return MGetControl()->GetMacMenuH();
}


/*======================================================================================
	Refresh the menu display.
======================================================================================*/

void CGAIconPopupFolderMixin::MRefreshMenu(void) {

	MGetControl()->RefreshMenu();
}


#pragma mark -

/*======================================================================================
	Get a handle to the Mac menu associated with this object.
======================================================================================*/

MenuHandle CGAPopupFolderMixin::MGetSystemMenuHandle(void) {

	return MGetControl()->GetMacMenuH();
}


#pragma mark -

/*======================================================================================
	Get a handle to the Mac menu associated with this object.
======================================================================================*/

MenuHandle CGAIconButtonPopupFolderMixin::MGetSystemMenuHandle(void) {

	return MGetControl()->GetMacMenuH();
}


/*======================================================================================
	Refresh the menu display.
======================================================================================*/

void CGAIconButtonPopupFolderMixin::MRefreshMenu(void) {

	// Don't need to do anything since the menu is not visible unless the user has clicked
	// on it. For a displayed name alongside the button, override in descended class.
}


#pragma mark -

/*======================================================================================
	Get the number of menu items.
======================================================================================*/

Int16 CMenuMailFolderMixin::MGetNumMenuItems(void) {

	MenuHandle menuH = MGetSystemMenuHandle();
	return ::CountMItems(menuH);
}


/*======================================================================================
	Get the selected menu item.
======================================================================================*/

Int16 CMenuMailFolderMixin::MGetSelectedMenuItem(void) {

	MenuHandle menuH = MGetSystemMenuHandle();
	Int16 numMenuItems = MGetNumMenuItems();

	for (Int16 i = 1; i <= numMenuItems; ++i) {
		short mark;
		::GetItemMark(menuH, i, &mark);
		if ( mark ) return i;
	}
	
	return 0;	// No item selected
}


/*======================================================================================
	Set the selected menu item. Refresh the menu if the selection changes.
======================================================================================*/

void CMenuMailFolderMixin::MSetSelectedMenuItem(Int16 inIndex, Boolean)
{

	Int16 selectedItem = MGetSelectedMenuItem();
	if ( selectedItem != inIndex ) {
		MenuHandle menuH = MGetSystemMenuHandle();
		if ( selectedItem > 0 ) ::SetItemMark(menuH, selectedItem, noMark);
		if ( inIndex > 0 ) ::SetItemMark(menuH, inIndex, checkMark);
	}
}


/*======================================================================================
	Update the current menu item. If any of the input parameters are different from
	the current menu item parameters, return true and reset the menu item; otherwise,
	return false and don't modify the menu. If inIconID is non-zero, set the icon for
	the menu item to that resource. Don't refresh the menu display in anyway.
======================================================================================*/

Boolean CMenuMailFolderMixin::MUpdateMenuItem(Int16 inMenuItem, ConstStr255Param inName, 
									   	  	  ResIDT inIconID) {

	Boolean changed = false;
	MenuHandle menuH = MGetSystemMenuHandle();
	Assert_(menuH != nil);
	Assert_((inIconID > cMenuIconIDDiff) || (inIconID == 0));
	
	Assert_(::CountMItems(menuH) == MGetNumMenuItems());
	Assert_((inMenuItem > 0) && (inMenuItem <= MGetNumMenuItems()));
	
	Str255 currentText;
	::GetMenuItemText(menuH, inMenuItem, currentText);
	
	changed = !::EqualString(currentText, inName, true, true);
	if ( changed ) ::SetMenuItemText(menuH, inMenuItem, inName);
	
	if ( inIconID ) inIconID -= cMenuIconIDDiff;
	Int16 iconID;
	::GetItemIcon(menuH, inMenuItem, &iconID);
	if ( iconID != inIconID ) {
		changed = true;
		::SetItemIcon(menuH, inMenuItem, inIconID);
	}
	
	return changed;
}


/*======================================================================================
	Append the specified menu item to the end of the menu. If inIconID is non-zero, set 
	the icon for the menu item to that resource. Don't refresh the menu display in 
	anyway.
======================================================================================*/

void CMenuMailFolderMixin::MAppendMenuItem(ConstStr255Param inName, ResIDT inIconID) {

	LMenu *theMenu = MGetMenu();

	MenuHandle menuH = MGetSystemMenuHandle();
	Assert_(menuH != nil);
	Assert_((inIconID > cMenuIconIDDiff) || (inIconID == 0));
	Int16 numMenuItems = MGetNumMenuItems();
	
	Assert_(::CountMItems(menuH) == MGetNumMenuItems());
	
	theMenu->InsertCommand(inName, mMenuCommand, numMenuItems);
	
	if ( inIconID ) {
		::SetItemIcon(menuH, numMenuItems + 1, inIconID - cMenuIconIDDiff);
	}
}


/*======================================================================================
	Remove menu items from the end of the current menu, beginning with the item at
	inStartClipItem. Don't refresh the menu display in anyway.
======================================================================================*/

void CMenuMailFolderMixin::MClipMenuItems(Int16 inStartClipItem) {

	LMenu *theMenu = MGetMenu();

	Int16 numMenuItems = MGetNumMenuItems();
	
	Assert_((inStartClipItem > 0) && (inStartClipItem <= numMenuItems));
	
	while ( numMenuItems >= inStartClipItem ) {
		theMenu->RemoveItem(numMenuItems--);
	}
}


/*======================================================================================
	Get the current menu text for the specified item.
======================================================================================*/

void CMenuMailFolderMixin::MGetMenuItemName(Int16 inMenuItem, Str255 outItemName) {

	MenuHandle menuH = MGetSystemMenuHandle();
	Assert_(menuH != nil);
	Assert_(::CountMItems(menuH) == MGetNumMenuItems());
	Assert_((inMenuItem > 0) && (inMenuItem <= MGetNumMenuItems()));

	::GetMenuItemText(menuH, inMenuItem, outItemName);
}


/*======================================================================================
	Refresh the menu display.
======================================================================================*/

void CMenuMailFolderMixin::MRefreshMenu(void) {

	// Don't need to do anything since the menu is not visible unless the user has clicked
	// on it.
}


/*======================================================================================
	Get a handle to the Mac menu associated with this object.
======================================================================================*/

MenuHandle CMenuMailFolderMixin::MGetSystemMenuHandle(void) {

	return MGetMenu()->GetMacMenuH();
}


#pragma mark -

static const Char16	gsPopup_SmallMark			=	'¥';	// Mark used for small font popups
static const Int16 gsPopup_ArrowButtonWidth 	= 	22;	//	Width used in drawing the arrow only

enum
{
	eSelectAll = 1,
	eUnselectAll = 2
};

CSelectFolderMenu::CSelectFolderMenu(LStream	*inStream):
LGAPopup(inStream),
mSelectedItemCount(0),
mTotalItemCount(0),
mMenuWidth(0)
{
	mDesiredFolderFlags = (FolderChoices)(eWantIMAP + eWantNews);
}

CSelectFolderMenu::~CSelectFolderMenu()
{
	
}

void CSelectFolderMenu::FinishCreateSelf()
{
	LGAPopup::FinishCreateSelf();
	mInitialMenuCount = CountMItems(GetMacMenuH());
	CMailFolderMixin::UpdateMailFolderMixinsNow(this);
//	SetArrowOnly(true);
	mTotalItemCount = MGetNumMenuItems() - mInitialMenuCount;
	for (int i = 1; i <= mTotalItemCount; ++i)
	{
		UInt32 folderPrefFlag = MGetFolder(i).GetFolderPrefFlags();
		if (folderPrefFlag & MSG_FOLDER_PREF_OFFLINE)
		{
			mSelectedItemCount++;
			Char16	mark = GetMenuFontSize () < 12 ? gsPopup_SmallMark : checkMark;
			::SetItemMark(GetMacMenuH(), i + mInitialMenuCount, mark);
		}
	}
	UpdateCommandMarks();
}

void
CSelectFolderMenu::SetupCurrentMenuItem(	MenuHandle	/*inMenuH*/,
											Int16		/*inCurrentItem*/)
{
	// Don't muck around with our marks!
}

void
CSelectFolderMenu::UpdateCommandMarks()
{
	short	mark;
	if (mSelectedItemCount == mTotalItemCount && mTotalItemCount)
	{
		// Mark the SelectAllCommand.
		mark = GetMenuFontSize () < 12 ? gsPopup_SmallMark : checkMark;
	}
	else
	{
		// Unmark the SelectAllCommand.
		mark = 0;
	}
	::SetItemMark(GetMacMenuH(), eSelectAll, mark);
	if (!mSelectedItemCount && mTotalItemCount)
	{
		// Mark the UnselectAllCommand.
		mark = GetMenuFontSize () < 12 ? gsPopup_SmallMark : checkMark;
	}
	else
	{
		// Unmark the UnselectAllCommand.
		mark = 0;
	}
	::SetItemMark(GetMacMenuH(), eUnselectAll, mark);
	if (!mTotalItemCount)
	{
		::DisableItem(GetMacMenuH(), eSelectAll);
		::DisableItem(GetMacMenuH(), eUnselectAll);
	}
	else
	{
		::EnableItem(GetMacMenuH(), eSelectAll);
		::EnableItem(GetMacMenuH(), eUnselectAll);
	}
}

void
CSelectFolderMenu::CommitCurrentSelections()
{
	for (int i = 1; i <= mTotalItemCount; ++i)
	{
		CMessageFolder currentFolder = MGetFolder(i);
		UInt32 folderPrefFlag = currentFolder.GetFolderPrefFlags();
		short	mark;
		::GetItemMark(GetMacMenuH(), i + mInitialMenuCount, &mark);
		folderPrefFlag = mark?	folderPrefFlag | MSG_FOLDER_PREF_OFFLINE:
								folderPrefFlag & ~MSG_FOLDER_PREF_OFFLINE;
		::MSG_SetFolderPrefFlags(currentFolder.GetFolderInfo(), folderPrefFlag);
	}
}

void
CSelectFolderMenu::SetValue(Int32 inValue)
{
	short	mark;
	if (inValue > eUnselectAll)
	{
		::GetItemMark(GetMacMenuH(), inValue, &mark);
		if (mark)
		{
			mSelectedItemCount--;
			mark = 0;
		}
		else
		{
			mSelectedItemCount++;
			mark = GetMenuFontSize () < 12 ? gsPopup_SmallMark : checkMark;
		}
		::SetItemMark(GetMacMenuH(), inValue, mark);
	}
	else
	{
		// Optimization if this option is already checked, then don't do anything
		if (inValue == eUnselectAll)
		{
			mark = 0;
			mSelectedItemCount = 0;
		}
		if (inValue == eSelectAll)
		{
			mark = GetMenuFontSize () < 12 ? gsPopup_SmallMark : checkMark;
			mSelectedItemCount = mTotalItemCount;
		}
		for (int i = 1; i <= mTotalItemCount; ++i)
		{
			::SetItemMark(GetMacMenuH(), i + mInitialMenuCount, mark);
		}
	}
	UpdateCommandMarks();
}
