/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#ifndef __CBrowserShell__
#define __CBrowserShell__

#include <LView.h>
#include <LCommander.h>
#include <LPeriodical.h>
#include <LListener.h>
#include <LString.h>

#include "nsIWebBrowser.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWidget.h"
#include "nsMacMessageSink.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebProgress.h"

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

class CBrowserWindow;
class CURIContentListener;
class CFindComponent;
class nsIClipboardCommands;

//*****************************************************************************
//***    CBrowserShell
//*****************************************************************************

class CBrowserShell :	public LView,
           			    public LCommander,
           				public LPeriodical,
           				public LListener
{
  
private:
	typedef LView Inherited;

public:
	enum { class_ID = FOUR_CHAR_CODE('BroS') };

	enum { cmd_Find = 'Find', cmd_FindNext = 'FNxt' };

                                CBrowserShell();
                                CBrowserShell(LStream*	inStream);

	virtual                     ~CBrowserShell();
	

	// LPane
	virtual void		        FinishCreateSelf();
	virtual void		        ResizeFrameBy(SInt16	inWidthDelta,
                							  SInt16	inHeightDelta,
                							  Boolean	inRefresh);
	virtual void		        MoveBy(SInt32		inHorizDelta,
				                       SInt32		inVertDelta,
								       Boolean      inRefresh);
    virtual void                ShowSelf();
	virtual void		        DrawSelf();	
	virtual void		        ClickSelf(const SMouseDownEvent	&inMouseDown);
	virtual void		        EventMouseUp(const EventRecord	&inMacEvent);
    virtual void                AdjustCursorSelf(Point inPortPt,
                                                 const EventRecord&	inMacEvent);

	// LCommander
	virtual void                BeTarget();
	virtual void		        DontBeTarget();
	virtual Boolean		        HandleKeyPress(const EventRecord	&inKeyEvent);
    virtual Boolean             ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam);
	virtual void                FindCommandStatus(PP_PowerPlant::CommandT inCommand,
            								      Boolean &outEnabled, Boolean &outUsesMark,
            					                  PP_PowerPlant::Char16 &outMark, Str255 outName);

	// LPeriodical
	virtual	void		        SpendTime(const EventRecord&		inMacEvent);
	
	// LListener
	virtual void				ListenToMessage(MessageT			inMessage,
														 void*				ioParam);
	
	// CBrowserShell
	NS_METHOD                   SetTopLevelWindow(nsIWebBrowserChrome * aTopLevelWindow);
	NS_METHOD				    GetWebBrowser(nsIWebBrowser** aBrowser);
	NS_METHOD                   SetWebBrowser(nsIWebBrowser* aBrowser);
	                            // Drops ref to current one, installs given one
	
	Boolean                     CanGoBack();
	Boolean                     CanGoForward();
	Boolean                     IsLoading();
	void                        Back();
	void                        Forward();
	void                        Stop();
	                        
	void                        LoadURL(Ptr urlText, SInt32 urlTextLen);
	void                        LoadURL(const nsString& urlText);
	
	   // Puts up a find dialog and does the find operation                        
	Boolean                     Find();
	   // Does the find operation with the given params - no UI
	Boolean                     Find(const nsString& searchStr,
                                     Boolean caseSensitive,
                                     Boolean searchBackward,
                                     Boolean wrapSearch,
                                     Boolean wholeWordOnly);
	Boolean                     CanFindNext();
	Boolean                     FindNext();
	                        
protected:
   NS_METHOD                    CommonConstruct();
   
    void                        HandleMouseMoved(const EventRecord& inMacEvent);
    void                        AdjustFrame();
    
    NS_METHOD                   EnsureFinder();
    virtual Boolean             GetFindParams(nsString& searchText,
                                              PRBool& caseSensitive,
                                              PRBool& searchBackwards,
                                              PRBool& wrapSearch,
                                              PRBool& entireWord);
                                              
    nsresult                    GetClipboardHandler(nsIClipboardCommands **aCommand);
	
protected:
   static nsMacMessageSink mMessageSink;
   
   LStr255                      mInitURL;
      
   nsCOMPtr<nsIWebBrowser>      mWebBrowser;            // The thing we actually create
   nsCOMPtr<nsIBaseWindow>      mWebBrowserAsBaseWin;   // Convenience interface to above 
   nsCOMPtr<nsIWebNavigation>   mWebBrowserAsWebNav;    // Ditto
   
   CFindComponent*              mFinder;
};


#endif // __CBrowserShell__
