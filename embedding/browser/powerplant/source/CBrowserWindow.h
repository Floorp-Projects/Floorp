/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Conrad Carlen <conrad@ingress.com>
 */

#include <LWindow.h>
#include <LListener.h>

#ifndef nsError_h
#include "nsError.h"
#endif

#ifndef nsCom_h__
#include "nsCom.h"
#endif

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsIWidget_h__
#include "nsIWidget.h"
#endif

#include "nsIWebBrowserChrome.h"

class CBrowserShell;
class CWebBrowserChrome;
class LEditText;
class LStaticText;
class CThrobber;
class LBevelButton;
class LProgressBar;
class nsIDocumentLoader;
class nsIURI;
class nsIRequest;
class nsIWebProgress;
class CWebBrowserCMAttachment;
class nsIDOMEvent;
class nsIDOMNode;

// CBrowserWindow:
// A simple browser window that hooks up a CBrowserShell to a minimal set of controls
// (Back, Forward and Stop buttons + URL field + status bar).


class CBrowserWindow :	public LWindow,
						public LListener,
						public LBroadcaster
{
private:
    typedef LWindow Inherited;

    friend class CWebBrowserChrome;	

public:
	enum { class_ID = FOUR_CHAR_CODE('BroW') };

                                CBrowserWindow();
                                CBrowserWindow(LCommander*		inSuperCommander,
                                               const Rect&		inGlobalBounds,
                                               ConstStringPtr	inTitle,
                                               SInt16			inProcID,
                                               UInt32			inAttributes,
                                               WindowPtr		inBehind);
                                CBrowserWindow(LStream*	inStream);

	virtual				        ~CBrowserWindow();

    static CBrowserWindow*      CreateWindow(PRUint32 chromeFlags, PRInt32 width, PRInt32 height);

    virtual void                FinishCreate();
	virtual void                FinishCreateSelf();
	virtual void                ResizeFrameBy(SInt16		inWidthDelta,
             								  SInt16		inHeightDelta,
             								  Boolean	    inRefresh);
    virtual void                ShowSelf();

	virtual void                ListenToMessage(MessageT		inMessage,
    								             void*			ioParam);

	virtual Boolean             ObeyCommand(CommandT			inCommand,
      								        void				*ioParam);
								
    virtual void                FindCommandStatus(PP_PowerPlant::CommandT	inCommand,
                                             	  Boolean					&outEnabled,
                                             	  Boolean					&outUsesMark,
                                             	  PP_PowerPlant::Char16	    &outMark,
                                             	  Str255					outName);

    NS_METHOD                   GetWidget(nsIWidget** aWidget);
    CBrowserShell*              GetBrowserShell() const
                                { return mBrowserShell; }
                        
    void                        SetSizeToContent(Boolean isSizedToContent)
                                { mSizeToContent = isSizedToContent; }
    Boolean                     GetSizeToContent()
                                { return mSizeToContent; }
    NS_METHOD                   SizeToContent();
                        
    NS_METHOD                   Stop();
    Boolean                     IsBusy()
                                { return mBusy; }
                        
protected:

      // Called by both constructors
   NS_METHOD            CommonConstruct();
   
   // -----------------------------------
   // Methods called by CWebBrowserChrome
   // -----------------------------------
   
    NS_METHOD                   SetStatus(const PRUnichar* aStatus);
    NS_METHOD                   SetOverLink(const PRUnichar* aStatus)
                                { return SetStatus(aStatus); }
                        
    NS_METHOD                   SetLocation(const nsString& aLocation);
                        
	NS_METHOD                   OnStatusNetStart(nsIWebProgress *progress, nsIRequest *request,
                                                 PRInt32 progressStateFlags, PRUint32 status);
	NS_METHOD                   OnStatusNetStop(nsIWebProgress *progress, nsIRequest *request,
                                                PRInt32 progressStateFlags, PRUint32 status);

    NS_METHOD                   OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                                 PRInt32 curSelfProgress, PRInt32 maxSelfProgress, 
                                                 PRInt32 curTotalProgress, PRInt32 maxTotalProgress);
                                                 
    NS_METHOD                   SetVisibility(PRBool aVisibility);
    
    NS_METHOD                   OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode);
       
protected:
    nsCOMPtr<nsIWidget>         mWindow;

	CBrowserShell*		        mBrowserShell;
	CWebBrowserChrome*          mBrowserChrome;
	LEditText*			        mURLField;
	LStaticText*		        mStatusBar;
	CThrobber*                  mThrobber;
	LBevelButton			    *mBackButton, *mForwardButton, *mStopButton;
	LProgressBar*               mProgressBar;
	Boolean                     mBusy;
	Boolean                     mInitialLoadComplete, mShowOnInitialLoad;
	Boolean                     mSizeToContent;
	
	PRUint32                    mContextMenuContext;
	nsIDOMNode*                 mContextMenuDOMNode; // weak ref - only kept during call of OnShowContextMenu
};


