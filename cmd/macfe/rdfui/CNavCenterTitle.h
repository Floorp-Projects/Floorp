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

//
// Mike Pinkerton, Netscape Communications
//
// Class that draws the header area for the nav center which shows the title
// of the currently selected view as well as harboring the closebox for
// an easy way to close up the navCenter shelf.
//

#include "htrdf.h"
#include "CGrayBevelView.h"
#include "CImageIconMixin.h"
#include "CColorCaption.h"

extern RDF_NCVocab gNavCenter;			// RDF vocab struct for NavCenter


class CNavCenterStrip 
	: public CGrayBevelView, public CTiledImageMixin, public LCommander
{
		// Construction, Destruction
	CNavCenterStrip(LStream *inStream);
	~CNavCenterStrip();

protected:

		// override to use various RDF properties
	virtual void* BackgroundURLProperty ( ) const = 0;
	virtual void* BackgroundColorProperty ( ) const = 0;
	virtual void* ForegroundColorProperty ( ) const = 0;
	
		// override to change properties based on a new view
	virtual void SetView ( HT_View inView ) = 0;
	virtual HT_View GetView ( ) const { return mView; }
		
		// PowerPlant overrides
	virtual void ListenToMessage ( MessageT inMessage, void* ioParam ) ;
	virtual void AdjustCursorSelf( Point inPoint, const EventRecord& inEvent ) ;
	virtual void FindCommandStatus ( CommandT inCommand, Boolean &outEnabled,
										Boolean	&outUsesMark, Char16 &outMark, Str255 outName) ;
	virtual void ClickSelf ( const SMouseDownEvent & inMouseDown ) ;
	
	virtual	void DrawBeveledFill ( ) ;
	virtual void DrawStandby ( const Point & inTopLeft, IconTransformType inTransform ) const;
	virtual void EraseBackground ( HT_Resource inTopNode ) const ;
	virtual void ImageIsReady ( ) ;
	
private:

	HT_View		mView;				// ref back to current view for custom drawing 
		
}; // class CNavCenterTitle


class CNavCenterCaption : public CChameleonCaption {
public:
	enum { class_ID = 'ccp8' };
	CNavCenterCaption(LStream *inStream);
protected:
	virtual void		DrawText(Rect frame, Int16 inJust);	
};

class CNavCenterTitle : public CNavCenterStrip
{
public:	
	enum { class_ID = 'hbar', kTitlePaneID = 'titl' };

	CNavCenterTitle(LStream *inStream);
	~CNavCenterTitle();

		// Provide access to the LCaption that displays the title
	CChameleonCaption& TitleCaption ( ) { return *mTitle; }
	const CChameleonCaption& TitleCaption ( ) const { return *mTitle; }
	
protected:

	virtual void SetView ( HT_View inView ) ;
	virtual void FinishCreateSelf ( ) ;
	
	virtual void* BackgroundURLProperty ( ) const { return gNavCenter->titleBarBGURL; }
	virtual void* BackgroundColorProperty ( ) const { return gNavCenter->titleBarBGColor; }
	virtual void* ForegroundColorProperty ( ) const { return gNavCenter->titleBarFGColor; } 

private:

	CChameleonCaption*	mTitle;

}; // class CNavCenterTitle



class CNavCenterCommandStrip : public CNavCenterStrip
{
public:
	enum { class_ID = 'tcmd', kAddPagePaneID = 'addp', kManagePaneID = 'edit', kClosePaneID = 'clos' } ;
	enum { msg_CloseShelfNow = 'clos', msg_AddPage = 'addp', msg_Manage = 'edit' } ;

		// Construction, Destruction
	CNavCenterCommandStrip(LStream *inStream);
	~CNavCenterCommandStrip();
	
private:

	virtual void SetView ( HT_View inView ) ;
	virtual void FinishCreateSelf ( ) ;

	virtual void* BackgroundURLProperty ( ) const { return gNavCenter->controlStripBGURL; }
	virtual void* BackgroundColorProperty ( ) const { return gNavCenter->controlStripBGColor; }
	virtual void* ForegroundColorProperty ( ) const { return gNavCenter->controlStripFGColor; } 

	CChameleonCaption* mAddPage;
	CChameleonCaption* mManage;
	CChameleonCaption* mClose;
	
}; // class CNavCenterCommandStrip
