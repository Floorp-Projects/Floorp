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

#include <LWindow.h>
#include <LListener.h>
#include <LBroadcaster.h>

class CBrowserShell;
class LEditText;
class LStaticText;
class CThrobber;
class LProgressBar;
class LIconControl;


// CBrowserWindow:
// A simple browser window that hooks up a CBrowserShell to a minimal set of controls
// (Back, Forward and Stop buttons + URL field + status bar).


class CBrowserWindow :  public LWindow,
                        public LListener,
                        public LBroadcaster
{
private:
    typedef LWindow Inherited;

public:
    enum { class_ID = FOUR_CHAR_CODE('BroW') };

                                CBrowserWindow();
                                CBrowserWindow(LCommander*      inSuperCommander,
                                               const Rect&      inGlobalBounds,
                                               ConstStringPtr   inTitle,
                                               SInt16           inProcID,
                                               UInt32           inAttributes,
                                               WindowPtr        inBehind);
                                CBrowserWindow(LStream* inStream);

    virtual                     ~CBrowserWindow();

    virtual void                FinishCreateSelf();

    virtual void                ListenToMessage(MessageT        inMessage,
                                                 void*          ioParam);

    virtual Boolean             ObeyCommand(CommandT            inCommand,
                                            void                *ioParam);
                                    
protected:
    CBrowserShell*              mBrowserShell;
    
    LEditText*                  mURLField;
    LStaticText*                mStatusBar;
    CThrobber*                  mThrobber;
    LControl                    *mBackButton, *mForwardButton, *mReloadButton, *mStopButton;
    LProgressBar*               mProgressBar;
    LIconControl*               mLockIcon;
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

// Must be called at initialization time - after NS_InitEmbedding
nsresult InitializeWindowCreator();

