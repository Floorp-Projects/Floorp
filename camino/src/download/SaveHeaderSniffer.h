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
 *   David Hyatt  <hyatt@mozilla.org>
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

 
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserPersist.h"
#include "nsIURI.h"
#include "nsILocalFile.h"
#include "nsIInputStream.h"
#include "nsIDOMDocument.h"


// Implementation of a header sniffer class that is used when saving Web pages and images.
// NB. GetInterface() for nsIProgressEventSink is called on this class, if we wanted to implement it.
class nsHeaderSniffer : public nsIInterfaceRequestor,
                        public nsIWebProgressListener
{
public:
    nsHeaderSniffer(nsIWebBrowserPersist* aPersist, nsIFile* aFile, nsIURI* aURL,
                    nsIDOMDocument* aDocument, nsIInputStream* aPostData,
                    const nsAString& aSuggestedFilename, PRBool aBypassCache,
                    NSView* aFilterView);
    virtual ~nsHeaderSniffer();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWEBPROGRESSLISTENER
  
protected:

    nsresult  PerformSave(nsIURI* inOriginalURI);
    nsresult  InitiateDownload(nsISupports* inSourceData, nsString& inFileName, nsIURI* inOriginalURI);

private:

    nsIWebBrowserPersist*     mSniffingPersist; // Weak. It owns us as a listener. Only lives until we get the start state.
    nsCOMPtr<nsIFile>         mTmpFile;
    nsCOMPtr<nsIURI>          mURL;
    nsCOMPtr<nsIDOMDocument>  mDocument;
    nsCOMPtr<nsIInputStream>  mPostData;
    nsString                  mDefaultFilename;
    PRBool                    mBypassCache;
    nsCString                 mContentType;
    nsCString                 mContentDisposition;
    NSView*                   mFilterView;
};


@interface FilterViewController : NSObject
{
  IBOutlet NSPopUpButton* mSaveOptionsPopUpButton;
  IBOutlet NSView*        mFilterView;
}

-(IBAction)setNewSaveOption:(id)sender;

@end

