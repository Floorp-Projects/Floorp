/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
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

#ifndef UDownloadDisplay_h__
#define UDownloadDisplay_h__
#pragma once

#include "UDownload.h"

#include <LWindow.h>

class CMultiDownloadProgressWindow;
class CDownloadProgressView;

class LProgressBar;
class LStaticText;


//*****************************************************************************
// CMultiDownloadProgress
//
// A concrete instance of ADownloadProgressView which displays each download
// as a subpane of the same window.
//
// NOTE: The implementation uses nsILocalFile::Launch() to "open" a downloaded
// item. If the app which opens it is Stuffit Expander, we will receive an
// ae_ApplicationDied event when Stuffit finishes and terminates.
// PowerPlant will throw an exception for any unhandled Apple Event. To inhibit
// this, add a case for ae_ApplicationDied to your application's HandleAppleEvent().
// The event does not need to be handled, just prevented from reaching
// the default handler.
//*****************************************************************************

class CMultiDownloadProgress : public ADownloadProgressView,
                               public LCommander,
                               public LListener
{
public:
                            CMultiDownloadProgress();
    virtual                 ~CMultiDownloadProgress();

    // ADownloadProgressView
    virtual void            AddDownloadItem(CDownload *aDownloadItem);
    
    // LCommander
    virtual Boolean         AllowSubRemoval(LCommander* inSub);
    virtual Boolean         AttemptQuitSelf(SInt32 inSaveOption);
    
    // LListener
    virtual void            ListenToMessage(MessageT inMessage,
                                            void* ioParam);

protected:
    static bool             sRegisteredViewClasses;

    CMultiDownloadProgressWindow *mWindow;
};


//*****************************************************************************
// CMultiDownloadProgressWindow
//
// A window class which holds an array of progress views.
//*****************************************************************************

class CMultiDownloadProgressWindow : public LWindow,
                                     public LBroadcaster
{
public:
    enum { class_ID = FOUR_CHAR_CODE('MDPW') };

                            CMultiDownloadProgressWindow();
                            CMultiDownloadProgressWindow(LStream* inStream);
                            
    virtual                 ~CMultiDownloadProgressWindow();
                                            
    // CMultiDownloadProgressWindow
    virtual void            AddDownloadView(CDownloadProgressView *aView);
    virtual void            RemoveDownloadView(CDownloadProgressView *aView);
    virtual Boolean         ConfirmClose();    
    
protected:
    SInt32                  mDownloadViewCount;
};


//*****************************************************************************
// CDownloadProgressView
//
// A view class which tracks a single download.
//*****************************************************************************

class CDownloadProgressView : public LView,
                              public LCommander,
                              public LListener
{
public:
    enum { class_ID = FOUR_CHAR_CODE('DPrV') };

                            CDownloadProgressView();
                            CDownloadProgressView(LStream* inStream);
                            
    virtual                 ~CDownloadProgressView();

    // LCommander
    virtual Boolean         ObeyCommand(CommandT inCommand,
                                        void *ioParam);
                                        

    // LPane
    virtual void            FinishCreateSelf();

    // LListener
    virtual void            ListenToMessage(MessageT inMessage,
                                            void* ioParam);

    // CDownloadProgressView
    virtual void            SetDownload(CDownload *aDownload);
    virtual void            CancelDownload();
    virtual Boolean         IsActive();
                                                        
    virtual void            UpdateStatus(CDownload::MsgOnDLProgressChangeInfo *info);
    LStr255&                FormatBytes(float inBytes, LStr255& ioString);
    LStr255&                FormatFuzzyTime(PRInt32 inSecs, LStr255& ioString);

    static OSErr            CreateStyleRecFromThemeFont(ThemeFontID inThemeID,
                                                        ControlFontStyleRec& outStyle);

protected:
    enum { kStatusUpdateIntervalTicks = 60 };

    LProgressBar            *mProgressBar;
    LControl                *mCancelButton, *mOpenButton, *mRevealButton;
    LControl                *mCloseButton;
    LStaticText             *mStatusText, *mTimeRemainingText;
    LStaticText             *mSrcURIText, *mDestFileText;

    nsCOMPtr<nsIDownload>   mDownload;
    Boolean                 mDownloadActive;
    SInt32                  mLastStatusUpdateTicks;
};

#endif // DownloadProgressViews_h__
