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

#include <nsCOMPtr.h>

#include <nsIMIMEService.h>
#include <nsIStreamListener.h>
#include <nsIOutputStream.h>
#include <nsIInputStream.h>
#include <nsIFileStream.h>
#include <nsIChannel.h>
#include <nsIStyleSheet.h>
#include <nsIDocumentEncoder.h>

#include <nsHashtable.h>

#include "nsIWebBrowserPersist.h"
#include "nsDOMWalker.h"

class nsEncoderNodeFixup;

class nsWebBrowserPersist : public nsIWebBrowserPersist,
                            public nsIWebBrowserPersistProgress,
                            public nsIStreamListener,
                            public nsDOMWalkerCallback
{
    friend nsEncoderNodeFixup;

// Public members
public:
    nsWebBrowserPersist();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERPERSIST
    NS_DECL_NSIWEBBROWSERPERSISTPROGRESS
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER
    
// Protected members
protected:    
    virtual ~nsWebBrowserPersist();
    nsresult CloneNodeWithFixedUpURIAttributes(nsIDOMNode *aNodeIn, nsIDOMNode **aNodeOut);

// Private members
private:
    void CleanUp();
    nsresult MakeAndStoreLocalFilenameInURIMap(const char *aURI, nsString &aFilename, PRBool aNeedsPersisting);
    nsresult MakeFilenameFromURI(nsIURI *aURI, nsIChannel *aChannel, nsString &aFilename);
    nsresult StoreURIAttribute(nsIDOMNode *aNode, char *aAttribute, PRBool aNeedsPersisting = PR_TRUE, nsString *aFilename = nsnull);
    nsresult FixupNodeAttribute(nsIDOMNode *aNode, char *aAttribute);
    nsresult StoreAndFixupStyleSheet(nsIStyleSheet *aStyleSheet);
    nsresult SaveDocumentToFileWithFixup(
        nsIDocument    *pDocument,
        nsIDocumentEncoderNodeFixup *pFixup,
        nsFileSpec*     aFileSpec,
        PRBool          aReplaceExisting,
        PRBool          aSaveCopy,
        const nsString& aFormatType,
        const nsString& aSaveCharset,
        PRUint32        aFlags);
    nsresult SaveSubframeContent(nsIDOMDocument *aFrameContent, const nsString &aFilename);

    void OnBeginDownload();
    void OnEndDownload();

    // nsDOMWalkerCallback method
    nsresult OnWalkDOMNode(nsIDOMNode *aNode, PRBool *aAbort);

    // Hash table enumerators
    static PRBool PR_CALLBACK PersistURIs(nsHashKey *aKey, void *aData, void* closure);
    static PRBool PR_CALLBACK CleanupURIMap(nsHashKey *aKey, void *aData, void* closure);

    nsCOMPtr<nsIMIMEService>  mMIMEService;
    nsCOMPtr<nsIChannel>      mInputChannel;
    nsCOMPtr<nsIInputStream>  mInputStream;
    nsCOMPtr<nsIChannel>      mOutputChannel;
    nsCOMPtr<nsIOutputStream> mOutputStream;
    nsCOMPtr<nsIURI>          mBaseURI;
    nsCOMPtr<nsIURI>          mURI;
    nsCOMPtr<nsIWebBrowserPersistProgress> mProgressListener;
    PRUint32                  mFileCounter;
    PRUint32                  mFrameCounter;
    PRUint32                  mTaskCounter;
    nsCString                 mDataPath;
    nsHashtable               mURIMap;
    PRBool                    mFirstAndOnlyUse;
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
