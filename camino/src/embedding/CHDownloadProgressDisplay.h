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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 *   Calum Robinson <calumr@mac.com>
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

/*
  The classes and protocol in this file allow Cocoa applications to easily
  reuse the underlying download implementation, which deals with the complexity
  of Gecko's downloading callbacks.
  
  There are three things here:
  
  1.  The CHDownloadProgressDisplay protocol.
  
      This is a formal protocol that needs to be implemented by
      an object (eg. a window controller or a view controller) 
      for your progress window. Its methods will be called by the
      underlying C++ downloading classes.
     
  2.  The CHDownloadDisplayFactory protocol
  
      This is a formal protocol that can be implemented by
      any object that can create objects that themselves
      implement CHDownloadProgressDisplay. This would probably be 
      an NSWindowController. 
            
  3.  The CHDownloader C++ class
  
      This base class exists to hide the complextity of the download
      listener classes (which deal with Gecko callbacks) from the
      window controller.  Embedders don't need to do anything with it.

  How these classes fit together:
  
  There are 2 ways in which a download is initiated:
  
  (i)   File->Save.
  
        Chimera does a complex dance here in order to get certain
        information about the data being downloaded (it needs to
        start the download before it can read some optional MIME headers).
        
        CBrowserView creates an nsIWebBrowserPersist (WBP), and then a
        nsHeaderSniffer, which implements nsIWebProgressListener and is set to
        observer the WBP. When nsHeaderSniffer hears about the start of the
        download, it decides on a file name, and what format to save the data
        in. It then cancels the current WPB, makes another one, and does
        a CreateInstance of an nsIDownload (making one of our nsDownloadListener
        -- aka nsDownloder -- objects), and sets that as the nsIWebProgressListener.
        The full download procedes from there.
        
  (ii)  Click to download (e.g. FTP link)
  
        This is simpler. The backend (necko) does the CreateInstance of the
        nsIDownload, and the download progresses.
        
  In both cases, creating the nsDownloadListener and calling its Init() method
  calls nsDownloder::CreateDownloadDisplay(). The nsDownloder has as a member
  variable an object that implements CHDownloadDisplayFactory (see above), which got set
  on it via a call to SetDisplayFactory. It then uses that CHDownloadDisplayFactory
  to create an instance of the download progress display, which then controls
  something (like a view or window) that shows in the UI.
  
  Simple, eh?
  
*/

#import <AppKit/AppKit.h>

#include "nsISupports.h"

class CHDownloader;

// A formal protocol for something that implements progress display.
// Embedders can make a window or view controller that conforms to this
// protocol, and reuse nsDownloadListener to get download UI.
@protocol CHDownloadProgressDisplay <NSObject>

- (void)onStartDownload:(BOOL)isFileSave;
- (void)onEndDownload:(BOOL)completedOK statusCode:(nsresult)aStatus;

- (void)setProgressTo:(long long)aCurProgress ofMax:(long long)aMaxProgress;

- (void)setDownloadListener:(CHDownloader*)aDownloader;
- (void)setSourceURL:(NSString*)aSourceURL;
- (NSString*)sourceURL;

- (void)setDestinationPath:(NSString*)aDestPath;
- (NSString*)destinationPath;

- (void)displayWillBeRemoved;

@end

// A formal protocol which is implemented by a factory of progress UI.
@protocol CHDownloadDisplayFactory <NSObject>

- (id <CHDownloadProgressDisplay>)createProgressDisplay;

@end


// Pure virtual base class for a generic downloader, that the progress UI can talk to.
// It implements nsISupports so that it can be refcounted. This class insulates the
// UI code from having to know too much about the nsDownloadListener.
// It is responsible for creating the download UI, via the CHDownloadController
// that it owns.
class CHDownloader : public nsISupports
{
public:
                  CHDownloader();
    virtual       ~CHDownloader();

    NS_DECL_ISUPPORTS

    virtual void SetDisplayFactory(id<CHDownloadDisplayFactory> inDownloadDisplayFactory);    // retains

    virtual void PauseDownload() = 0;
    virtual void ResumeDownload() = 0;
    virtual void CancelDownload() = 0;
    virtual void DownloadDone(nsresult aStatus) = 0;
    virtual void DetachDownloadDisplay() = 0;   // tell downloader to forget about its display
    virtual void CreateDownloadDisplay();
    virtual PRBool IsDownloadPaused() = 0;
    
protected:
  
    PRBool      IsFileSave() { return mIsFileSave; }
    void        SetIsFileSave(PRBool inIsFileSave) { mIsFileSave = inIsFileSave; }

protected:

    id<CHDownloadDisplayFactory>    mDisplayFactory;
    id<CHDownloadProgressDisplay>   mDownloadDisplay;   // something that implements the CHDownloadProgressDisplay protocol
    PRBool                          mIsFileSave;        // true if we're doing a save, rather than a download
};

