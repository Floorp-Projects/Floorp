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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMailFolderButtonPopup.h

#pragma once

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CPatternButtonPopup.h"
#include "CPatternButtonPopupText.h"
#include "UMailFolderMenus.h"

/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark - CMailFolderButtonPopup

//======================================
class CMailFolderButtonPopup : public CPatternButtonPopup,
							   public CMailFolderPopupMixin
// Class for the button popup
//======================================
{				  
public:

						enum { class_ID = 'MfPu' };
						
						CMailFolderButtonPopup(LStream *inStream) :
											   CPatternButtonPopup(inStream) {
						}

	
protected:

	virtual void		FinishCreateSelf(void);
	virtual void		BroadcastValueMessage(void);
	virtual Boolean		TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers);

	virtual MenuHandle	MGetSystemMenuHandle(void);
	virtual void		MRefreshMenu(void);
};

#pragma mark - CMailFolderPatternTextPopup

//======================================
class CMailFolderPatternTextPopup : public CPatternButtonPopupText,
							 public CGAPopupFolderMixin
// Class for the relocation popup menu in the thread pane
//======================================
{				  
private:
	typedef CPatternButtonPopupText Inherited;
						
public:
						enum { class_ID = 'MfPT' };

						CMailFolderPatternTextPopup(LStream *inStream);

protected:

	virtual void		FinishCreateSelf(void);
	virtual void		BroadcastValueMessage(void);
	virtual Boolean		TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers);
	virtual void		HandlePopupMenuSelect(	Point		inPopupLoc,
												Int16		inCurrentItem,
												Int16		&outMenuID,
												Int16		&outMenuItem);

	virtual MenuHandle	MGetSystemMenuHandle(void);
	virtual void		MRefreshMenu(void);

}; // class CMailFolderPatternTextPopup

#pragma mark - CMailFolderGAPopup

//======================================
class CMailFolderGAPopup : public LGAPopup,
							 public CGAPopupFolderMixin
// Class for the relocation popup menu in the thread pane
//======================================
{				  
public:

						enum { class_ID = 'MfPM' };
						
						CMailFolderGAPopup(LStream *inStream);

	
protected:

	virtual void		FinishCreateSelf(void);
	virtual void		BroadcastValueMessage(void);
	virtual Boolean		TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers);
	virtual void		HandlePopupMenuSelect(	Point		inPopupLoc,
												Int16		inCurrentItem,
												Int16		&outMenuID,
												Int16		&outMenuItem);

	virtual MenuHandle	MGetSystemMenuHandle(void);
	virtual void		MRefreshMenu(void);
}; // class CMailFolderGAPopup

#pragma mark - CFolderScopeGAPopup

//======================================
class CFolderScopeGAPopup : public CMailFolderGAPopup
// differs from CMailFolderGAPopup only in the setting of mDesiredFolderFlags
//======================================
{				  
public:

						enum { class_ID = 'SsPM' };
						
						CFolderScopeGAPopup(LStream *inStream);
}; // class CFolderScopeGAPopup

#pragma mark - CFolderMoveGAPopup

//======================================
class CFolderMoveGAPopup : public CMailFolderGAPopup
//======================================
{				  
	typedef	CMailFolderGAPopup	Inherited;

public:
	
						enum { class_ID = 'SsFM' };
						
						CFolderMoveGAPopup(LStream *inStream);
	virtual				~CFolderMoveGAPopup();
	
protected:
	virtual Boolean		TrackHotSpot(
								Int16				inHotSpot,
								Point				inPoint,
								Int16 				inModifiers);
	
}; // class CFolderScopeGAPopup

#pragma mark - CMailFolderSubmenu

//======================================
class CMailFolderSubmenu : public LMenu,
						   public CMenuMailFolderMixin
// Class for the hierarchical menu
//======================================
{				  
public:

	static void			InstallMailFolderSubmenus(void);
	static void			RemoveMailFolderSubmenus(void);
	static void			SetSelectedFolder(const MSG_FolderInfo* inInfo);
	static Boolean		IsMailFolderCommand(CommandT *ioMenuCommand, const char** outName = nil);

						CMailFolderSubmenu(Int16 inMENUid, 
										   CommandT inMenuCommand = cmd_UseMenuItem) :
										   LMenu(inMENUid),
										   CMenuMailFolderMixin(inMenuCommand) {
						}
						
protected:

						enum {
							menuID_MoveMessage = 44
						,	menuID_CopyMessage = 46
						};

	static void			CreateMenus(void);
	
	// Instance variables
	
	// Class variables
	
	static CMailFolderSubmenu	*sMoveMessageMenu;
	static CMailFolderSubmenu	*sCopyMessageMenu;
};

