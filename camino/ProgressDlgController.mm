/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import "ProgressDlgController.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIWebBrowserPersist.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsILocalFile.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIWebProgressListener.h"
#include "nsIComponentManager.h"
#include "nsIPref.h"


class nsDownloadListener : public nsIWebProgressListener
{
public:
    nsDownloadListener(ProgressDlgController* aController,
                       nsIWebBrowserPersist* aPersist,
                       nsISupports*     aSource,
                       NSString*        aDestination,
                       const char*      aContentType,
                       nsIInputStream*  aPostData,
                       BOOL             aBypassCache)
    {
        NS_INIT_ISUPPORTS();
        mController = aController;
        mWebPersist = aPersist;
        // The source is either a simple URL or a complete document.
        mURL = do_QueryInterface(aSource);
        if (!mURL)
            mDocument = do_QueryInterface(aSource);
        
        PRUint32 dstLen = [aDestination length];
        PRUnichar* tmp = new PRUnichar[dstLen + sizeof(PRUnichar)];
        tmp[dstLen] = (PRUnichar)'\0';
        [aDestination getCharacters:tmp];
        nsAutoString dstStr(tmp);
        delete tmp;
        
        NS_NewLocalFile(dstStr, PR_FALSE, getter_AddRefs(mDestination));
        // XXX check for failure

        mContentType          = aContentType;
        mPostData             = aPostData;
        mBypassCache          = aBypassCache;
        mNetworkTransfer      = PR_FALSE;
        mGotFirstStateChange  = PR_FALSE;
    };
    
    virtual ~nsDownloadListener() {};

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
  
public:
    void BeginDownload();
    void InitDialog();
    void CancelDownload();
    
private: // Member variables
    ProgressDlgController*          mController;        // Controller for our UI.
    nsCOMPtr<nsIWebBrowserPersist>  mWebPersist;        // Our web persist object.
    nsCOMPtr<nsIURL>                mURL;               // The URL of our source file. Null if we're saving a complete document.
    nsCOMPtr<nsILocalFile>          mDestination;       // Our destination URL.
    nsCString                       mContentType;       // Our content type string.
    nsCOMPtr<nsIDOMHTMLDocument>    mDocument;          // A DOM document.  Null if we're only saving a simple URL.
    nsCOMPtr<nsIInputStream>        mPostData;          // For complete documents, this is our post data from session history.
    PRPackedBool                    mBypassCache;       // Whether we should bypass the cache or not.
    PRPackedBool                    mNetworkTransfer;     // true if the first OnStateChange has the NETWORK bit set
    PRPackedBool                    mGotFirstStateChange; // true after we've seen the first OnStateChange
};

NS_IMPL_ISUPPORTS1(nsDownloadListener, nsIWebProgressListener)

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsDownloadListener::OnProgressChange(nsIWebProgress *aWebProgress, 
                                      nsIRequest *aRequest, 
                                      PRInt32 aCurSelfProgress, 
                                      PRInt32 aMaxSelfProgress, 
                                      PRInt32 aCurTotalProgress, 
                                      PRInt32 aMaxTotalProgress)
{
  [mController setProgressBar:aCurTotalProgress maxProg:aMaxTotalProgress];
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsDownloadListener::OnLocationChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           nsIURI *location)
{
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsDownloadListener::OnStatusChange(nsIWebProgress *aWebProgress, 
               nsIRequest *aRequest, 
               nsresult aStatus, 
               const PRUnichar *aMessage)
{
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
nsDownloadListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  return NS_OK;
}

// Implementation of nsIWebProgressListener
/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
nsDownloadListener::OnStateChange(nsIWebProgress *aWebProgress,  nsIRequest *aRequest,  PRUint32 aStateFlags, 
                                    PRUint32 aStatus)
{
  // NSLog(@"State changed: state %u, status %u", aStateFlags, aStatus);  
  if (!mGotFirstStateChange) {
    mNetworkTransfer = ((aStateFlags & STATE_IS_NETWORK) != 0);
    mGotFirstStateChange = PR_TRUE;
  }
  
  // when the entire download finishes, stop the progress timer and clean up
  // the window and controller. We will get this even in the event of a cancel,
  // so this is the only place in the listener where we should kill the download.
  if ((aStateFlags & STATE_STOP) && (!mNetworkTransfer || (aStateFlags & STATE_IS_NETWORK))) {
    [mController killDownloadTimer];
    [mController setDownloadProgress:nil];
  }
  return NS_OK; 
}

void
nsDownloadListener::BeginDownload()
{
    if (mWebPersist) {
        mWebPersist->SetProgressListener(this);
        PRInt32 flags = nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION | 
                        nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;
        if (mBypassCache)
            flags |= nsIWebBrowserPersist::PERSIST_FLAGS_BYPASS_CACHE;
        else
            flags |= nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;
            
        if (mURL)
            mWebPersist->SaveURI(mURL, mPostData, mDestination);
        else {
            PRInt32 encodingFlags = 0;
            nsCOMPtr<nsILocalFile> filesFolder;
            
            if (!mContentType.Equals("text/plain")) {
                // Create a local directory in the same dir as our file.  It
                // will hold our associated files.
                filesFolder = do_CreateInstance("@mozilla.org/file/local;1");
                nsAutoString unicodePath;
                mDestination->GetPath(unicodePath);
                filesFolder->InitWithPath(unicodePath);
                
                nsAutoString leafName;
                filesFolder->GetLeafName(leafName);
                nsAutoString nameMinusExt(leafName);
                PRInt32 index = nameMinusExt.RFind(".");
                if (index >= 0)
                    nameMinusExt.Left(nameMinusExt, index);
                nameMinusExt += NS_LITERAL_STRING(" Files"); // XXXdwh needs to be localizable!
                filesFolder->SetLeafName(nameMinusExt);
                PRBool exists = PR_FALSE;
                filesFolder->Exists(&exists);
                if (!exists)
                    filesFolder->Create(nsILocalFile::DIRECTORY_TYPE, 0755);
            }
            else
                encodingFlags |= nsIWebBrowserPersist::ENCODE_FLAGS_FORMATTED |
                                 nsIWebBrowserPersist::ENCODE_FLAGS_ABSOLUTE_LINKS |
                                 nsIWebBrowserPersist::ENCODE_FLAGS_NOFRAMES_CONTENT;
                                 
            mWebPersist->SaveDocument(mDocument, mDestination, filesFolder, mContentType.get(),
                                      encodingFlags, 80);
        }
    }
    
    InitDialog();
}

void
nsDownloadListener::InitDialog()
{
    if (!mURL && !mDocument)
        return;
        
    if (mWebPersist) {
        if (mURL) {
            nsCAutoString spec;
            mURL->GetSpec(spec);
            nsAutoString spec2; spec2.AssignWithConversion(spec.get());
            [mController setSourceURL: spec2.get()];
        }
        else {
            nsAutoString spec;
            mDocument->GetURL(spec);
            [mController setSourceURL: spec.get()];
        }
    }

    nsAutoString pathStr;
    mDestination->GetPath(pathStr);
    [mController setDestination: pathStr.get()];
    [mController setDownloadTimer];
}

void
nsDownloadListener::CancelDownload()
{
  if (mWebPersist)
    mWebPersist->CancelSave();
}


static NSString *SaveFileToolbarIdentifier    		= @"Save File Dialog Toolbar";
static NSString *CancelToolbarItemIdentifier  		= @"Cancel Toolbar Item";
static NSString *PauseResumeToolbarItemIdentifier = @"Pause and Resume Toggle Toolbar Item";
static NSString *ShowFileToolbarItemIdentifier    = @"Show File Toolbar Item";
static NSString *OpenFileToolbarItemIdentifier    = @"Open File Toolbar Item";
static NSString *LeaveOpenToolbarItemIdentifier   = @"Leave Open Toggle Toolbar Item";

@interface ProgressDlgController(Private)
-(void)setupToolbar;
@end

@implementation ProgressDlgController

-(void)setWebPersist:(nsIWebBrowserPersist*)aPersist 
              source:(nsISupports*)aSource
         destination:(NSString*)aDestination
         contentType:(const char*)aContentType
            postData:(nsIInputStream*)aInputStream
         bypassCache:(BOOL)aBypassCache
{
    mDownloadListener = new nsDownloadListener(self, aPersist, aSource,
                                               aDestination, aContentType,
                                               aInputStream, aBypassCache);
    NS_ADDREF(mDownloadListener);
}

-(void) setProgressBar:(long int)curProgress maxProg:(long int)maxProgress
{
  aCurrentProgress = curProgress; // fall back for stat calcs
  if (![mProgressBar isIndeterminate]) { //most likely - just update value
    if (curProgress == maxProgress) //handles little bug in FTP download size
      [mProgressBar setMaxValue:maxProgress];
    [mProgressBar setDoubleValue:curProgress];
  }
  else if (maxProgress > 0) { // ok, we're starting up with good max & cur values
    [mProgressBar setIndeterminate:NO];
    [mProgressBar setMaxValue:maxProgress];
    [mProgressBar setDoubleValue:curProgress];
  } // if neither case was true, it's barber pole city.
}

-(void) setSourceURL: (const PRUnichar*)aSource
{
  [mFromField setStringValue: [NSString stringWithCharacters:aSource length:nsCRT::strlen(aSource)]];
}

-(void) setDestination: (const PRUnichar*)aDestination
{
  [mToField setStringValue: [[NSString stringWithCharacters:aDestination length:nsCRT::strlen(aDestination)] stringByAbbreviatingWithTildeInPath]];
}

- (void)windowDidLoad
{
  mDownloadIsPaused = NO;
  mDownloadIsComplete = NO;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  PRBool save = PR_FALSE;
  prefs->GetBoolPref("browser.download.progressDnldDialog.keepAlive",&save);
  mSaveFileDialogShouldStayOpen = save;
  [self setupToolbar];
  [mProgressBar setUsesThreadedAnimation:YES];      
  [mProgressBar startAnimation:self];
  if (mDownloadListener)
    mDownloadListener->BeginDownload();
}

- (void)setupToolbar
{
    NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:SaveFileToolbarIdentifier] autorelease];

    [toolbar setDisplayMode:NSToolbarDisplayModeDefault];
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:YES];
    [toolbar setDelegate:self];
    [[self window] setToolbar:toolbar];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: CancelToolbarItemIdentifier,
        PauseResumeToolbarItemIdentifier,
        ShowFileToolbarItemIdentifier,
        OpenFileToolbarItemIdentifier,
        LeaveOpenToolbarItemIdentifier,
        NSToolbarCustomizeToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier,
        NSToolbarSpaceItemIdentifier,
        NSToolbarSeparatorItemIdentifier,
        nil];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: CancelToolbarItemIdentifier,
        PauseResumeToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier,
        LeaveOpenToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier,
        ShowFileToolbarItemIdentifier,
        OpenFileToolbarItemIdentifier,
        nil];
}

- (BOOL)validateToolbarItem:(NSToolbarItem *)toolbarItem
{
  if ([toolbarItem action] == @selector(cancel))  // cancel button
    return (!mDownloadIsComplete);
  if ([toolbarItem action] == @selector(pauseAndResumeDownload))  // pause/resume button
    return (NO);  // Hey - it hasn't been hooked up yet. !mDownloadIsComplete when it is.
  if ([toolbarItem action] == @selector(showFile))  // show file
    return (mDownloadIsComplete);
  if ([toolbarItem action] == @selector(openFile))  // open file
    return (mDownloadIsComplete);
  return YES;           // turn it on otherwise.
}
- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar
      itemForItemIdentifier:(NSString *)itemIdent
  willBeInsertedIntoToolbar:(BOOL)willBeInserted
{
    NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdent] autorelease];

    if ( [itemIdent isEqual:CancelToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Cancel"];
        [toolbarItem setPaletteLabel:@"Cancel Download"];
        [toolbarItem setToolTip:@"Cancel this file download"];
        [toolbarItem setImage:[NSImage imageNamed:@"saveCancel"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(cancel)];
    } else if ( [itemIdent isEqual:PauseResumeToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Pause"];
        [toolbarItem setPaletteLabel:@"Pause Download"];
        [toolbarItem setToolTip:@"Pause this FTP file download"];
        [toolbarItem setImage:[NSImage imageNamed:@"savePause"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(pauseAndResumeDownload)];
        if ( willBeInserted ) {
            pauseResumeToggleToolbarItem = toolbarItem; //establish reference
        }
    } else if ( [itemIdent isEqual:ShowFileToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Show File"];
        [toolbarItem setPaletteLabel:@"Show File"];
        [toolbarItem setToolTip:@"Show the saved file in the Finder"];
        [toolbarItem setImage:[NSImage imageNamed:@"saveShowFile"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(showFile)];
    } else if ( [itemIdent isEqual:OpenFileToolbarItemIdentifier] ) {
        [toolbarItem setLabel:@"Open File"];
        [toolbarItem setPaletteLabel:@"Open File"];
        [toolbarItem setToolTip:@"Open the saved file in its default application."];
        [toolbarItem setImage:[NSImage imageNamed:@"saveOpenFile"]];
        [toolbarItem setTarget:self];
        [toolbarItem setAction:@selector(openFile)];
    } else if ( [itemIdent isEqual:LeaveOpenToolbarItemIdentifier] ) {
        if ( mSaveFileDialogShouldStayOpen ) {
            [toolbarItem setLabel:@"Leave Open"];
            [toolbarItem setPaletteLabel:@"Toggle Close Behavior"];
            [toolbarItem setToolTip:@"Window will stay open when download finishes."];
            [toolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenYES"]];
            [toolbarItem setTarget:self];
            [toolbarItem setAction:@selector(toggleLeaveOpen)];
        } else {
            [toolbarItem setLabel:@"Close When Done"];
            [toolbarItem setPaletteLabel:@"Toggle Close Behavior"];
            [toolbarItem setToolTip:@"Window will close automatically when download finishes."];
            [toolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenNO"]];
            [toolbarItem setTarget:self];
            [toolbarItem setAction:@selector(toggleLeaveOpen)];
        }
        if ( willBeInserted ) {
            leaveOpenToggleToolbarItem = toolbarItem; //establish reference
        }
    } else {
        toolbarItem = nil;
    }

    return toolbarItem;
}

-(void)cancel
{
  mDownloadListener->CancelDownload();
  // clean up downloaded file. - do it here on in CancelDownload?
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSString *thePath = [[mToField stringValue] stringByExpandingTildeInPath];
  if ([fileManager isDeletableFileAtPath:thePath])
    // if we delete it, fantastic.  if not, oh well.  better to move to trash instead?
    [fileManager removeFileAtPath:thePath handler:nil];
  
  // we can _not_ set the |mIsDownloadComplete| flag here because the download really
  // isn't done yet. We'll probably continue to process more PLEvents that are already
  // in the queue until we get a STATE_STOP state change. As a result, we just keep
  // going until that comes in (and it will, because we called CancelDownload() above).
  // Ensure that the window goes away when we get there by flipping the 'stay alive'
  // flag. (bug 154913)
  mSaveFileDialogShouldStayOpen = NO;
}

-(void)pauseAndResumeDownload
{
  if ( ! mDownloadIsPaused ) {
    //Do logic to pause download
    mDownloadIsPaused = YES;
    [pauseResumeToggleToolbarItem setLabel:@"Resume"];
    [pauseResumeToggleToolbarItem setPaletteLabel:@"Resume Download"];
    [pauseResumeToggleToolbarItem setToolTip:@"Resume the paused FTP download"];
    [pauseResumeToggleToolbarItem setImage:[NSImage imageNamed:@"saveResume"]];
    [self killDownloadTimer];
  } else {
    //Do logic to resume download
    mDownloadIsPaused = NO;
    [pauseResumeToggleToolbarItem setLabel:@"Pause"];
    [pauseResumeToggleToolbarItem setPaletteLabel:@"Pause Download"];
    [pauseResumeToggleToolbarItem setToolTip:@"Pause this FTP file download"];
    [pauseResumeToggleToolbarItem setImage:[NSImage imageNamed:@"savePause"]];
    [self setDownloadTimer];
  }
}

-(void)showFile
{
  NSString *theFile = [[mToField stringValue] stringByExpandingTildeInPath];
  if ([[NSWorkspace sharedWorkspace] selectFile:theFile
                       inFileViewerRootedAtPath:[theFile stringByDeletingLastPathComponent]])
    return;
  // hmmm.  it didn't work.  that's odd. need localized error messages. for now, just beep.
  NSBeep();
}

-(void)openFile
{
  NSString *theFile = [[mToField stringValue] stringByExpandingTildeInPath];
  if ([[NSWorkspace sharedWorkspace] openFile:theFile])
    return;
  // hmmm.  it didn't work.  that's odd.  need localized error message. for now, just beep.
  NSBeep();
    
}

-(void)toggleLeaveOpen
{
    if ( ! mSaveFileDialogShouldStayOpen ) {
        mSaveFileDialogShouldStayOpen = YES;
        [leaveOpenToggleToolbarItem setLabel:@"Leave Open"];
        [leaveOpenToggleToolbarItem setPaletteLabel:@"Toggle Close Behavior"];
        [leaveOpenToggleToolbarItem setToolTip:@"Window will stay open when download finishes."];
        [leaveOpenToggleToolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenYES"]];
    } else {
        mSaveFileDialogShouldStayOpen = NO;
        [leaveOpenToggleToolbarItem setLabel:@"Close When Done"];
        [leaveOpenToggleToolbarItem setPaletteLabel:@"Toggle Close Behavior"];
        [leaveOpenToggleToolbarItem setToolTip:@"Window will close automatically when download finishes."];
        [leaveOpenToggleToolbarItem setImage:[NSImage imageNamed:@"saveLeaveOpenNO"]];
    }
    
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    prefs->SetBoolPref("browser.download.progressDnldDialog.keepAlive", mSaveFileDialogShouldStayOpen);
}

- (void)windowWillClose:(NSNotification *)notification
{
  [self autorelease];
}

- (BOOL)windowShouldClose:(NSNotification *)notification
{
  [self killDownloadTimer];
  if (!mDownloadIsComplete) { //whoops.  hard cancel.
    [self cancel];
    return NO;  // let setDownloadProgress handle the close.
  }
  return YES; 
}

- (void)dealloc
{
  NS_IF_RELEASE(mDownloadListener);
  [super dealloc];
}

- (void)killDownloadTimer
{
  if (mDownloadTimer) {
    [mDownloadTimer invalidate];
    [mDownloadTimer release];
    mDownloadTimer = nil;
  }    
}
- (void)setDownloadTimer
{
  [self killDownloadTimer];
  mDownloadTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0
                                                     target:self
                                                   selector:@selector(setDownloadProgress:)
                                                   userInfo:nil
                                                    repeats:YES] retain];
}

-(NSString *)formatTime:(int)seconds
{
  NSMutableString *theTime =[[[NSMutableString alloc] initWithCapacity:8] autorelease];
  [theTime setString:@""];
  NSString *padZero = [NSString stringWithString:@"0"];
  //write out new elapsed time
  if (seconds >= 3600){
    [theTime appendFormat:@"%d:",(seconds / 3600)];
    seconds = seconds % 3600;
  }
  NSString *elapsedMin = [NSString stringWithFormat:@"%d:",(seconds / 60)];
  if ([elapsedMin length] == 2)
    [theTime appendString:[padZero stringByAppendingString:elapsedMin]];
  else
    [theTime appendString:elapsedMin];
  seconds = seconds % 60;
  NSString *elapsedSec = [NSString stringWithFormat:@"%d",seconds];
  if ([elapsedSec length] == 2)
    [theTime appendString:elapsedSec];
  else
    [theTime appendString:[padZero stringByAppendingString:elapsedSec]];
  return theTime;
}
// fuzzy time gives back strings like "about 5 seconds"
-(NSString *)formatFuzzyTime:(int)seconds
{
  // check for seconds first
  if (seconds < 60) {
    if (seconds < 7)
      return [[[NSString alloc] initWithFormat:[[NSBundle mainBundle] localizedStringForKey:@"UnderSec" value:@"Under %d seconds" table:@"ProgressDialog"],5] autorelease];
    if (seconds < 13)
      return [[[NSString alloc] initWithFormat:[[NSBundle mainBundle] localizedStringForKey:@"UnderSec" value:@"Under %d seconds" table:@"ProgressDialog"],10] autorelease];
    return [[[NSString alloc] initWithString:[[NSBundle mainBundle] localizedStringForKey:@"UnderMin" value:@"Under a minute" table:@"ProgressDialog"]] autorelease];
  }    
  // seconds becomes minutes and we keep checking.  
  seconds=seconds/60;
  if (seconds < 60) {
    if (seconds < 2)
      return [[[NSString alloc] initWithString:[[NSBundle mainBundle] localizedStringForKey:@"AboutMin" value:@"About a minute" table:@"ProgressDialog"]] autorelease];
    // OK, tell the good people how much time we have left.
    return [[[NSString alloc] initWithFormat:[[NSBundle mainBundle] localizedStringForKey:@"AboutMins" value:@"About %d minutes" table:@"ProgressDialog"],seconds] autorelease];
  }
  //this download will never seemingly never end. now seconds become hours.
  seconds=seconds/60;
  if (seconds < 2)
    return [[[NSString alloc] initWithString:[[NSBundle mainBundle] localizedStringForKey:@"AboutHour" value:@"Over an hour" table:@"ProgressDialog"]] autorelease];
  return [[[NSString alloc] initWithFormat:[[NSBundle mainBundle] localizedStringForKey:@"AboutHours" value:@"Over %d hours" table:@"ProgressDialog"],seconds] autorelease];
}


-(NSString *)formatBytes:(float)bytes
{   // this is simpler than my first try.  I peaked at Omnigroup byte formatting code.
    // if bytes are negative, we return question marks.
  if (bytes < 0)
    return [[[NSString alloc] initWithString:@"???"] autorelease];
  // bytes first.
  if (bytes < 1024)
    return [[[NSString alloc] initWithFormat:@"%.1f bytes",bytes] autorelease];
  // kb
  bytes = bytes/1024;
  if (bytes < 1024)
    return [[[NSString alloc] initWithFormat:@"%.1f KB",bytes] autorelease];
  // mb
  bytes = bytes/1024;
  if (bytes < 1024)
    return [[[NSString alloc] initWithFormat:@"%.1f MB",bytes] autorelease];
  // gb
  bytes = bytes/1024;
  return [[[NSString alloc] initWithFormat:@"%.1f GB",bytes] autorelease];
}

// this handles lots of things.
- (void)setDownloadProgress:(NSTimer *)downloadTimer;
{
  // Ack! we're closing the window with the download still running!
  if (mDownloadIsComplete) {        
    [[self window] performClose:self];  
    return;
  }
  // get the elapsed time
  NSArray *elapsedTimeArray = [[mElapsedTimeLabel stringValue] componentsSeparatedByString:@":"];
  int j = [elapsedTimeArray count];
  int elapsedSec = [[elapsedTimeArray objectAtIndex:(j-1)] intValue] + [[elapsedTimeArray objectAtIndex:(j-2)] intValue]*60;
  if (j==3)  // this download is taking forever.
    elapsedSec += [[elapsedTimeArray objectAtIndex:0] intValue]*3600;
  // update elapsed time
  [mElapsedTimeLabel setStringValue:[self formatTime:(++elapsedSec)]];
  // for status field & time left
  float maxBytes = ([mProgressBar maxValue]);
  float byteSec = aCurrentProgress/elapsedSec;
  // OK - if downloadTimer is nil, we're done - fix maxBytes value for status report.
  if (!downloadTimer)
    maxBytes = aCurrentProgress;
  // update status field 
  NSString *labelString = [[NSBundle mainBundle] localizedStringForKey:@"LabelString"
                                                                 value:@"%@ of %@ total (at %@/sec)"
                                                                 table:@"ProgressDialog"];
  [mStatusLabel setStringValue: [NSString stringWithFormat:labelString, [self formatBytes:aCurrentProgress], [self formatBytes:maxBytes], [self formatBytes:byteSec]]];
  // updating estimated time left field
  // if maxBytes < 0, can't calc time left.
  // if !downloadTimer, download is finished.  either way, make sure time left is 0.
  if ((maxBytes > 0) && (downloadTimer)) {
    int secToGo = (int)ceil((elapsedSec*maxBytes/aCurrentProgress)-elapsedSec);
    [mTimeLeftLabel setStringValue:[self formatFuzzyTime:secToGo]];
  }
  else if (!downloadTimer) {  // download done.  Set remaining time to 0, fix progress bar & cancel button
    mDownloadIsComplete = YES;            // all done. we got a STATE_STOP
    [mTimeLeftLabel setStringValue:@""];
    [self setProgressBar:aCurrentProgress maxProg:aCurrentProgress];
    if (!mSaveFileDialogShouldStayOpen)
      [[self window] performClose:self];  // close window
    else
      [[self window] update];             // redraw window
  }
  else //maxBytes is undetermined.  Set remaining time to question marks.
      [mTimeLeftLabel setStringValue:@"???"];
}

@end
