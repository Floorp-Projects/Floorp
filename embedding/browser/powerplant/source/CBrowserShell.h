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
#include <LDragAndDrop.h>

#include "CHeaderSniffer.h"

#include "nsCOMPtr.h"
#include "nsAString.h"
#include "nsIWebBrowser.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsIEventSink.h"
#include "nsIWebProgress.h"
#include "nsIDragHelperService.h"
#include "nsIPrintSettings.h"

class CBrowserChrome;
class CBrowserShellProgressListener;

class nsIContentViewer;
class nsIClipboardCommands;
class nsIDOMEvent;
class nsIDOMNode;
class nsIContextMenuInfo;

//*****************************************************************************
//***    CBrowserShell
//*****************************************************************************

class CBrowserShell : public LView,
                      public LCommander,
                      public LBroadcaster,
                      public LPeriodical,
                      public LDropArea
{
    friend class CBrowserChrome;
      
private:
    typedef LView Inherited;

public:
    enum { class_ID = FOUR_CHAR_CODE('BroS') };
    enum { paneID_MainBrowser = 'WebS' };
    static const nsCString      kEmptyCString;

                                CBrowserShell();
                                CBrowserShell(const SPaneInfo   &inPaneInfo,
                                              const SViewInfo   &inViewInfo,
                                              UInt32            inChromeFlags,
                                              Boolean           inIsMainContent);
                                CBrowserShell(LStream*  inStream);

    virtual                     ~CBrowserShell();
    

    // LPane
    virtual void                FinishCreateSelf();
    virtual void                ResizeFrameBy(SInt16        inWidthDelta,
                                              SInt16        inHeightDelta,
                                              Boolean       inRefresh);
    virtual void                MoveBy(SInt32   inHorizDelta,
                                       SInt32   inVertDelta,
                                       Boolean  inRefresh);
                                       
    virtual void                ActivateSelf();
    virtual void                DeactivateSelf();
    virtual void                ShowSelf();
    virtual void                DrawSelf(); 
    virtual void                ClickSelf(const SMouseDownEvent &inMouseDown);
    virtual void                EventMouseUp(const EventRecord  &inMacEvent);
    
    virtual void                AdjustMouseSelf(Point               /* inPortPt */,
                                                const EventRecord&  inMacEvent,
                                                RgnHandle           outMouseRgn);

    // LCommander
    virtual void                BeTarget();
    virtual void                DontBeTarget();
    virtual Boolean             HandleKeyPress(const EventRecord    &inKeyEvent);
    virtual Boolean             ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam);
    virtual void                FindCommandStatus(PP_PowerPlant::CommandT inCommand,
                                                  Boolean &outEnabled, Boolean &outUsesMark,
                                                  UInt16 &outMark, Str255 outName);

    // LPeriodical
    virtual void                SpendTime(const EventRecord&        inMacEvent);
    
        // Text Input Event Handling
    virtual OSStatus HandleUpdateActiveInputArea(const nsAString& text, 
                                                 PRInt16 script,  PRInt16 language, 
                                                 PRInt32 fixLen, const TextRangeArray * hiliteRng);
    virtual OSStatus HandleUnicodeForKeyEvent(const nsAString& text, 
                                              PRInt16 script, PRInt16 language, 
                                              const EventRecord* keyboardEvent);
    virtual OSStatus HandleOffsetToPos(PRInt32 offset, PRInt16 *pointX, PRInt16 *pointY);
    virtual OSStatus HandlePosToOffset(PRInt16 currentPointX, PRInt16 currentPointY, 
                                       PRInt32 *offset, PRInt16 *regionClass);
    virtual OSStatus HandleGetSelectedText(nsAString& selectedText);
    
    
    // CBrowserShell
        
        // Called by the window creator after parameterized contructor. Not used
        // when we're created from a 'PPob' resource. In that case, attachments can be
        // added with Constructor.
    virtual void            AddAttachments();
    
    NS_METHOD               GetWebBrowser(nsIWebBrowser** aBrowser);
    NS_METHOD               SetWebBrowser(nsIWebBrowser* aBrowser);
                            // Drops ref to current one, installs given one
                            
    NS_METHOD               GetWebBrowserChrome(nsIWebBrowserChrome** aChrome);
                            
    NS_METHOD               GetContentViewer(nsIContentViewer** aViewer);
    
    NS_METHOD               GetPrintSettings(nsIPrintSettings** aSettings);

    NS_METHOD               GetFocusedWindowURL(nsAString& outURL);
    
    Boolean                 IsBusy();
    Boolean                 CanGoBack();
    Boolean                 CanGoForward();

    NS_METHOD               Back();
    NS_METHOD               Forward();
    NS_METHOD               Stop();
    NS_METHOD               Reload();
    
        // String params are UTF-8 encoded.                        
    NS_METHOD               LoadURL(const nsACString& urlText, const nsACString& referrer = kEmptyCString);
    NS_METHOD               GetCurrentURL(nsACString& urlText);

        // Puts up a Save As dialog and saves current URI and all images, etc.
    NS_METHOD               SaveDocument(ESaveFormat inSaveFormat = eSaveFormatUnspecified);
        // Puts up a Save As dialog and saves the given URI. Pass null to save the current page.
    NS_METHOD               SaveLink(nsIURI* inURI);
    NS_METHOD               SaveInternal(nsIURI* inURI, nsIDOMDocument* inDocument, const nsAString& inSuggestedFilename, Boolean inBypassCache, ESaveFormat inSaveFormat = eSaveFormatUnspecified);
    
       // Puts up a find dialog and does the find operation                        
    Boolean                 Find();
       // Does the find operation with the given params - no UI
    Boolean                 Find(const nsAString& searchStr,
                                Boolean caseSensitive,
                                Boolean searchBackward,
                                Boolean wrapSearch,
                                Boolean wholeWordOnly);
    Boolean                 CanFindNext();
    Boolean                 FindNext();
                            
protected:

    // LDropArea
    virtual void            InsideDropArea( DragReference inDragRef );
    virtual Boolean         PointInDropArea( Point inGlobalPt) ;
    virtual Boolean         DragIsAcceptable( DragReference inDragRef );
    virtual void            EnterDropArea( DragReference inDragRef, Boolean inDragHasLeftSender);
    virtual void            LeaveDropArea( DragReference inDragRef );
    virtual void            DoDragReceive( DragReference inDragRef );

    NS_IMETHOD              OnShowContextMenu(PRUint32 aContextFlags,
                                              nsIContextMenuInfo *aInfo);
                                              
    NS_IMETHOD              OnShowTooltip(PRInt32 aXCoords,
                                          PRInt32 aYCoords,
                                          const PRUnichar *aTipText);
    NS_IMETHOD              OnHideTooltip();
                                              
                                              
   NS_METHOD                CommonConstruct();
   
   void                     HandleMouseMoved(const EventRecord& inMacEvent);
   void                     AdjustFrame();
   virtual Boolean          DoFindDialog(nsAString& searchText,
                                         PRBool& findBackwards,
                                         PRBool& wrapFind,
                                         PRBool& entireWord,
                                         PRBool& caseSensitive);

   NS_METHOD                GetClipboardHandler(nsIClipboardCommands **aCommand);
   
   Boolean                  HasFormElements();

   virtual void             PostOpenURLEvent(const nsACString& url, const nsACString& referrer);
    
protected:   
    UInt32                          mChromeFlags;
    Boolean                         mIsMainContent;
      
    nsCOMPtr<nsIEventSink>          mEventSink;             // for event dispatch
    nsCOMPtr<nsIWebBrowser>         mWebBrowser;            // The thing we actually create
    nsCOMPtr<nsIBaseWindow>         mWebBrowserAsBaseWin;   // Convenience interface to above 
    nsCOMPtr<nsIWebNavigation>      mWebBrowserAsWebNav;    // Ditto
   
    static nsCOMPtr<nsIDragHelperService> sDragHelper;

    CBrowserChrome                  *mChrome;
    CBrowserShellProgressListener   *mProgressListener;
    
        // These are stored only during OnShowContextMenu so that they can
        // be used by FindCommandStatus and ObeyCommand which get called
        // during OnShowContextMenu.
    PRUint32                        mContextMenuFlags;
    nsIContextMenuInfo             *mContextMenuInfo;
    
    nsCOMPtr<nsIPrintSettings>      mPrintSettings;
};


#endif // __CBrowserShell__
