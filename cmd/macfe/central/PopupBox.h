/*-----------------------------------------------------------------------------
	StdPopup
	Written 1994 Netscape Communications Corporation
	
	Portions derived from MacApp, 
	Copyright © 1984-1994 Apple Computer, Inc. All rights reserved.
-----------------------------------------------------------------------------*/

#pragma once
#ifndef _DISCRETE_LIST_BOX_
#define _DISCRETE_LIST_BOX_

/*-----------------------------------------------------------------------------
	StdPopup
	We need non-text menu items, so we will use a custom menu definition
	procedure.
	We need to have a dynamic number of menus, so we will use PopupMenuSelect.
	
	static Handle sMenuProc
	In order to use a custom procedure, we need to dispatch from a handle.
	This handle is constant and therefore can be global.
	
	static StdPopup * sMenuObject
	We can't figure out which object called PopupMenuSelect, so we will set
	this global right before calling it. We will never get a menu callback
	except from within PopupMenuSelect.

	Handle fDefaultMenuProc
	This is the normal definition procedure for the menu. We'll use it to
	handle most operations; only overriding drawing.
	
	MenuHandle fMenu
	We only need a menu widget when we're calling PopupMenuSelect; we could 
	actually just have 1 of them and reuse it amongst any number of objects, or
	give each item its own widget.

//	short item = 0;
//	DoMenuProc (mSizeMsg, m, gZeroRect, gZeroPt, item);
-----------------------------------------------------------------------------*/
#include <Menus.h>
#include <LAttachment.h>
#include <LGAPopup.h>
#include "PascalString.h"

class LP_Glypher;

#define dlbPopupMenuID 'cz'

class CGAPopupMenu;

// StdPopup

class StdPopup: public LAttachment {
public:
					StdPopup (CGAPopupMenu* target);
					~StdPopup ();
	// definition
	virtual short	GetCount ()=0;
	virtual CStr255	GetText (short item)=0;
	// interface
	Point			CalcTargetFrame (short & baseline);
	void			DirtyMenu ();
	
	ResIDT			GetTextTraits() const;
	ResIDT			SetTextTraits(ResIDT inTextTraits);
protected:
	MenuHandle		GetMacMenuH();
	
	virtual Boolean	NeedCustomPopup() const;
	
 	virtual void	SyncMenu (MenuHandle aquiredMenu);
	virtual short	CalcMaxWidth (MenuHandle aquiredMenu);
	void			DrawWidget (MenuHandle aquiredMenu, const Rect & widgetFrame);
	virtual void	ExecuteSelf (MessageT message, void *param);
	
	// do ScriptCode Menu Trick on IM:MacTbxEss Page3-46
	// Give subclass a chance to do it.
	virtual void	SetMenuItemText(MenuHandle aquiredMenu, int item, CStr255& itemText) 
						{ ::SetMenuItemText(aquiredMenu, item, itemText); } ;
	// Let us have a chance to override PopUpMenuSelect
	virtual	long	PopUpMenuSelect(MenuHandle aquiredMenu, short top, short left, short popUpItem)
						{ return ::PopUpMenuSelect( aquiredMenu,  top,  left,  popUpItem); };
	virtual	void 	DrawTruncTextBox (CStr255 text, const Rect& box);

	friend class TempUseMenu;
	CGAPopupMenu*	fTarget;
	MenuHandle	fMenu; // this is used if we're doing the  UTF8 stuff
private:
	Boolean			fDirty;
};


class	CGAPopupMenu : public LGAPopup {
friend class StdPopup;
public:
	enum { class_ID = 'Gatt' };
	typedef LGAPopup super;
	
				CGAPopupMenu(LStream* inStream);
	virtual		~CGAPopupMenu();
	
	void SetTextTraits(ResIDT inTextTraitsID) { mTextTraitsID = inTextTraitsID; }
	ResIDT GetTextTraits() const { return mTextTraitsID; }

	void SetNeedCustomDrawFlag(Boolean needCustomDraw) { mNeedCustomDraw = needCustomDraw; }
// For UTF8
	//----<< ¥ DRAWING ¥ >>------------------------------------------------------------

	virtual	void			DrawPopupTitle			();
	virtual	void			CalcTitleRect				( Rect	&outRect );		// ¥ OVERRIDE
protected:
	//----<< ¥ POPUP MENU HANDLING ¥ >>------------------------------------------------

	virtual	void			HandlePopupMenuSelect	(	Point		inPopupLoc,
																	Int16		inCurrentItem,
																	Int16		&outMenuID,
																	Int16		&outMenuItem );


private:
	Boolean mNeedCustomDraw;
};

#endif /* _DISCRETE_LIST_BOX_ */

