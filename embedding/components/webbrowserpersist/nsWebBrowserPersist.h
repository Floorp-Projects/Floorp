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
#include "nsIWebBrowserPersistDocument.h"

#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

#include "nsCWebBrowserPersist.h"

class nsIStorageStream;
class nsIWebBrowserPersistDocument;

class nsWebBrowserPersist final : public nsIInterfaceRequestor,
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

// Private members
private:
    virtual ~nsWebBrowserPersist();
    nsresult SaveURIInternal(
        nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer,
        uint32_t aReferrerPolicy, nsIInputStream *aPostData,
        const char *aExtraHeaders, nsIURI *aFile,
        bool aCalcFileExt, bool aIsPrivate);
    nsresult SaveChannelInternal(
        nsIChannel *aChannel, nsIURI *aFile, bool aCalcFileExt);
    nsresult SaveDocumentInternal(
        nsIWebBrowserPersistDocument *aDocument,
        nsIURI *aFile,
        nsIURI *aDataPath);
    nsresult SaveDocuments();
    void FinishSaveDocumentInternal(nsIURI* aFile, nsIFile* aDataPath);
    nsresult GetExtensionForContentType(
        const char16_t *aContentType, char16_t **aExt);

    struct CleanupData;
    struct DocData;
    struct OutputData;
    struct UploadData;
    struct URIData;
    struct WalkData;
    struct URIFixupData;

    class OnWalk;
    class OnWrite;
    class FlatURIMap;
    friend class OnWalk;
    friend class OnWrite;

    nsresult SaveDocumentDeferred(mozilla::UniquePtr<WalkData>&& aData);
    void Cleanup();
    void CleanupLocalFiles();
    nsresult GetValidURIFromObject(nsISupports *aObject, nsIURI **aURI) const;
    static nsresult GetLocalFileFromURI(nsIURI *aURI, nsIFile **aLocalFile);
    static nsresult AppendPathToURI(nsIURI *aURI, const nsAString & aPath);
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
    bool DocumentEncoderExists(const char *aContentType);

    nsresult SaveSubframeContent(
        nsIWebBrowserPersistDocument *aFrameContent,
        const nsCString& aURISpec,
        URIData *aData);
    nsresult SendErrorStatusChange(
        bool aIsReadError, nsresult aResult, nsIRequest *aRequest, nsIURI *aURI);

    nsresult FixRedirectedChannelEntry(nsIChannel *aNewChannel);

    void EndDownload(nsresult aResult);
    void FinishDownload();
    void SerializeNextFile();
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
    static PLDHashOperator EnumCopyURIsToFlatMap(
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
    nsCOMPtr<nsIWebBrowserPersistURIMap> mFlatURIMap;
    nsTArray<mozilla::UniquePtr<WalkData>> mWalkStack;
    nsTArray<DocData*>        mDocList;
    nsTArray<CleanupData*>    mCleanupList;
    nsTArray<nsCString>       mFilenameList;
    bool                      mFirstAndOnlyUse;
    bool                      mSavingDocument;
    bool                      mCancel;
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

#endif
