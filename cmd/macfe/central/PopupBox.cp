/*-----------------------------------------------------------------------------
	StdPopup
	Written 1994 Netscape Communications Corporation
	
	Portions derived from MacApp, 
	Copyright й 1984-1994 Apple Computer, Inc. All rights reserved.
-----------------------------------------------------------------------------*/

// primary header
#include "PopupBox.h"
// local libraries
// cross-platform libraries
#include "xpassert.h"
#include "xp_trace.h"
// PowerPlant
#include <UDrawingState.h>
#include <URegistrar.h>
#include <LStdControl.h>
// Macintosh headers
#include <Icons.h>
#include <Memory.h>
#include <Menus.h>
#include <OSUtils.h>
#include <Traps.h>
// ANSI headers
#include <stdio.h>
#include <UGAColorRamp.h>
#include <UGraphicsUtilities.h>
#include "UFontSwitcher.h"
#include "UUTF8TextHandler.h"
#include "UPropFontSwitcher.h"
#include "UCustomizePopUp.h"
#include "LCustomizeMenu.h"


//-----------------------------------------------------------------------------
// random stuff
//-----------------------------------------------------------------------------

#define SETMODRECT(DEST,SOURCE,TOP,LEFT,BOTTOM,RIGHT) \
	SetRect (&(DEST), (SOURCE).LEFT, (SOURCE).TOP, (SOURCE).RIGHT, (SOURCE).BOTTOM)


#ifndef GetMenuProc
#define GetMenuProc(menu)	(*((Handle *) ((*((Ptr *) (menu))) + 0x06)))
#endif


//-----------------------------------------------------------------------------
// Discrete List Box
//-----------------------------------------------------------------------------
const Int16	fontNumber_Unknown	= -1;

StdPopup::StdPopup (CGAPopupMenu * target)
:	LAttachment ()
{
	ThrowIfNil_(fTarget = target);
	fTarget->SetNeedCustomDrawFlag(NeedCustomPopup());
	mExecuteHost = false;
	fMenu = nil;
	fDirty = true;
}

StdPopup::~StdPopup ()
{
	if (fMenu)
		DisposeMenu (fMenu);
}

//
// definition
//

short
StdPopup::GetCount ()
{
	return 0;
}

CStr255	
StdPopup::GetText (short item)
{
	char buffer [20];
	sprintf (buffer, "%hi", item);
	return buffer;
}

//
// interface
//

const int tlo = 18; // offset of text from left side of widget
const int tls = 23; // offset from right side of widget to left side of triangle icon

Point
StdPopup::CalcTargetFrame (short & baseline)
{
	SyncMenu (fTarget->GetMacMenuH());
	
	Point size;
	size.v = /* text */ 16 + /* border */ 3;
	size.h = CalcMaxWidth (fTarget->GetMacMenuH()) + tls;
	
	StColorPenState saveColorPenState;
	StTextState saveTextState;
	saveColorPenState.Normalize();
	saveTextState.Normalize();
	
	FontInfo fontInfo;
	GetFontInfo (&fontInfo);
	baseline = 1 + fontInfo.ascent;
	size.v = fontInfo.ascent+fontInfo.descent+fontInfo.leading+ 2;
	return size;
}

void
StdPopup::DirtyMenu ()
{
	fDirty = true;
}

//
// internal
//

short
StdPopup::CalcMaxWidth (MenuHandle aquiredMenu)
{
	if (1 || fDirty)
	{
		Rect menuRect;
		Point hitPt = {0,0};
		short whichItem;
		MenuDefUPP * menuProc = (MenuDefUPP*) (*aquiredMenu)->menuProc;
		
		SInt8 theState = HGetState((*aquiredMenu)->menuProc);
		HLock((*aquiredMenu)->menuProc);
		CallMenuDefProc (*menuProc, mSizeMsg, aquiredMenu, &menuRect, hitPt, &whichItem);
		HSetState((*aquiredMenu)->menuProc, theState);
	}
	return (*aquiredMenu)->menuWidth;
}

void StdPopup::DrawTruncTextBox (CStr255 text, const Rect& box)
{
/*
        Truncates the text before drawing.
        Does not word wrap.
*/
        FontInfo fontInfo;
        GetFontInfo (&fontInfo);
        MoveTo (box.left, box.bottom - fontInfo.descent -1);
        TruncString (box.right - box.left, text, truncEnd);
        DrawString (text);
}

void StdPopup::DrawWidget (MenuHandle aquiredMenu, const Rect & widgetFrame)
{
	StColorPenState saveColorPenState;
	StTextState saveTextState;
	saveColorPenState.Normalize();
	saveTextState.Normalize();
	
	if (GetTextTraits() != fontNumber_Unknown)
		UTextTraits::SetPortTextTraits(GetTextTraits());
	Rect r;
	
	SETMODRECT(r,widgetFrame,top+1,left+1,bottom-2,right-2);
	EraseRect (&r);
	
	MoveTo (widgetFrame.left + 3, widgetFrame.bottom - 1);
	LineTo (widgetFrame.right -1, widgetFrame.bottom - 1);
	MoveTo (widgetFrame.right -1, widgetFrame.top + 3);
	LineTo (widgetFrame.right -1, widgetFrame.bottom - 1);
	SETMODRECT(r,widgetFrame,top,left,bottom-1,right-1);
	FrameRect (&r);
	
	SETMODRECT(r,widgetFrame,top-1,right-tls,top-1+16,right-tls+16);
	::PlotIconID (&r, atNone, ttNone, 'cz');
	
	short whichItem = fTarget->GetValue();
	CStr255 itemText;
	if (whichItem)
		GetMenuItemText (aquiredMenu, whichItem, itemText);
	SETMODRECT(r,widgetFrame,top+1,left+tlo,bottom-1,right-tls);
	DrawTruncTextBox (itemText, r);
}

void
StdPopup::ExecuteSelf (MessageT message, void *param)
{
	fTarget->SetNeedCustomDrawFlag(NeedCustomPopup());
	switch (message) {
	case msg_DrawOrPrint: {
		// 97-06-07 pkc -- put back SyncMenu otherwise Javascript reflection back
		// into popup menu list is broken
		SyncMenu (fTarget->GetMacMenuH());
		mExecuteHost = true;
		break;
	}
	case msg_Click: {
		SMouseDownEvent* event = (SMouseDownEvent*) param;
		ThrowIfNil_(event);
		{
											// Determine which HotSpot was clicked
			Int16	theHotSpot = fTarget->FindHotSpot(event->whereLocal);
			
			if (theHotSpot > 0)
			{
				fTarget->FocusDraw();
											// Track mouse while it is down
				if (fTarget->TrackHotSpot(theHotSpot, event->whereLocal, event->macEvent.modifiers))
				{
											// Mouse released inside HotSpot
					fTarget->HotSpotResult(theHotSpot);
				}
			}
			
			mExecuteHost = false;			
		}
		break;
	}
	}
}

void
StdPopup::SyncMenu (MenuHandle aquiredMenu)
{
	if (!fDirty)
		return;
	
	int current = CountMItems (aquiredMenu);
	int want = GetCount();
	int add = want - current;
	if (0 < add) {
		for (int i = 1; i <= add; i++)
			AppendMenu (aquiredMenu, "\pTest");
	}
	else if (add < 0) {
		for (int i = 1; i <= -add; i++)
			DeleteMenuItem (aquiredMenu, want + 1);
	}
	
	for (int item = 1; item <= want; item++)
	{
		CStr255		itemText;
		itemText = GetText( item );
		if ( itemText[ 1 ] == '-' )
			itemText = " " + itemText;
		SetMenuItemText (aquiredMenu, item, itemText );
	}
	if (fTarget->GetMaxValue() != want)
		fTarget->SetMaxValue (want);
	
	(*aquiredMenu)->menuWidth += tls;
	fDirty = false;
}


Boolean	StdPopup::NeedCustomPopup() const
{
	return false;
}

ResIDT StdPopup::GetTextTraits() const
{
	return fTarget->GetTextTraits();
}

/*-----------------------------------------------------------------------------
	LCustomizeFontMenu
-----------------------------------------------------------------------------*/
class LCustomizeFontMenu : public LCustomizeMenu {
public: 
	LCustomizeFontMenu(short fontNum);
	virtual void Draw	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item);
	virtual void Size	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item);
	virtual void Choose(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item);
protected:
	virtual void DrawItemText( Rect& itemrect, Str255 itemtext );
	virtual void SetupFont() { ::TextFont(fFontNum); };
private:
	short fFontNum;
};

#pragma mark == LCustomizeFontMenu ==

LCustomizeFontMenu::LCustomizeFontMenu(short fontNum)
	: LCustomizeMenu()
{
	fFontNum = fontNum;
}

void LCustomizeFontMenu::Draw(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item)
{
	SetupFont();
	LCustomizeMenu::Draw(menu,  root, rect, hitPt, item);
}
void LCustomizeFontMenu::Size(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item)
{
	SetupFont();
	LCustomizeMenu::Size(menu,  root, rect, hitPt, item);
}
void LCustomizeFontMenu::Choose(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item)
{
	SetupFont();
	LCustomizeMenu::Choose(menu,  root, rect, hitPt, item);
}
void LCustomizeFontMenu::DrawItemText( Rect& itemrect, Str255 itemtext )
{
	SetupFont();
	LCustomizeMenu::DrawItemText(itemrect,itemtext);
}

/*-----------------------------------------------------------------------------
	LMultiFontTextMenu
-----------------------------------------------------------------------------*/
class LMultiFontTextMenu : public LCustomizeMenu {
public: 
	LMultiFontTextMenu(UMultiFontTextHandler* texthandler, UFontSwitcher* fs);
protected:
	virtual void DrawItemText( Rect& itemrect, Str255 itemtext )
		{ fTextHandler->DrawString(fFontSwitcher, itemtext); }
		
	virtual short MeasureItemText(Str255 itemtext )	
		{ return fTextHandler->StringWidth(fFontSwitcher, itemtext); };
private:
	UMultiFontTextHandler*	fTextHandler;
	UFontSwitcher*			fFontSwitcher;
};

LMultiFontTextMenu::LMultiFontTextMenu(UMultiFontTextHandler* texthandler, UFontSwitcher* fs)
	: LCustomizeMenu()
{
	fTextHandler = texthandler;
	fFontSwitcher = fs;
}

#pragma mark -

// ===========================================================================
// е CGAPopupMenu											   CGAPopupMenu е
// ===========================================================================

// ---------------------------------------------------------------------------
//		е CGAPopupMenu(LStream*)
// ---------------------------------------------------------------------------
//	Construct from data in a Stream

CGAPopupMenu::CGAPopupMenu(
	LStream	*inStream)
		:	mNeedCustomDraw(false),
		
			super(inStream)
{	
}

CGAPopupMenu::~CGAPopupMenu()
{
}
//-------------------------------------------------------------------------------------
// CGAPopupMenu::DrawPopupTitle
//-------------------------------------------------------------------------------------

void
CGAPopupMenu::DrawPopupTitle	()
{
	if(! mNeedCustomDraw)
	{	// hacky way, depend on mIsUTF8 to decide wheather we use the super class implementation
		super::DrawPopupTitle();
		return;
	}
	StColorPenState	theColorPenState;
	StTextState 		theTextState;
	
	// е Get some loal variables setup including the rect for the title
	ResIDT	textTID = GetTextTraitsID ();
	Rect	titleRect;
	Str255 title;
	GetCurrentItemTitle ( title );
	
	// е Figure out what the justification is from the text trait and 
	// get the port setup with the text traits
	UTextTraits::SetPortTextTraits ( textTID );
	
	// е Setup the title justification which is always left justified
	Int16	titleJust = teFlushLeft;
	
	// е Calculate the title rect
	CalcTitleRect ( titleRect );
	
	// е Setup the text color which by default is black
	RGBColor	textColor;
	::GetForeColor ( &textColor );
	
	// е Get the current item's title
	Str255 currentItemTitle;
	GetCurrentItemTitle ( currentItemTitle );

	// е Loop over any devices we might be spanning and handle the drawing
	// appropriately for each devices screen depth
	StDeviceLoop	theLoop ( titleRect );
	Int16				depth;
	while ( theLoop.NextDepth ( depth )) 
	{
		if ( depth < 4 )		//	е BLACK & WHITE
		{
			// е If the control is dimmed then we use the grayishTextOr 
			// transfer mode to draw the text
			if ( !IsEnabled ())
			{
				::RGBForeColor ( &UGAColorRamp::GetBlackColor () );
				::TextMode ( grayishTextOr );
			}
			else if ( IsEnabled () && IsHilited () )
			{
				// е When we are hilited we simply draw the title in white
				::RGBForeColor ( &UGAColorRamp::GetWhiteColor () );
			}
			
			// е Now get the actual title drawn with all the appropriate settings
			UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
			UFontSwitcher *fs = UPropFontSwitcher::Instance();
			
			FontInfo info;	
			th->GetFontInfo(fs, &info);
			::MoveTo(titleRect.left,
					 (titleRect.top + titleRect.bottom + info.ascent - info.descent ) / 2);
			th->DrawString(fs, currentItemTitle);	
		}
		else	// е COLOR
		{
			// е If control is selected we always draw the text in the title
			// hilite color, if requested
			if ( IsHilited ())
				::RGBForeColor ( &UGAColorRamp::GetWhiteColor() );
		
			// е If the box is dimmed then we have to do our own version of the
			// grayishTextOr as it does not appear to work correctly across
			// multiple devices
			if ( !IsEnabled () || !IsActive ())
			{
				textColor = UGraphicsUtilities::Lighten ( &textColor );
				::TextMode ( srcOr );
				::RGBForeColor ( &textColor );
			}
				
			// е Now get the actual title drawn with all the appropriate settings
			UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
			UFontSwitcher *fs = UPropFontSwitcher::Instance();
			
			FontInfo info;	
			th->GetFontInfo(fs, &info);
			::MoveTo(titleRect.left,
					 (titleRect.top + titleRect.bottom + info.ascent - info.descent) / 2);
			th->DrawString(fs, currentItemTitle);	
		}	
	}
														
}	//	CGAPopupMenu::DrawPopupTitle
//-------------------------------------------------------------------------------------
//
//	This method is used to calculate the title rect for the currently selected item in
// 	the popup, this title is drawn inside the popup
const Int16 gsPopup_RightInset 			= 	24;	//	Used to position the title rect
const Int16 gsPopup_TitleInset			= 	8;		// Apple specification
void
CGAPopupMenu::CalcTitleRect	( Rect	&outRect )
{
	if(! mNeedCustomDraw)
	{	// hacky way, depend on mIsUTF8 to decide wheather we use the super class implementation
		super::CalcTitleRect(outRect);
		return;
	}	
	StTextState			theTextState;
	StColorPenState	thePenState;
	Int16		bevelWidth = 2;
	
	// е Get some loal variables setup including the rect for the title
	ResIDT	textTID = GetTextTraitsID ();

	// е Get the port setup with the text traits
	UTextTraits::SetPortTextTraits ( textTID );
	
	// е Figure out the height of the text for the selected font
	FontInfo fInfo;
	UFontSwitcher *fs = UPropFontSwitcher::Instance();
	UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
	th->GetFontInfo(fs, &fInfo);
	
	Int16		textHeight = fInfo.ascent + fInfo.descent;
	Int16		textBaseline = fInfo.ascent;
	
	// е Get the local inset frame rectangle
	CalcLocalPopupFrameRect ( outRect );
	::InsetRect ( &outRect, 0, bevelWidth );
	outRect.right -= gsPopup_RightInset;
	outRect.left += gsPopup_TitleInset;
	
	// е Adjust the title rect to match the height of the font
	outRect.top += (( UGraphicsUtilities::RectHeight ( outRect ) - textBaseline) / 2) - 2;
	outRect.bottom = outRect.top + textHeight;
		
}	//	CGAPopupMenu::CalcTitleRect
//=====================================================================================
// ее POPUP MENU HANDLING
//-------------------------------------------------------------------------------------
// CGAPopupMenu::HandlePopupMenuSelect
//-------------------------------------------------------------------------------------

void
CGAPopupMenu::HandlePopupMenuSelect	(	Point		inPopupLoc,
												Int16		inCurrentItem,
												Int16		&outMenuID,
												Int16		&outMenuItem )
{
	MenuHandle	menuH = GetMacMenuH ();
	ThrowIfNil_ ( menuH );
	if ( menuH )
	{
		// BUG#69583: Make sure we *do* use the system font so that the current
		// item will be checked properly as in Akbar. So we don't do the LMSetSysFont
		// stuff that LGAPopup does.

		// е Handle the actual insertion into the hierarchical menubar
		::InsertMenu ( menuH, hierMenu );
		
		FocusDraw ();

		// е Before we display the menu we need to make sure that we have the
		// current item marked in the menu. NOTE: we do NOT use the current
		// item that has been passed in here as that always has a value of one
		// in the case of a pulldown menu
		SetupCurrentMenuItem ( menuH, GetValue () );

		// е Then we call PopupMenuSelect and wait for it to return
		
		Int32 result;
		
		// hacky way, depend on mIsUTF8 to decide which implementation to use
		if (!mNeedCustomDraw)
		{
			result = ::PopUpMenuSelect(menuH, inPopupLoc.v, inPopupLoc.h, inCurrentItem );
		}
		else
		{
		 	LMultiFontTextMenu utf8menu(UUTF8TextHandler::Instance(), UPropFontSwitcher::Instance());
			result = UCustomizePopUp::PopUpMenuSelect(menuH, &utf8menu, inPopupLoc.v, inPopupLoc.h, inCurrentItem);
		}

		// е Then we extract the values from the returned result
		// these are then passed back out to the caller
		outMenuID = HiWord ( result );
		outMenuItem = LoWord ( result );

		// е Finally get the menu removed
		::DeleteMenu ( GetPopupMenuResID ());
	}
}	//	CGAPopupMenu::HandlePopupMenuSelect
