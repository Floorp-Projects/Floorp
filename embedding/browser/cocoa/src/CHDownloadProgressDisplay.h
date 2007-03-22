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
      a window controller for your progress window. Its methods
      will be called by the underlying C++ downloading classes.
     
  2.  The Obj-C DownloadControllerFactory class.
  
      This class should be subclassed by an embedder, with an
      implementation of 'createDownloadController' that hands back
      a new instance of an NSWindowController that implements the
      <CHDownloadProgressDisplay> protocol.
      
      The underlying C++ classes use this factory to create the
      progress window controller.
  
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
  variable a DownloadControllerFactory (see above), which got passed to it
  via our XPCOM factory for nsIDownload objects. It then uses that DownloadControllerFactory
  to get an instance of the download progress window controller, which then
  shows the download progress window.
  
  Simple, eh?
  
*/

#import <AppKit/AppKit.h>

#include "nsISupports.h"

class CHDownloader;

// a formal protocol for something that implements progress display
// Embedders can make a window controller that conforms to this
// protocol, and reuse nsDownloadListener to get download UI.
@protocol CHDownloadProgressDisplay

- (void)onStartDownload:(BOOL)isFileSave;
- (void)onEndDownload;

- (void)setProgressTo:(long)aCurProgress ofMax:(long)aMaxProgress;

- (void)setDownloadListener:(CHDownloader*)aDownloader;
- (void)setSourceURL:(NSString*)aSourceURL;
- (void)setDestinationPath:(NSString*)aDestPath;

@end

// subclass this, and have your subclass instantiate and return your window
// controller in createDownloadController
@interface DownloadControllerFactory : NSObject
{
}

- (NSWindowController<CHDownloadProgressDisplay> *)createDownloadController;

@end


// Pure virtual base class for a generic downloader, that the progress UI can talk to.
// It implements nsISupports so that it can be refcounted. This class insulates the
// UI code from having to know too much about the nsIDownloadListener.
// It is responsible for creating the download UI, via the DownloadControllerFactory
// that it owns.
class CHDownloader : public nsISupports
{
public:
                  CHDownloader(DownloadControllerFactory* inControllerFactory);
    virtual       ~CHDownloader();

    NS_DECL_ISUPPORTS

    virtual void PauseDownload() = 0;
    virtual void ResumeDownload() = 0;
    virtual void CancelDownload() = 0;
    virtual void DownloadDone() = 0;
    virtual void DetachDownloadDisplay() = 0;		// tell downloader to forget about its display

    virtual void CreateDownloadDisplay();
    
protected:
  
    PRBool      IsFileSave() { return mIsFileSave; }
    void        SetIsFileSave(PRBool inIsFileSave) { mIsFileSave = inIsFileSave; }

protected:

    DownloadControllerFactory*    mControllerFactory;
    id <CHDownloadProgressDisplay>  mDownloadDisplay;   // something that implements the CHDownloadProgressDisplay protocol
    PRBool                        mIsFileSave;        // true if we're doing a save, rather than a download
};

