/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Adam Lock <adamlock@netscape.com>
 */

#ifndef nsWebBrowserPersist_h__
#define nsWebBrowserPersist_h__

#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIInterfaceRequestor.h"
#include "nsIMIMEService.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIChannel.h"
#include "nsIStyleSheet.h"
#include "nsIDocumentEncoder.h"
#include "nsITransport.h"
#include "nsIProgressEventSink.h"
#include "nsILocalFile.h"
#include "nsIWebProgressListener.h"

#include "nsHashtable.h"
#include "nsVoidArray.h"

#include "nsCWebBrowserPersist.h"
#include "nsDOMWalker.h"

class nsEncoderNodeFixup;
struct URIData;

class nsWebBrowserPersist : public nsIInterfaceRequestor,
                            public nsIWebBrowserPersist,
                            public nsIStreamListener,
                            public nsDOMWalkerCallback,
                            public nsIProgressEventSink,
                            public nsSupportsWeakReference
{
    friend class nsEncoderNodeFixup;

// Public members
public:
    nsWebBrowserPersist();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWEBBROWSERPERSIST
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIPROGRESSEVENTSINK
    
// Protected members
protected:    
    virtual ~nsWebBrowserPersist();
    nsresult CloneNodeWithFixedUpURIAttributes(
        nsIDOMNode *aNodeIn, nsIDOMNode **aNodeOut);
    nsresult SaveURIInternal(
        nsIURI *aURI, nsIInputStream *aPostData, nsILocalFile *aFile,
        PRBool aCalcFileExt);
    nsresult SaveDocumentInternal(nsIDOMDocument *aDocument,
        nsILocalFile *aFile, nsILocalFile *aDataPath);
    nsresult SaveDocuments();

// Private members
private:
    void CleanUp();
    nsresult MakeAndStoreLocalFilenameInURIMap(
        const char *aURI, PRBool aNeedsPersisting, URIData **aData);
    nsresult MakeOutputStream(
        nsILocalFile *aFile, PRBool aCalcFileExt,
        nsIChannel *aChannel, nsIOutputStream **aOutputStream);
    nsresult MakeFilenameFromURI(
        nsIURI *aURI, nsString &aFilename);
    nsresult StoreURIAttribute(
        nsIDOMNode *aNode, char *aAttribute, PRBool aNeedsPersisting = PR_TRUE,
        URIData **aData = nsnull);
    nsresult FixupNodeAttribute(nsIDOMNode *aNode, char *aAttribute);
    nsresult FixupAnchor(nsIDOMNode *aNode);
    nsresult StoreAndFixupStyleSheet(nsIStyleSheet *aStyleSheet);
    nsresult SaveDocumentToFileWithFixup(
        nsIDocument *pDocument, nsIDocumentEncoderNodeFixup *pFixup,
        nsIFile *aFile, PRBool aReplaceExisting, PRBool aSaveCopy,
        const nsString &aFormatType, const nsString &aSaveCharset,
        PRUint32  aFlags);
    nsresult SaveSubframeContent(
        nsIDOMDocument *aFrameContent, URIData *aData);
    nsresult SetDocumentBase(nsIDOMDocument *aDocument, nsIURI *aBaseURI);

    nsresult FixRedirectedChannelEntry(nsIChannel *aNewChannel);

    void EndDownload(nsresult aResult = NS_OK);
    void CalcTotalProgress();

    // nsDOMWalkerCallback method
    nsresult OnWalkDOMNode(nsIDOMNode *aNode, PRBool *aAbort);

    // Hash table enumerators
    static PRBool PR_CALLBACK EnumPersistURIs(
        nsHashKey *aKey, void *aData, void* closure);
    static PRBool PR_CALLBACK EnumCleanupURIMap(
        nsHashKey *aKey, void *aData, void* closure);
    static PRBool PR_CALLBACK EnumCleanupOutputMap(
        nsHashKey *aKey, void *aData, void* closure);
    static PRBool PR_CALLBACK EnumCalcProgress(
        nsHashKey *aKey, void *aData, void* closure);
    static PRBool PR_CALLBACK EnumFixRedirect(
        nsHashKey *aKey, void *aData, void* closure);

    nsCOMPtr<nsILocalFile>    mCurrentDataPath;
    PRBool                    mCurrentDataPathIsRelative;
    nsCString                 mCurrentRelativePathToData;
    nsCOMPtr<nsIURI>          mCurrentBaseURI;

    nsCOMPtr<nsIMIMEService>  mMIMEService;
    nsCOMPtr<nsIURI>          mURI;
    nsCOMPtr<nsIWebProgressListener> mProgressListener;
    nsHashtable               mOutputMap;
    nsHashtable               mURIMap;
    nsVoidArray               mDocList;
    PRUint32                  mFileCounter;
    PRUint32                  mFrameCounter;
    PRBool                    mFirstAndOnlyUse;
    PRBool                    mCancel;
    PRBool                    mJustStartedLoading;
    PRBool                    mCompleted;
    PRUint32                  mPersistFlags;
    PRUint32                  mPersistResult;
    PRInt32                   mTotalCurrentProgress;
    PRInt32                   mTotalMaxProgress;

};

// Helper class does node fixup during persistence
class nsEncoderNodeFixup : public nsIDocumentEncoderNodeFixup
{
public:
    nsEncoderNodeFixup();
    
    NS_DECL_ISUPPORTS
    NS_IMETHOD FixupNode(nsIDOMNode *aNode, nsIDOMNode **aOutNode);
    
    nsWebBrowserPersist *mWebBrowserPersist;

protected:    
    virtual ~nsEncoderNodeFixup();
};

#endif
