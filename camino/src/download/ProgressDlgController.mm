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
#include "nsIPrefBranch.h"

const char* prefContractID = "@mozilla.org/preferences-service;1";

class nsDownloadListener : public nsIWebProgressListener
{
public:
    nsDownloadListener(ProgressDlgController* aController,
                       nsIWebBrowserPersist* aPersist,
                       nsISupports* aSource,
                       NSString* aDestination,
                       const char* aContentType,
                       nsIInputStream* aPostData,
                       BOOL aBypassCache)
    {
        NS_INIT_REFCNT();
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
        nsAutoString dstStr ( tmp );
        delete tmp;
        NS_NewLocalFile(dstStr, PR_FALSE, getter_AddRefs(mDestination));
        mContentType = aContentType;
        mPostData = aPostData;
        mBypassCache = aBypassCache;
    };
    
    virtual ~nsDownloadListener() {};

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
  
public:
    void BeginDownload();
    void InitDialog();
    
private: // Member variables
    ProgressDlgController* mController; // Controller for our UI.
    nsCOMPtr<nsIWebBrowserPersist> mWebPersist; // Our web persist object.
    nsCOMPtr<nsIURL> mURL; // The URL of our source file. Null if we're saving a complete document.
    nsCOMPtr<nsILocalFile> mDestination; // Our destination URL.
    nsCString mContentType; // Our content type string.
    nsCOMPtr<nsIDOMHTMLDocument> mDocument; // A DOM document.  Null if we're only saving a simple URL.
    nsCOMPtr<nsIInputStream> mPostData;  // For complete documents, this is our post data from session history.
    PRBool mBypassCache; // Whether we should bypass the cache or not.
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
  NSProgressIndicator *progressBar= [mController progressBar];
  if ([progressBar doubleValue] == 0.0)
    [progressBar setMaxValue:aMaxSelfProgress];
  [progressBar setDoubleValue:aCurSelfProgress];
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

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP 
nsDownloadListener::OnSecurityChange(nsIWebProgress *aWebProgress, 
					              nsIRequest *aRequest, 
                                  PRInt32 state)
{
  return NS_OK;
}

// Implementation of nsIWebProgressListener
/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
nsDownloadListener::OnStateChange(nsIWebProgress *aWebProgress, 
				      nsIRequest *aRequest, 
				      PRInt32 aStateFlags, 
				      PRUint32 aStatus)
{
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
    
    
}

static NSString *SaveFileToolbarIdentifier			= @"Save File Dialog Toolbar";
static NSString *CancelToolbarItemIdentifier		= @"Cancel Toolbar Item";
static NSString *PauseResumeToolbarItemIdentifier	= @"Pause and Resume Toggle Toolbar Item";
static NSString *ShowFileToolbarItemIdentifier		= @"Show File Toolbar Item";
static NSString *OpenFileToolbarItemIdentifier		= @"Open File Toolbar Item";
static NSString *LeaveOpenToolbarItemIdentifier		= @"Leave Open Toggle Toolbar Item";

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
-(id) progressBar
{
    return mProgressBar;
}

-(void) setSourceURL: (const PRUnichar*)aSource
{
    [mFromField setStringValue: [NSString stringWithCharacters:aSource length:nsCRT::strlen(aSource)]];
}

-(void) setDestination: (const PRUnichar*)aDestination
{
    [mToField setStringValue: [NSString stringWithCharacters:aDestination length:nsCRT::strlen(aDestination)]];
}

- (void)windowDidLoad
{
    mDownloadIsPaused = NO; 
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(prefContractID));
    PRBool save = PR_FALSE;
    prefs->GetBoolPref("browser.download.progressDnldDialog.keepAlive", 
                        &save);
    mSaveFileDialogShouldStayOpen = save;

    [self setupToolbar];
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
    NSLog(@"Request to cancel download.");
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
    } else {
        //Do logic to resume download
        mDownloadIsPaused = NO;
        [pauseResumeToggleToolbarItem setLabel:@"Pause"];
        [pauseResumeToggleToolbarItem setPaletteLabel:@"Pause Download"];
        [pauseResumeToggleToolbarItem setToolTip:@"Pause this FTP file download"];
        [pauseResumeToggleToolbarItem setImage:[NSImage imageNamed:@"savePause"]];
    }
}

-(void)showFile
{
    NSLog(@"Request to show file.");
}

-(void)openFile
{
    NSLog(@"Request to open file.");
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
    
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(prefContractID));
    prefs->SetBoolPref("browser.download.progressDnldDialog.keepAlive", mSaveFileDialogShouldStayOpen);
}

- (void)windowWillClose:(NSNotification *)notification
{
    [self autorelease];
}

- (void)dealloc
{
    NS_IF_RELEASE(mDownloadListener);
    [super dealloc];
}

@end
