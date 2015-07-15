/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIFile.h"
#include "nsIWebProgressListener2.h"

#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

#include "nsCWebBrowserPersist.h"

class nsEncoderNodeFixup;
class nsIStorageStream;

struct CleanupData;
struct DocData;
struct OutputData;
struct UploadData;
struct URIData;

class nsWebBrowserPersist : public nsIInterfaceRequestor,
                            public nsIWebBrowserPersist,
                            public nsIStreamListener,
                            public nsIProgressEventSink,
                            public nsSupportsWeakReference
{
    friend class nsEncoderNodeFixup;

// Public members
public:
    nsWebBrowserPersist();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICANCELABLE
    NS_DECL_NSIWEBBROWSERPERSIST
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIPROGRESSEVENTSINK

// Protected members
protected:
    virtual ~nsWebBrowserPersist();
    nsresult CloneNodeWithFixedUpAttributes(
        nsIDOMNode *aNodeIn, bool *aSerializeCloneKids, nsIDOMNode **aNodeOut);
    nsresult SaveURIInternal(
        nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer,
        uint32_t aReferrerPolicy, nsIInputStream *aPostData,
        const char *aExtraHeaders, nsIURI *aFile,
        bool aCalcFileExt, bool aIsPrivate);
    nsresult SaveChannelInternal(
        nsIChannel *aChannel, nsIURI *aFile, bool aCalcFileExt);
    nsresult SaveDocumentInternal(
        nsIDOMDocument *aDocument, nsIURI *aFile, nsIURI *aDataPath);
    nsresult SaveDocuments();
    nsresult GetDocEncoderContentType(
        nsIDOMDocument *aDocument, const char16_t *aContentType,
        char16_t **aRealContentType);
    nsresult GetExtensionForContentType(
        const char16_t *aContentType, char16_t **aExt);
    nsresult GetDocumentExtension(nsIDOMDocument *aDocument, char16_t **aExt);

// Private members
private:
    void Cleanup();
    void CleanupLocalFiles();
    nsresult GetValidURIFromObject(nsISupports *aObject, nsIURI **aURI) const;
    nsresult GetLocalFileFromURI(nsIURI *aURI, nsIFile **aLocalFile) const;
    nsresult AppendPathToURI(nsIURI *aURI, const nsAString & aPath) const;
    nsresult MakeAndStoreLocalFilenameInURIMap(
        nsIURI *aURI, bool aNeedsPersisting, URIData **aData);
    nsresult MakeOutputStream(
        nsIURI *aFile, nsIOutputStream **aOutputStream);
    nsresult MakeOutputStreamFromFile(
        nsIFile *aFile, nsIOutputStream **aOutputStream);
    nsresult MakeOutputStreamFromURI(nsIURI *aURI, nsIOutputStream  **aOutStream);
    nsresult CreateChannelFromURI(nsIURI *aURI, nsIChannel **aChannel);
    nsresult StartUpload(nsIStorageStream *aOutStream, nsIURI *aDestinationURI,
        const nsACString &aContentType);
    nsresult StartUpload(nsIInputStream *aInputStream, nsIURI *aDestinationURI,
        const nsACString &aContentType);
    nsresult CalculateAndAppendFileExt(nsIURI *aURI, nsIChannel *aChannel,
        nsIURI *aOriginalURIWithExtension);
    nsresult CalculateUniqueFilename(nsIURI *aURI);
    nsresult MakeFilenameFromURI(
        nsIURI *aURI, nsString &aFilename);
    nsresult StoreURI(
        const char *aURI,
        bool aNeedsPersisting = true,
        URIData **aData = nullptr);
    nsresult StoreURI(
        nsIURI *aURI,
        bool aNeedsPersisting = true,
        URIData **aData = nullptr);
    nsresult StoreURIAttributeNS(
        nsIDOMNode *aNode, const char *aNamespaceURI, const char *aAttribute,
        bool aNeedsPersisting = true,
        URIData **aData = nullptr);
    nsresult StoreURIAttribute(
        nsIDOMNode *aNode, const char *aAttribute,
        bool aNeedsPersisting = true,
        URIData **aData = nullptr)
    {
        return StoreURIAttributeNS(aNode, "", aAttribute, aNeedsPersisting, aData);
    }
    bool DocumentEncoderExists(const char16_t *aContentType);

    nsresult GetNodeToFixup(nsIDOMNode *aNodeIn, nsIDOMNode **aNodeOut);
    nsresult FixupURI(nsAString &aURI);
    nsresult FixupNodeAttributeNS(nsIDOMNode *aNode, const char *aNamespaceURI, const char *aAttribute);
    nsresult FixupNodeAttribute(nsIDOMNode *aNode, const char *aAttribute)
    {
        return FixupNodeAttributeNS(aNode, "", aAttribute);
    }
    nsresult FixupAnchor(nsIDOMNode *aNode);
    nsresult FixupXMLStyleSheetLink(nsIDOMProcessingInstruction *aPI, const nsAString &aHref);
    nsresult GetXMLStyleSheetLink(nsIDOMProcessingInstruction *aPI, nsAString &aHref);

    nsresult StoreAndFixupStyleSheet(nsIStyleSheet *aStyleSheet);
    nsresult SaveDocumentWithFixup(
        nsIDOMDocument *pDocument,
        nsIURI *aFile, bool aReplaceExisting, const nsACString &aFormatType,
        const nsCString &aSaveCharset, uint32_t  aFlags);
    nsresult SaveSubframeContent(
        nsIDOMDocument *aFrameContent, URIData *aData);
    nsresult SendErrorStatusChange(
        bool aIsReadError, nsresult aResult, nsIRequest *aRequest, nsIURI *aURI);
    nsresult OnWalkDOMNode(nsIDOMNode *aNode);

    nsresult FixRedirectedChannelEntry(nsIChannel *aNewChannel);

    void EndDownload(nsresult aResult = NS_OK);
    nsresult SaveGatheredURIs(nsIURI *aFileAsURI);
    bool SerializeNextFile();
    void CalcTotalProgress();

    void SetApplyConversionIfNeeded(nsIChannel *aChannel);

    // Hash table enumerators
    static PLDHashOperator EnumPersistURIs(
        const nsACString &aKey, URIData *aData, void* aClosure);
    static PLDHashOperator EnumCleanupOutputMap(
        nsISupports *aKey, OutputData *aData, void* aClosure);
    static PLDHashOperator EnumCleanupUploadList(
        nsISupports *aKey, UploadData *aData, void* aClosure);
    static PLDHashOperator EnumCalcProgress(
        nsISupports *aKey, OutputData *aData, void* aClosure);
    static PLDHashOperator EnumCalcUploadProgress(
        nsISupports *aKey, UploadData *aData, void* aClosure);
    static PLDHashOperator EnumFixRedirect(
        nsISupports *aKey, OutputData *aData, void* aClosure);
    static PLDHashOperator EnumCountURIsToPersist(
        const nsACString &aKey, URIData *aData, void* aClosure);

    nsCOMPtr<nsIURI>          mCurrentDataPath;
    bool                      mCurrentDataPathIsRelative;
    nsCString                 mCurrentRelativePathToData;
    nsCOMPtr<nsIURI>          mCurrentBaseURI;
    nsCString                 mCurrentCharset;
    nsCOMPtr<nsIURI>          mTargetBaseURI;
    uint32_t                  mCurrentThingsToPersist;

    nsCOMPtr<nsIMIMEService>  mMIMEService;
    nsCOMPtr<nsIURI>          mURI;
    nsCOMPtr<nsIWebProgressListener> mProgressListener;
    /**
     * Progress listener for 64-bit values; this is the same object as
     * mProgressListener, but is a member to avoid having to qi it for each
     * progress notification.
     */
    nsCOMPtr<nsIWebProgressListener2> mProgressListener2;
    nsCOMPtr<nsIProgressEventSink> mEventSink;
    nsClassHashtable<nsISupportsHashKey, OutputData> mOutputMap;
    nsClassHashtable<nsISupportsHashKey, UploadData> mUploadList;
    nsClassHashtable<nsCStringHashKey, URIData> mURIMap;
    nsTArray<DocData*>        mDocList;
    nsTArray<CleanupData*>    mCleanupList;
    nsTArray<nsCString>       mFilenameList;
    bool                      mFirstAndOnlyUse;
    bool                      mCancel;
    bool                      mJustStartedLoading;
    bool                      mCompleted;
    bool                      mStartSaving;
    bool                      mReplaceExisting;
    bool                      mSerializingOutput;
    bool                      mIsPrivate;
    uint32_t                  mPersistFlags;
    nsresult                  mPersistResult;
    int64_t                   mTotalCurrentProgress;
    int64_t                   mTotalMaxProgress;
    int16_t                   mWrapColumn;
    uint32_t                  mEncodingFlags;
    nsString                  mContentType;
};

// Helper class does node fixup during persistence
class nsEncoderNodeFixup : public nsIDocumentEncoderNodeFixup
{
public:
    nsEncoderNodeFixup();

    NS_DECL_ISUPPORTS
    NS_IMETHOD FixupNode(nsIDOMNode *aNode, bool *aSerializeCloneKids, nsIDOMNode **aOutNode) override;

    nsWebBrowserPersist *mWebBrowserPersist;

protected:
    virtual ~nsEncoderNodeFixup();
};

#endif
