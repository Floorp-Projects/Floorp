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

#include "UDownloadDisplay.h"
#include "ApplIDs.h"
#include "CIconServicesIcon.h"

// Gecko
#include "nsString.h"
#include "nsILocalFileMac.h"

// Std
#include <algorithm>
using namespace std;

// PowerPlant
#include <LStaticText.h>
#include <LProgressBar.h>

//*****************************************************************************
// CMultiDownloadProgress
//*****************************************************************************
#pragma mark [CMultiDownloadProgress]

bool CMultiDownloadProgress::sRegisteredViewClasses = false;

CMultiDownloadProgress::CMultiDownloadProgress() :
    mWindow(nil)
{
    if (!sRegisteredViewClasses) {
        RegisterClass_(CMultiDownloadProgressWindow);
        RegisterClass_(CDownloadProgressView);
        RegisterClass_(CIconServicesIcon);
        sRegisteredViewClasses = true;
    }
}

CMultiDownloadProgress::~CMultiDownloadProgress()
{
}
    
void CMultiDownloadProgress::AddDownloadItem(CDownload *aDownloadItem)
{
    if (!mWindow) {
        mWindow = static_cast<CMultiDownloadProgressWindow*>
            (LWindow::CreateWindow(wind_DownloadProgress, this));
        ThrowIfNil_(mWindow);
        
        // Add this as a listener to the window so we can know when it's destroyed.
        mWindow->AddListener(this);
    }
    
    // Create the view...
    LView::SetDefaultView(mWindow);
    LCommander::SetDefaultCommander(mWindow);
    LAttachable::SetDefaultAttachable(nil);

    CDownloadProgressView *itemView = static_cast<CDownloadProgressView*>
        (UReanimator::ReadObjects(ResType_PPob, view_DownloadProgressItem));
    ThrowIfNil_(itemView);
    
    // and add it to the window.
    mWindow->AddDownloadView(itemView);
    itemView->SetDownload(aDownloadItem);
}

// This happens in response to the window being closed. Check for active
// downloads and confirm stopping them if there are any. Returning false
// will prevent the window from being closed.
Boolean CMultiDownloadProgress::AllowSubRemoval(LCommander* inSub)
{
    return (!mWindow || (inSub == mWindow && mWindow->ConfirmClose()));
}

// This happens in response to the app being quit. Check for active
// downloads and confirm stopping them if there are any. Returning
// false will prevent the app from quitting.
Boolean CMultiDownloadProgress::AttemptQuitSelf(SInt32 inSaveOption)
{
    return (!mWindow || mWindow->ConfirmClose());
}

void CMultiDownloadProgress::ListenToMessage(MessageT inMessage,
                                             void* ioParam)
{
    if (inMessage == msg_BroadcasterDied &&
        (CMultiDownloadProgressWindow*)((LBroadcaster*)ioParam) == mWindow)
        mWindow = nil;
}


//*****************************************************************************
// CMultiDownloadProgressWindow
//*****************************************************************************
#pragma mark -
#pragma mark [CMultiDownloadProgressWindow]

CMultiDownloadProgressWindow::CMultiDownloadProgressWindow() :
    mDownloadViewCount(0)
{
    StartBroadcasting();
}

CMultiDownloadProgressWindow::CMultiDownloadProgressWindow(LStream* inStream) :
    LWindow(inStream),
    mDownloadViewCount(0)
{
    StartBroadcasting();
}
                            
CMultiDownloadProgressWindow::~CMultiDownloadProgressWindow()
{
}

void CMultiDownloadProgressWindow::AddDownloadView(CDownloadProgressView *aView)
{
    const SInt16 kSeparatorHeight = 3;
    
    SDimension16 currSize;
    GetFrameSize(currSize);
    
    SDimension16 viewSize;
    aView->GetFrameSize(viewSize);
    ResizeWindowTo(currSize.width,  (viewSize.height * (mDownloadViewCount + 1)) - kSeparatorHeight);
    
    aView->PlaceInSuperFrameAt(0, viewSize.height * mDownloadViewCount++, false);
    aView->FinishCreate();
}

void CMultiDownloadProgressWindow::RemoveDownloadView(CDownloadProgressView *aView)
{
    // We can't remove the last view, leaving an empty window frame
    if (mDownloadViewCount <= 1)
        return;
        
    SDimension16 removedPaneSize;
    aView->GetFrameSize(removedPaneSize);
    SPoint32 removedPaneLoc;
    aView->GetFrameLocation(removedPaneLoc);
    
    delete aView;
    RemoveSubPane(aView);
    mDownloadViewCount--;

    TArrayIterator<LPane*>  iterator(GetSubPanes());
    LPane   *subPane;
    while (iterator.Next(subPane)) {
        SPoint32 subPaneLoc;
        subPane->GetFrameLocation(subPaneLoc);
        if (subPaneLoc.v >= removedPaneLoc.v + removedPaneSize.height) {
            subPane->MoveBy(0, -removedPaneSize.height, true);
        }
    }
    ResizeWindowBy(0, -removedPaneSize.height);
}

Boolean CMultiDownloadProgressWindow::ConfirmClose()
{
    Boolean canClose = true;
    SInt32 numActiveDownloads = 0;

    TArrayIterator<LPane*>  iterator(GetSubPanes());
    LPane   *subPane;
    while (iterator.Next(subPane)) {
        CDownloadProgressView *downloadView = dynamic_cast<CDownloadProgressView*>(subPane);
        if (downloadView && downloadView->IsActive())
            numActiveDownloads++;    
    }
    
    if (numActiveDownloads != 0) {
        short itemHit;
        AlertStdAlertParamRec pb;

        pb.movable = false;
        pb.helpButton = false;
        pb.filterProc = nil;
        pb.defaultText = (StringPtr) kAlertDefaultOKText;
        pb.cancelText = (StringPtr) kAlertDefaultCancelText;
        pb.otherText = nil;
        pb.defaultButton = kStdOkItemIndex;
        pb.cancelButton = kStdCancelItemIndex;
        pb.position = kWindowAlertPositionParentWindowScreen;

        LStr255 msgString(STRx_StdAlertStrings, str_ConfirmCloseDownloads);
        LStr255 explainString(STRx_StdAlertStrings, str_ConfirmCloseDownloadsExp);
        ::StandardAlert(kAlertStopAlert, msgString, explainString, &pb, &itemHit);
        if (itemHit != kAlertStdAlertOKButton)
            canClose = false;
    }
    return canClose;    
}


//*****************************************************************************
// CDownloadProgressView
//*****************************************************************************
#pragma mark -
#pragma mark [CDownloadProgressView]

enum {
    paneID_ProgressBar      = 'Prog',
    paneID_CancelButton     = 'Cncl',
    paneID_OpenButton       = 'Open',
    paneID_RevealButton     = 'Rvel',
    paneID_CloseButton      = 'Clos',
    paneID_StatusText       = 'Stat',
    paneID_StatusLabel      = 'StLa',
    paneID_TimeRemText      = 'Time',
    paneID_TimeRemLabel     = 'TiLa',
    paneID_SrcURIText       = 'SURI',
    paneID_SrcURILabel      = 'SULa',
    paneID_DestFileText     = 'Dest',
    paneID_DestFileLabel    = 'DFLa'
};

CDownloadProgressView::CDownloadProgressView() :
    mDownloadActive(false)
{
}

CDownloadProgressView::CDownloadProgressView(LStream* inStream) :
    LView(inStream),
    mDownloadActive(false)
{
}
                            
CDownloadProgressView::~CDownloadProgressView()
{
    if (mDownloadActive)
        CancelDownload();
        
    mDownload = nsnull;
}

void CDownloadProgressView::FinishCreateSelf()
{
    mProgressBar = dynamic_cast<LProgressBar*>(FindPaneByID(paneID_ProgressBar));
    mCancelButton = dynamic_cast<LControl*>(FindPaneByID(paneID_CancelButton));
    mOpenButton = dynamic_cast<LControl*>(FindPaneByID(paneID_OpenButton));
    mRevealButton = dynamic_cast<LControl*>(FindPaneByID(paneID_RevealButton));
    mCloseButton = dynamic_cast<LControl*>(FindPaneByID(paneID_CloseButton));
    mStatusText = dynamic_cast<LStaticText*>(FindPaneByID(paneID_StatusText));
    mTimeRemainingText = dynamic_cast<LStaticText*>(FindPaneByID(paneID_TimeRemText));
    mSrcURIText = dynamic_cast<LStaticText*>(FindPaneByID(paneID_SrcURIText));
    mDestFileText = dynamic_cast<LStaticText*>(FindPaneByID(paneID_DestFileText));

    // The control value is set in terms of percent
    if (mProgressBar)
        mProgressBar->SetMaxValue(100);
        
    ControlFontStyleRec styleRec;
    
    if (CreateStyleRecFromThemeFont(kThemeSmallSystemFont, styleRec) == noErr) {
        if (mStatusText)
            mStatusText->SetFontStyle(styleRec);
        if (mTimeRemainingText)
            mTimeRemainingText->SetFontStyle(styleRec);
        if (mSrcURIText)
            mSrcURIText->SetFontStyle(styleRec);
        if (mDestFileText)
            mDestFileText->SetFontStyle(styleRec);    
    }
    if (CreateStyleRecFromThemeFont(kThemeSmallEmphasizedSystemFont, styleRec) == noErr) {
        
        ResIDT labelIDs [] = { paneID_StatusLabel,
                               paneID_TimeRemLabel,
                               paneID_SrcURILabel,
                               paneID_DestFileLabel };
                                 
        for (size_t i = 0; i < sizeof(labelIDs) / sizeof(labelIDs[0]); i++) {
            LStaticText *staticText = dynamic_cast<LStaticText*>(FindPaneByID(labelIDs[i]));
            if (staticText)
                staticText->SetFontStyle(styleRec);
        }
    }
   
    UReanimator::LinkListenerToControls(this, this, view_DownloadProgressItem);
    StartListening();
}


Boolean CDownloadProgressView::ObeyCommand(CommandT inCommand,
                                           void *ioParam)
{
#pragma unused(ioParam)

    Boolean cmdHandled = false;
    nsCOMPtr<nsILocalFile> targetFile;
    
    switch (inCommand)
    {            
        case paneID_CancelButton:
        {
            CancelDownload();
            cmdHandled = true;
        }
        break;
        
        case paneID_OpenButton:
        {
            mDownload->GetTarget(getter_AddRefs(targetFile));
            if (targetFile)
                targetFile->Launch();
        }
        break;
        
        case paneID_RevealButton:
        {
            mDownload->GetTarget(getter_AddRefs(targetFile));
            if (targetFile)
                targetFile->Reveal();
        }
        break;
        
        case paneID_CloseButton:
        {
            LView *view = this, *superView;
            while ((superView = view->GetSuperView()) != nil)
                view = superView;
            CMultiDownloadProgressWindow *multiWindow = dynamic_cast<CMultiDownloadProgressWindow*>(view);
            if (multiWindow)
                multiWindow->RemoveDownloadView(this);
        }
        break;

    }

    return cmdHandled;
}

void CDownloadProgressView::ListenToMessage(MessageT inMessage,
                                            void* ioParam)
{
    switch (inMessage) {
    
        case CDownload::msg_OnDLStart:
        {
            mDownloadActive = true;
            if (mCancelButton)
                mCancelButton->Enable();
        }
        break;

        case CDownload::msg_OnDLComplete:
        {
            mDownloadActive = false;
            if (mCancelButton)
                mCancelButton->Disable();
            if (mCloseButton)
                mCloseButton->Enable();

            CDownload *download = reinterpret_cast<CDownload*>(ioParam);                    
            nsresult downloadStatus;
            download->GetStatus(downloadStatus);
            
            // When saving documents as plain text, we might not get any
            // progress change notifications in which to set the progress bar.
            // Now that the download is done, put the bar in a reasonable state.
            if (mProgressBar) {
                mProgressBar->SetIndeterminateFlag(false, false);
                if (NS_SUCCEEDED(downloadStatus))
                    mProgressBar->SetValue(mProgressBar->GetMaxValue());
                else
                    mProgressBar->SetValue(0);
            }
            
            if (NS_SUCCEEDED(downloadStatus)) {
                if (mOpenButton)
                    mOpenButton->Enable();
            }
            if (mRevealButton)
                mRevealButton->Enable();
            if (mTimeRemainingText)
                mTimeRemainingText->SetText(LStr255("\p"));
        }
        break;

        case CDownload::msg_OnDLProgressChange:
        {
            CDownload::MsgOnDLProgressChangeInfo *info =
                static_cast<CDownload::MsgOnDLProgressChangeInfo*>(ioParam);
                            
            if (mProgressBar) {
                PRInt32 percentComplete;
                info->mBroadcaster->GetPercentComplete(&percentComplete);
                if (percentComplete != -1 && mProgressBar->IsIndeterminate())
                    mProgressBar->SetIndeterminateFlag(false, false);
                else if (percentComplete == -1 && !mProgressBar->IsIndeterminate())
                    mProgressBar->SetIndeterminateFlag(true, true);
                
                if (!mProgressBar->IsIndeterminate()) {
                    PRInt32 controlVal = min(100, max(0, percentComplete));
                    mProgressBar->SetValue(controlVal);
                }
            }
            
            // Set the progress bar as often as we're called. Smooth movement is nice.
            // Limit the frequency at which the textual status is updated though.
            if (info->mCurProgress == info->mMaxProgress ||
                ::TickCount() - mLastStatusUpdateTicks >= kStatusUpdateIntervalTicks) {
                UpdateStatus(info);
                mLastStatusUpdateTicks = ::TickCount();
            }
        }
        break;
                  
        default:
          ProcessCommand(inMessage, ioParam);
          break;
    }
}

void CDownloadProgressView::SetDownload(CDownload *aDownload)
{
    mDownload = aDownload;
    if (!mDownload)
        return;
    
    mDownloadActive = true;
    mLastStatusUpdateTicks = ::TickCount();    
    aDownload->AddListener(this);
    
    nsresult rv;
    nsCAutoString tempStr;
    
    if (mSrcURIText) {
        nsCOMPtr<nsIURI> srcURI;
        aDownload->GetSource(getter_AddRefs(srcURI));
        if (srcURI) {
            rv = srcURI->GetSpec(tempStr);
            if (NS_SUCCEEDED(rv))
                mSrcURIText->SetText(const_cast<Ptr>(PromiseFlatCString(tempStr).get()), tempStr.Length());    
        }
    }
    
    if (mDestFileText) {
        nsCOMPtr<nsILocalFile> destFile;
        aDownload->GetTarget(getter_AddRefs(destFile));
        if (destFile) {
            rv = destFile->GetNativePath(tempStr);
            if (NS_SUCCEEDED(rv))
                mDestFileText->SetText(const_cast<Ptr>(PromiseFlatCString(tempStr).get()), tempStr.Length());    
        }
    }
    
    // At this point, make sure our window is showing.
    LWindow::FetchWindowObject(GetMacWindow())->Show();
}

void CDownloadProgressView::CancelDownload()
{
    CDownload *download = dynamic_cast<CDownload*>(mDownload.get());
    download->Cancel();
}

Boolean CDownloadProgressView::IsActive()
{
    return mDownloadActive;
}

void CDownloadProgressView::UpdateStatus(CDownload::MsgOnDLProgressChangeInfo *info)
{    
    PRInt64 startTime;
    mDownload->GetStartTime(&startTime);
    PRInt32 elapsedSecs = (PR_Now() - startTime) / PR_USEC_PER_SEC;
    float bytesPerSec = info->mCurProgress / elapsedSecs;

    UInt8 startPos;
    LStr255 valueStr;
    
    if (mStatusText) {
        // "@1 of @2 (at @3/sec)"
        LStr255 formatStr(STRx_DownloadStatus, str_ProgressFormat);
        
        // Insert each item into the string individually in order to
        // allow certain elements to be omitted from the format.
        if ((startPos = formatStr.Find("\p@1")) != 0)
            formatStr.Replace(startPos, 2, FormatBytes(info->mCurProgress, valueStr));
        if ((startPos = formatStr.Find("\p@2")) != 0)
            formatStr.Replace(startPos, 2, FormatBytes(info->mMaxProgress, valueStr));
        if ((startPos = formatStr.Find("\p@3")) != 0)
            formatStr.Replace(startPos, 2, FormatBytes(bytesPerSec, valueStr));
            
        mStatusText->SetText(formatStr);
    }
    
    if (mTimeRemainingText) {
        PRInt32 secsRemaining = (PRInt32)(float(info->mMaxProgress - info->mCurProgress) / bytesPerSec + 0.5);
        mTimeRemainingText->SetText(FormatFuzzyTime(secsRemaining, valueStr));
    }
}

LStr255& CDownloadProgressView::FormatBytes(float inBytes, LStr255& ioString)
{
    const float kOneThousand24 = 1024.0;
    char buf[256];
    
    if (inBytes < 0) {
        return (ioString = "???");
    }
    if (inBytes < kOneThousand24) {
        sprintf(buf, "%.1f Bytes", inBytes);
        return (ioString = buf);
    }
    inBytes /= kOneThousand24;
    if (inBytes < 1024) {
        sprintf(buf, "%.1f KB", inBytes);
        return (ioString = buf);
    }
    inBytes /= kOneThousand24;
    if (inBytes < 1024) {
        sprintf(buf, "%.1f MB", inBytes);
        return (ioString = buf);
    }
    inBytes /= kOneThousand24;
    sprintf(buf, "%.2f GB", inBytes);
    return (ioString = buf);
}

LStr255& CDownloadProgressView::FormatFuzzyTime(PRInt32 inSecs, LStr255& ioString)
{
    char valueBuf[32];

    if (inSecs < 90) {
        if (inSecs < 7)
            ioString.Assign(STRx_DownloadStatus, str_About5Seconds);
        else if (inSecs < 13)
            ioString.Assign(STRx_DownloadStatus, str_About10Seconds);
        else if (inSecs < 60)
            ioString.Assign(STRx_DownloadStatus, str_LessThan1Minute);
        else
            ioString.Assign(STRx_DownloadStatus, str_About1Minute);
        return ioString;
    }
    inSecs = (inSecs + 30) / 60; // Round up so we don't say "About 1 minutes"
    if (inSecs < 60) {
        sprintf(valueBuf, "%d", inSecs);
        ioString.Assign(STRx_DownloadStatus, str_AboutNMinutes);
        ioString.Replace(ioString.Find("\p@1"), 2, LStr255(valueBuf));
        return ioString;
    }
    inSecs /= 60;
    if (inSecs == 1)
        ioString.Assign(STRx_DownloadStatus, str_About1Hour);
    else {
        sprintf(valueBuf, "%d", inSecs);
        ioString.Assign(STRx_DownloadStatus, str_AboutNHours);
        ioString.Replace(ioString.Find("\p@1"), 2, LStr255(valueBuf));
    }
    return ioString;
}

OSErr CDownloadProgressView::CreateStyleRecFromThemeFont(ThemeFontID inThemeID,
                                                         ControlFontStyleRec& outStyle)
{
    Str255  themeFontName;
    SInt16  themeFontSize;
    Style   themeStyle;
    SInt16  themeFontNum;

    OSErr err = ::GetThemeFont(kThemeSmallSystemFont,
                               smSystemScript,
                               themeFontName,
                               &themeFontSize,
                               &themeStyle);
    if (err != noErr)
        return err;
            
    outStyle.flags	= kControlUseFontMask +
                      kControlUseFaceMask +
                      kControlUseSizeMask;
    
    ::GetFNum(themeFontName, &themeFontNum);
    outStyle.font	= themeFontNum;
    outStyle.size	= themeFontSize;
    outStyle.style	= themeStyle;

    return noErr;
}

