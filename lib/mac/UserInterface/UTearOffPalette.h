/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//  author: atotic
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
#pragma once

/************************************************************************
 Floating palette classes
 
 Implements tearable tool palletes
 
 ### Standard usage:
 
 	Include UTearOffPalette.r in your project for floating window templates
 	
 	Initialization
		call InitUTearOffPalette(your-application)	on startup

	To create an instance of a palette:
		Create a stand-alone PPob that contains the palette as it appears in the floating
		window and one as it appears imbedded in place. These may be the same PPob If you
		want special shading in the palette, do it in its topmost view

		In the window containing the palette:
			a) create CTearOffBar, empty view and of the same size as stand-alone palette.
				CTearOffBar contains ID of the stand-alone and imbedded palettes
			b) in window::FinishCreateSelf, add window as AddListener to CTearOffBar

			c) inside window, handle the palette messages:
			virtual void		ListenToMessage(MessageT inMessage, void *ioParam)
			{
				switch (inMessage)
				{
					case msg_SetTearOffPalette:
						// Link in the palette controls
						LView * newView = (LView*)ioParam;
						if (newView->GetPaneID() == p1_ID)
							fP1 = newView;
						RefreshControls();
						break;

					case msg_RemoveTearOffPalette:
						// Unlink the palette controls
						PaneIDT paneID = (PaneIDT)ioParam;
						if (paneID == p1_ID)
							fP1 = NULL;
						break;
				}
			}

			d) since your window will not always have controls linked up,
			 make sure that you can deal with your control panes being NULL.

			e) If you want to reposition views in response to palette being torn off,
			 subclass CTearOffBar and override ShowPalette and HidePalette methods.
			 You can also check set the fAutoResize flag in CTearOffBar, and view will
			 shrink to 0,0 when palette is torn out. 
			
			f) When frontmost, your window will get broadcasted messages from 
				broadcasters in the palette

			g) The manager can be used as public message relay. It is both an
				LListener and an LBroadcaster. It generates no messages of it's
				own, but passes on every message that it receives.

 ### Design notes:
	 CTearOffBar (FBar) and TearOffWindow (FWindow)
	 do not know anything about each other, communications happens through
	 CTearOffManager (FManager).
	 
	 FBar and its listener (usually the container window), register with FManager.
	 FBar & FWindow broadcast all their messages through the manager, 
	 and manager dispatches them.
 
 	 Since FWindow is always created from a single PPob template, we do not put
 	 any palette customization flags in the window custom template. All flags
 	 are in the palette PPob. This causes some things to be initialized in 
 	 weird ways (fVisibility, fTransparentPane).
 	 
 	 More to come...
 	 
 ************************************************************************/

// We are reusing mail menu commands
const MessageT msg_SetTearOffPalette = 500;   // ioParam is topmost palette view
const MessageT msg_RemoveTearOffPalette = 501;	// ioParam is the view ID of the topmost palette view

#include <LView.h>
#include <LBroadcaster.h>
#include <LListener.h>
#include <LComparator.h>


/************************************************************************
 * Initialization function for the whole group
 ************************************************************************/
void InitUTearOffPalette(LCommander * defaultCommander);

class CTearOffManager;
class CTearOffWindow;
class LArray;

#pragma mark CTearOffBar

/************************************************************************
 * class CTearOffBar
 ************************************************************************/
class CTearOffBar : public LView,
						public LBroadcaster,	// Broadcasts messages to window
						public LListener		// Listens to messages 
{
	friend class CTearOffManager;
	public:
		enum { class_ID = 'DRPL' };
		
// Constructors

							CTearOffBar(LStream* inStream);

		virtual				~CTearOffBar();
		virtual void		FinishCreateSelf();


// Event handling
		
		virtual void		Click( SMouseDownEvent	&inMouseDown );
		virtual void		ClickSelf(const SMouseDownEvent	&inMouseDown);
		virtual	void		AddListener(LListener	*inListener);
		virtual void		ListenToMessage(MessageT inMessage, void *ioParam);
		virtual void		ActivateSelf();
		virtual void		DeactivateSelf();

// Floater interface
		virtual void		HidePalette();	// If you are overriding, beware that HidePalette can be
											// called on an already hiden palette
		virtual void		ShowPalette();	// Also, ShowPalette can be called on a shown palette

		Boolean				IsShown() {return fShown;}
		void				SetShown(Boolean state) {fShown = state;}
		Boolean				IsFloaterOnSide() {return fSideTitle;}
		Boolean				DisablePaletteWindow() {return fDisablePaletteWithWindow;}
	private:
		ResIDT				fFloatingSubpaneID;	// View to be loaded
							// when the floating window is shown
		ResIDT				fInPlaceSubpaneID;	// View to be loaded
							// when the imbedded pallete is shown

		Boolean				fAutoResize; // Set view width/height to 0,0 on hide
										// Resize to original width on show
		SDimension16		fOriginalFrameSize;	// Original view size 
		Boolean				fShown;
		UInt8				fVisibility;	// 0 -- always, 
											// 1 -- if at least 1 window exists, 
											// 2 -- if window is in front
		Str255				fTitle;
		
		Boolean				fSideTitle;	// title bar of the floating window is either
										// horizontal or vertical
	protected:
		PaneIDT				fTransparentPane;	// The pane that gets no clicks,
									// it is the top pane of the palette
		Boolean				fAllowClickDrag;	// If this is true, then the window
												// is created even if the user just
												// clicks and doesn't drag.
		Boolean				fDisablePaletteWithWindow;	// If this is true then the
														// palette is disabled if the
														// front most window does not
														// have the corresponding button
														// bar.
														// If this is false then the
														// palette will always work.
};


#pragma mark CTearOffManager
/************************************************************************
 * class CTearOffManager
 * One-instance class
 ************************************************************************/
class CTearOffManager: public LListener, public LBroadcaster
{
	friend void InitUTearOffPalette(LCommander * defCommander);

private:
							CTearOffManager(LCommander * defaultCommander);

							~CTearOffManager();

	static CTearOffManager *  sDefaultFloatManager;		

public:

	static CTearOffManager * GetDefaultManager() {return sDefaultFloatManager;}

	
	virtual void	ListenToMessage(
							MessageT		inMessage,
							void			*ioParam);
// CTearOffBar interface
	
	// registration
						
	void					RegisterBar(CTearOffBar * inBar);

	void					UnregisterBar(CTearOffBar * inBar);
	
	// messaging
	
	void					RegisterListener(CTearOffBar * inBar, 
											LListener * listener);
	
	void					BarReceivedMessage(CTearOffBar * inBar,
											MessageT inMessage,
											 void *ioParam);

	// dragging
	
	void					DragBar( CTearOffBar * inBar,
								Int16 inH,
								Int16 inV,
								Boolean usePosition);

	// activation
					
	void					BarActivated( CTearOffBar * inBar);
	
	void					BarDeactivated( CTearOffBar * inBar);

// CFloatPalleteWindow interface

	void					UnregisterWindow(CTearOffWindow * inWin);
	
	void					WindowReceivedMessage(CTearOffWindow * inWin,
											MessageT inMessage,
											 void *ioParam);
	void					CloseFloatingWindow(ResIDT inWinID);
	void					OpenFloatingWindow(	ResIDT inWinID,
												Point inTopLeft,
												Boolean inSideways,
												UInt8 visibility,
												Str255 &title);
	Boolean					IsFloatingWindow(ResIDT inWinID);

private:
	
	CTearOffWindow *	AssertBarWindowExists(CTearOffBar * inBar);
	CTearOffWindow *	AssertBarWindowExists(	ResIDT inWinID,
												Boolean inSideways,
												UInt8 visibility,
												Str255 &title);
	
	void					SetBarsView( CTearOffBar * inBar,
										LView * view);

	void					RemoveBarsView( CTearOffBar * inBar, PaneIDT resID);

private:
	LCommander *		fDefaultCommander;	// Default commander for floaters, usually the app
	LArray *			fPaletteBars;		// list of ButtonBarRecords
	LArray *			fPaletteWindows;	// list of FloatWindowRecords

	// Compare bars for search -- should really be templated
	class CPaletteBarComparator	: public LComparator
	{
		public:
							CPaletteBarComparator(){}
			virtual			~CPaletteBarComparator(){}
			Int32			Compare(const void*		inItemOne,
									const void*		inItemTwo,
									Uint32			inSizeOne,
									Uint32			inSizeTwo) const;
			Int32			CompareToKey(const void*			inItem,
								Uint32				inSize,
								const void*			inKey) const
					{return Compare(inItem, inKey, inSize, inSize);}
	};

	// Compare windows for search -- template this?
	class CPaletteWindowComparator	: public LComparator
	{
		public:
							CPaletteWindowComparator(){}
			virtual			~CPaletteWindowComparator(){}
			Int32			Compare(const void*		inItemOne,
									const void*		inItemTwo,
									Uint32			inSizeOne,
									Uint32			inSizeTwo) const;
			Int32			CompareToKey(const void*			inItem,
								Uint32				inSize,
								const void*			inKey) const
							{return Compare(inItem, inKey, inSize, inSize);}
	};

	CPaletteBarComparator 		fBarCompare;
	CPaletteWindowComparator 	fWindowCompare;
	
};
