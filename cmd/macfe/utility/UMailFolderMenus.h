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

#ifndef __H_UMailFolderMenus
#define __H_UMailFolderMenus
#pragma once
/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 12 DEC 96

	DESCRIPTION:	Mixin classes for displaying a UI menu containing a list of current 
					mail folders.

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LVariableArray.h>

#include "LStdControl.h"
#include "CGAIconPopup.h"
#include "LGAIconButtonPopup.h"

#include "msgcom.h"


#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

const Int16 cMaxMailFolderNameLength = 511;

#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

class CMessageFolder;
class RichItemDataYadaYada;

//======================================
class CMailFolderMixin
// Abstract class for use with basic menu population and management
//======================================
{
	friend pascal void MyMercutioCallback(
		Int16 menuID, Int16 previousModifiers, RichItemDataYadaYada& itemData);
					  
public:

	// Public interface
	
						// Static public method for updating all CMailFolderMixin objects
	static void			UpdateMailFolderMixins();
	static void			UpdateMailFolderMixinsNow(CMailFolderMixin *inSingleMailFolderMixin = nil);
	static void			GetFolderNameForDisplay(char * inName, CMessageFolder& folder);

						enum { eUseFolderIcons = true, eNoFolderIcons = false };
						CMailFolderMixin(Boolean inIncludeFolderIcons = eUseFolderIcons);
	virtual				~CMailFolderMixin(void);
	
	// Following two deliberately return by value.  CMessageFolder is only 4 bytes, and
	// is reference counted.  The first indexes only into the folder list.  The second
	// takes account of any initial non-folder items that might be in the menu.
	virtual CMessageFolder	MGetFolder(ArrayIndexT inItemNumber);
	virtual CMessageFolder	MGetFolderFromMenuItem(ArrayIndexT inItemNumber);

	virtual const char*	MGetFolderName(ArrayIndexT inItemNumber);
	ResIDT				GetMailFolderIconID(UInt16 inFlags);
	
						// Get the currently selected folder name
	virtual const char*	MGetSelectedFolderName();
	virtual CMessageFolder MGetSelectedFolder();

	virtual Boolean		MSetSelectedFolder(const CMessageFolder&, Boolean inDoBroadcast = true);
	virtual Boolean		MSetSelectedFolder(const MSG_FolderInfo*, Boolean inDoBroadcast = true);
	virtual Boolean		MSetSelectedFolderName(const char* inName, Boolean inDoBroadcast = true);
							// Needed only by filters.

	enum FolderChoices
	{
		eWantPOP =		1 << 0,
		eWantIMAP =		1 << 1,
		eWantNews =		1 << 2,
		eWantHosts =	1 << 3,
		eWantDividers =	1 << 4
	};
	void				MSetFolderChoices(FolderChoices inChoices) { mDesiredFolderFlags = inChoices; }

protected:

						// These two methods are called by automatically by CMailFolderMixins 
						// when they are created and destroyed
	static void			RegisterMailFolderMixin(CMailFolderMixin *inMailFolderMixin);
	static void			UnregisterMailFolderMixin(CMailFolderMixin *inMailFolderMixin);
	
	void				MGetCurrentMenuItemName(Str255 outItemName);
	virtual	Boolean		FolderFilter(const CMessageFolder &folder);

						// Populate the menu with current mail folders
	virtual Boolean		MPopulateWithFolders(const MSG_FolderInfo **inFolderInfoArray,
											 const Int32 inNumFolders);

// Pure virtual methods
	
	virtual Int16		MGetNumMenuItems(void) = 0;
	virtual Int16		MGetSelectedMenuItem(void) = 0;
	virtual void		MSetSelectedMenuItem(Int16 inIndex, Boolean inDoBroadcast = true) = 0;
	virtual Boolean		MUpdateMenuItem(Int16 inMenuItem, ConstStr255Param inName, 
									    ResIDT inIconID = 0) = 0;
	virtual void		MAppendMenuItem(ConstStr255Param inName, ResIDT inIconID = 0) = 0;
	virtual void		MClipMenuItems(Int16 inStartClipItem) = 0;
	virtual void		MGetMenuItemName(Int16 inIndex, Str255 outItemName) = 0;
	virtual MenuHandle	MGetSystemMenuHandle(void) = 0;
	virtual void		MRefreshMenu(void) = 0;
	
// Instance variables
		
	LArray				mFolderArray;	// ... or must we? jrm 97/03/18
	Boolean				mUseFolderIcons;	// Use icons in the menu?

	static LArray		sMailFolderMixinRegisterList;	// List of registered CMailFolderMixins
	FolderChoices		mDesiredFolderFlags;
	static Boolean		sMustRebuild;
	static void*		sMercutioCallback;
	Int32				mInitialMenuCount;
};


// Abstract class for use with control popup menus
class CMailFolderPopupMixin : public CMailFolderMixin {
					  
public:

						CMailFolderPopupMixin(void) {
						}
					
protected:

	virtual Int16		MGetNumMenuItems(void);
	virtual Int16		MGetSelectedMenuItem(void);
	virtual void		MSetSelectedMenuItem(Int16 inIndex, Boolean inDoBroadcast = true);
	virtual Boolean		MUpdateMenuItem(Int16 inMenuItem, ConstStr255Param inName, 
									    ResIDT inIconID = 0);
	virtual void		MAppendMenuItem(ConstStr255Param inName, ResIDT inIconID = 0);
	virtual void		MClipMenuItems(Int16 inStartClipItem);
	virtual void		MGetMenuItemName(Int16 inMenuItem, Str255 outItemName);
	virtual void		MRefreshMenu(void);
	
	LControl			*MGetControl(void) {
							LControl *theControl = dynamic_cast<LControl *>(this);
							Assert_(theControl != nil);
							return theControl;
						}
};


/*======================================================================================
	Mixin with a CGAIconPopup class or descendent.
	
	Use as follows:
	
		class MyPopup : public CGAIconPopup,
						public CGAIconPopupFolderMixin {
								 
				... Methods
		}
		
	Make sure to call CMailFolderMixin::UpdateMailFolderMixins(this) from 
	MyPopup::FinishCreateSelf().
======================================================================================*/

class CGAIconPopupFolderMixin : public CMailFolderPopupMixin {
					  
public:

						CGAIconPopupFolderMixin(void) {
						}
					
protected:

	virtual MenuHandle	MGetSystemMenuHandle(void);
	virtual void		MRefreshMenu(void);

	CGAIconPopup		*MGetControl(void) {
							CGAIconPopup *theControl = dynamic_cast<CGAIconPopup *>(this);
							Assert_(theControl != nil);
							return theControl;
						}
};


/*======================================================================================
	Mixin with a LStdPopupMenu class or descendent.
	
	Use as follows:
	
		class MyPopup : public LGAPopup,
						public CGAdPopupFolderMixin {
								 
				... Methods
		}
		
	Make sure to call CMailFolderMixin::UpdateMailFolderMixins(this) from 
	MyPopup::FinishCreateSelf().
======================================================================================*/

class CGAPopupFolderMixin : public CMailFolderPopupMixin {

public:

						CGAPopupFolderMixin(void)
						{
						}
					
protected:

	virtual MenuHandle	MGetSystemMenuHandle(void);

	LGAPopup		*MGetControl(void) {
							LGAPopup *theControl = dynamic_cast<LGAPopup *>(this);
							Assert_(theControl != nil);
							return theControl;
						}
};


/*======================================================================================
	Mixin with a LGAIconButtonPopup class or descendent.
	
	Use as follows:
	
		class MyPopup : public LGAIconButtonPopup,
						public CGAIconButtonPopupFolderMixin {
								 
				... Methods
		}
		
	Make sure to call CMailFolderMixin::UpdateMailFolderMixins(this) from 
	MyPopup::FinishCreateSelf().
======================================================================================*/

class CGAIconButtonPopupFolderMixin : public CMailFolderPopupMixin {

public:

						CGAIconButtonPopupFolderMixin(void) {
						}
					
protected:

	virtual MenuHandle	MGetSystemMenuHandle(void);
	virtual void		MRefreshMenu(void);

	LGAIconButtonPopup	*MGetControl(void) {
							LGAIconButtonPopup *theControl = dynamic_cast<LGAIconButtonPopup *>(this);
							Assert_(theControl != nil);
							return theControl;
						}
};


/*======================================================================================
	Mixin with a LMenu class or descendent.
	
	Use as follows:
	
		class MyPopup : public LMenu,
						public CMenuMailFolderMixin {
								 
				... Methods
		}
		
	After creating the menu, it should be added to the menu bar using the 
	LMenuBar::InstallMenu() method.
	
	For each menu item (i.e. folder) a synthetic command ID is created.
		
	Make sure to call CMailFolderMixin::UpdateMailFolderMixins(this) after constructing
	the object.
======================================================================================*/

class CMenuMailFolderMixin : public CMailFolderMixin {
					  
public:

						CMenuMailFolderMixin(CommandT inMenuCommand = cmd_UseMenuItem) {
							mMenuCommand = inMenuCommand;
						}
					
protected:

	virtual Int16		MGetNumMenuItems(void);
	virtual Int16		MGetSelectedMenuItem(void);
	virtual void		MSetSelectedMenuItem(Int16 inIndex, Boolean inDoBroadcast = true);
	virtual Boolean		MUpdateMenuItem(Int16 inMenuItem, ConstStr255Param inName, 
									    ResIDT inIconID = 0);
	virtual void		MAppendMenuItem(ConstStr255Param inName, ResIDT inIconID = 0);
	virtual void		MClipMenuItems(Int16 inStartClipItem);
	virtual void		MGetMenuItemName(Int16 inMenuItem, Str255 outItemName);
	virtual void		MRefreshMenu(void);
	virtual MenuHandle	MGetSystemMenuHandle(void);
	
	LMenu				*MGetMenu(void) {
							LMenu *theMenu = dynamic_cast<LMenu *>(this);
							Assert_(theMenu != nil);
							return theMenu;
						}
						
	// Instance variables
		
	CommandT			mMenuCommand;
};


#pragma mark
class CSelectFolderMenu : public LGAPopup, public CGAPopupFolderMixin
{
	public:
		enum
		{
			class_ID = 'SelF'
		};

						CSelectFolderMenu(LStream	*inStream);
		virtual			~CSelectFolderMenu();
		virtual	void	FinishCreateSelf();
		virtual	void	SetupCurrentMenuItem(MenuHandle inMenuH, Int16 inCurrentItem);
		virtual void	SetValue(Int32 inValue);

		void			CommitCurrentSelections();

	private:
		void			UpdateCommandMarks();

		Int16	mSelectedItemCount;
		Int16	mTotalItemCount;
		Int16	mMenuWidth;
};

#endif // __H_UMailFolderMenus

