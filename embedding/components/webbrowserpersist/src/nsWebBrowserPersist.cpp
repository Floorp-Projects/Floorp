/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nspr.h"

#include "nsIFileStreams.h"       // New Necko file streams
#include <algorithm>

#ifdef XP_OS2
#include "nsILocalFileOS2.h"
#endif

#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsIComponentRegistrar.h"
#include "nsIStorageStream.h"
#include "nsISeekableStream.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIEncodedChannel.h"
#include "nsIUploadChannel.h"
#include "nsICachingChannel.h"
#include "nsIFileChannel.h"
#include "nsEscape.h"
#include "nsUnicharUtils.h"
#include "nsIStringEnumerator.h"
#include "nsCRT.h"
#include "nsSupportsArray.h"
#include "nsContentCID.h"
#include "nsStreamUtils.h"

#include "nsCExternalHandlerService.h"

#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMNode.h"
#include "nsIDOMComment.h"
#include "nsIDOMMozNamedAttrMap.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNodeList.h"
#include "nsIWebProgressListener.h"
#include "nsIAuthPrompt.h"
#include "nsIPrompt.h"
#include "nsISHEntry.h"
#include "nsIWebPageDescriptor.h"
#include "nsIFormControl.h"
#include "nsContentUtils.h"

#include "nsIDOMNodeFilter.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsIDOMHTMLMediaElement.h"
 
#include "nsIImageLoadingContent.h"

#include "ftpCore.h"
#include "nsITransport.h"
#include "nsISocketTransport.h"
#include "nsIStringBundle.h"
#include "nsIProtocolHandler.h"

#include "nsWebBrowserPersist.h"

#include "nsIContent.h"
#include "nsIMIMEInfo.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSharedElement.h"
#include "mozilla/dom/HTMLSharedObjectElement.h"

using namespace mozilla;
using namespace mozilla::dom;

// Buffer file writes in 32kb chunks
#define BUFFERED_OUTPUT_SIZE (1024 * 32)

// Information about a DOM document
struct DocData
{
    nsCOMPtr<nsIURI> mBaseURI;
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIURI> mFile;
    nsCOMPtr<nsIURI> mDataPath;
    bool mDataPathIsRelative;
    nsCString mRelativePathToData;
    nsCString mCharset;
};

// Information about a URI
struct URIData
{
    bool mNeedsPersisting;
    bool mSaved;
    bool mIsSubFrame;
    bool mDataPathIsRelative;
    bool mNeedsFixup;
    nsString mFilename;
    nsString mSubFrameExt;
    nsCOMPtr<nsIURI> mFile;
    nsCOMPtr<nsIURI> mDataPath;
    nsCString mRelativePathToData;
    nsCString mCharset;
};

// Information about the output stream
struct OutputData
{
    nsCOMPtr<nsIURI> mFile;
    nsCOMPtr<nsIURI> mOriginalLocation;
    nsCOMPtr<nsIOutputStream> mStream;
    int64_t mSelfProgress;
    int64_t mSelfProgressMax;
    bool mCalcFileExt;

    OutputData(nsIURI *aFile, nsIURI *aOriginalLocation, bool aCalcFileExt) :
        mFile(aFile),
        mOriginalLocation(aOriginalLocation),
        mSelfProgress(0),
        mSelfProgressMax(10000),
        mCalcFileExt(aCalcFileExt)
    {
    }
    ~OutputData()
    {
        if (mStream)
        {
            mStream->Close();
        }
    }
};

struct UploadData
{
    nsCOMPtr<nsIURI> mFile;
    int64_t mSelfProgress;
    int64_t mSelfProgressMax;

    UploadData(nsIURI *aFile) :
        mFile(aFile),
        mSelfProgress(0),
        mSelfProgressMax(10000)
    {
    }
};

struct CleanupData
{
    nsCOMPtr<nsIFile> mFile;
    // Snapshot of what the file actually is at the time of creation so that if
    // it transmutes into something else later on it can be ignored. For example,
    // catch files that turn into dirs or vice versa.
    bool mIsDirectory;
};

// Maximum file length constant. The max file name length is
// volume / server dependent but it is difficult to obtain
// that information. Instead this constant is a reasonable value that
// modern systems should able to cope with.
const uint32_t kDefaultMaxFilenameLength = 64;

// Default flags for persistence
const uint32_t kDefaultPersistFlags = 
    nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION |
    nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;

// String bundle where error messages come from
const char *kWebBrowserPersistStringBundle =
    "chrome://global/locale/nsWebBrowserPersist.properties";

nsWebBrowserPersist::nsWebBrowserPersist() :
    mCurrentThingsToPersist(0),
    mFirstAndOnlyUse(true),
    mCancel(false),
    mJustStartedLoading(true),
    mCompleted(false),
    mStartSaving(false),
    mReplaceExisting(true),
    mSerializingOutput(false),
    mIsPrivate(false),
    mPersistFlags(kDefaultPersistFlags),
    mPersistResult(NS_OK),
    mTotalCurrentProgress(0),
    mTotalMaxProgress(0),
    mWrapColumn(72),
    mEncodingFlags(0)
{
}

nsWebBrowserPersist::~nsWebBrowserPersist()
{
    Cleanup();
}

//*****************************************************************************
// nsWebBrowserPersist::nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsWebBrowserPersist)
NS_IMPL_RELEASE(nsWebBrowserPersist)

NS_INTERFACE_MAP_BEGIN(nsWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY(nsICancelable)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
NS_INTERFACE_MAP_END


//*****************************************************************************
// nsWebBrowserPersist::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP nsWebBrowserPersist::GetInterface(const nsIID & aIID, void **aIFace)
{
    NS_ENSURE_ARG_POINTER(aIFace);

    *aIFace = nullptr;

    nsresult rv = QueryInterface(aIID, aIFace);
    if (NS_SUCCEEDED(rv))
    {
        return rv;
    }
    
    if (mProgressListener && (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) 
                             || aIID.Equals(NS_GET_IID(nsIPrompt))))
    {
        mProgressListener->QueryInterface(aIID, aIFace);
        if (*aIFace)
            return NS_OK;
    }

    nsCOMPtr<nsIInterfaceRequestor> req = do_QueryInterface(mProgressListener);
    if (req)
    {
        return req->GetInterface(aIID, aIFace);
    }

    return NS_ERROR_NO_INTERFACE;
}


//*****************************************************************************
// nsWebBrowserPersist::nsIWebBrowserPersist
//*****************************************************************************

/* attribute unsigned long persistFlags; */
NS_IMETHODIMP nsWebBrowserPersist::GetPersistFlags(uint32_t *aPersistFlags)
{
    NS_ENSURE_ARG_POINTER(aPersistFlags);
    *aPersistFlags = mPersistFlags;
    return NS_OK;
}
NS_IMETHODIMP nsWebBrowserPersist::SetPersistFlags(uint32_t aPersistFlags)
{
    mPersistFlags = aPersistFlags;
    mReplaceExisting = (mPersistFlags & PERSIST_FLAGS_REPLACE_EXISTING_FILES) ? true : false;
    mSerializingOutput = (mPersistFlags & PERSIST_FLAGS_SERIALIZE_OUTPUT) ? true : false;
    return NS_OK;
}

/* readonly attribute unsigned long currentState; */
NS_IMETHODIMP nsWebBrowserPersist::GetCurrentState(uint32_t *aCurrentState)
{
    NS_ENSURE_ARG_POINTER(aCurrentState);
    if (mCompleted)
    {
        *aCurrentState = PERSIST_STATE_FINISHED;
    }
    else if (mFirstAndOnlyUse)
    {
        *aCurrentState = PERSIST_STATE_SAVING;
    }
    else
    {
        *aCurrentState = PERSIST_STATE_READY;
    }
    return NS_OK;
}

/* readonly attribute unsigned long result; */
NS_IMETHODIMP nsWebBrowserPersist::GetResult(nsresult *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mPersistResult;
    return NS_OK;
}

/* attribute nsIWebBrowserPersistProgress progressListener; */
NS_IMETHODIMP nsWebBrowserPersist::GetProgressListener(
    nsIWebProgressListener * *aProgressListener)
{
    NS_ENSURE_ARG_POINTER(aProgressListener);
    *aProgressListener = mProgressListener;
    NS_IF_ADDREF(*aProgressListener);
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserPersist::SetProgressListener(
    nsIWebProgressListener * aProgressListener)
{
    mProgressListener = aProgressListener;
    mProgressListener2 = do_QueryInterface(aProgressListener);
    mEventSink = do_GetInterface(aProgressListener);
    return NS_OK;
}

/* void saveURI (in nsIURI aURI, in nsISupports aCacheKey, in nsIURI aReferrer,
   in nsIInputStream aPostData, in wstring aExtraHeaders,
   in nsISupports aFile, in nsILoadContext aPrivayContext); */
NS_IMETHODIMP nsWebBrowserPersist::SaveURI(
    nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer, nsIInputStream *aPostData, const char *aExtraHeaders, nsISupports *aFile, nsILoadContext* aPrivacyContext)
{
    return SavePrivacyAwareURI(aURI, aCacheKey, aReferrer, aPostData, aExtraHeaders, aFile,
                               aPrivacyContext && aPrivacyContext->UsePrivateBrowsing());
}

NS_IMETHODIMP nsWebBrowserPersist::SavePrivacyAwareURI(
    nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer, nsIInputStream *aPostData, const char *aExtraHeaders, nsISupports *aFile, bool aIsPrivate)
{
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);
    mFirstAndOnlyUse = false; // Stop people from reusing this object!

    nsCOMPtr<nsIURI> fileAsURI;
    nsresult rv;
    rv = GetValidURIFromObject(aFile, getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

    // SaveURI doesn't like broken uris.
    mPersistFlags |= PERSIST_FLAGS_FAIL_ON_BROKEN_LINKS;
    rv = SaveURIInternal(aURI, aCacheKey, aReferrer, aPostData, aExtraHeaders, fileAsURI, false, aIsPrivate);
    return NS_FAILED(rv) ? rv : NS_OK;
}

/* void saveChannel (in nsIChannel aChannel, in nsISupports aFile); */
NS_IMETHODIMP nsWebBrowserPersist::SaveChannel(
    nsIChannel *aChannel, nsISupports *aFile)
{
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);
    mFirstAndOnlyUse = false; // Stop people from reusing this object!

    nsCOMPtr<nsIURI> fileAsURI;
    nsresult rv;
    rv = GetValidURIFromObject(aFile, getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

    rv = aChannel->GetURI(getter_AddRefs(mURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // SaveURI doesn't like broken uris.
    mPersistFlags |= PERSIST_FLAGS_FAIL_ON_BROKEN_LINKS;
    rv = SaveChannelInternal(aChannel, fileAsURI, false);
    return NS_FAILED(rv) ? rv : NS_OK;
}


/* void saveDocument (in nsIDOMDocument aDocument, in nsIURI aFileURI,
   in nsIURI aDataPathURI, in string aOutputContentType,
   in unsigned long aEncodingFlags, in unsigned long aWrapColumn); */
NS_IMETHODIMP nsWebBrowserPersist::SaveDocument(
    nsIDOMDocument *aDocument, nsISupports *aFile, nsISupports *aDataPath,
    const char *aOutputContentType, uint32_t aEncodingFlags, uint32_t aWrapColumn)
{
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);
    mFirstAndOnlyUse = false; // Stop people from reusing this object!

    nsCOMPtr<nsIURI> fileAsURI;
    nsCOMPtr<nsIURI> datapathAsURI;
    nsresult rv;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
    nsCOMPtr<nsILoadContext> privacyContext = doc ? doc->GetLoadContext() : nullptr;
    mIsPrivate = privacyContext && privacyContext->UsePrivateBrowsing();

    rv = GetValidURIFromObject(aFile, getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);
    if (aDataPath)
    {
        rv = GetValidURIFromObject(aDataPath, getter_AddRefs(datapathAsURI));
        NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);
    }

    mWrapColumn = aWrapColumn;

    // Produce nsIDocumentEncoder encoding flags
    mEncodingFlags = 0;
    if (aEncodingFlags & ENCODE_FLAGS_SELECTION_ONLY)
        mEncodingFlags |= nsIDocumentEncoder::OutputSelectionOnly;
    if (aEncodingFlags & ENCODE_FLAGS_FORMATTED)
        mEncodingFlags |= nsIDocumentEncoder::OutputFormatted;
    if (aEncodingFlags & ENCODE_FLAGS_RAW)
        mEncodingFlags |= nsIDocumentEncoder::OutputRaw;
    if (aEncodingFlags & ENCODE_FLAGS_BODY_ONLY)
        mEncodingFlags |= nsIDocumentEncoder::OutputBodyOnly;
    if (aEncodingFlags & ENCODE_FLAGS_PREFORMATTED)
        mEncodingFlags |= nsIDocumentEncoder::OutputPreformatted;
    if (aEncodingFlags & ENCODE_FLAGS_WRAP)
        mEncodingFlags |= nsIDocumentEncoder::OutputWrap;
    if (aEncodingFlags & ENCODE_FLAGS_FORMAT_FLOWED)
        mEncodingFlags |= nsIDocumentEncoder::OutputFormatFlowed;
    if (aEncodingFlags & ENCODE_FLAGS_ABSOLUTE_LINKS)
        mEncodingFlags |= nsIDocumentEncoder::OutputAbsoluteLinks;
    if (aEncodingFlags & ENCODE_FLAGS_ENCODE_BASIC_ENTITIES)
        mEncodingFlags |= nsIDocumentEncoder::OutputEncodeBasicEntities;
    if (aEncodingFlags & ENCODE_FLAGS_ENCODE_LATIN1_ENTITIES)
        mEncodingFlags |= nsIDocumentEncoder::OutputEncodeLatin1Entities;
    if (aEncodingFlags & ENCODE_FLAGS_ENCODE_HTML_ENTITIES)
        mEncodingFlags |= nsIDocumentEncoder::OutputEncodeHTMLEntities;
    if (aEncodingFlags & ENCODE_FLAGS_ENCODE_W3C_ENTITIES)
        mEncodingFlags |= nsIDocumentEncoder::OutputEncodeW3CEntities;
    if (aEncodingFlags & ENCODE_FLAGS_CR_LINEBREAKS)
        mEncodingFlags |= nsIDocumentEncoder::OutputCRLineBreak;
    if (aEncodingFlags & ENCODE_FLAGS_LF_LINEBREAKS)
        mEncodingFlags |= nsIDocumentEncoder::OutputLFLineBreak;
    if (aEncodingFlags & ENCODE_FLAGS_NOSCRIPT_CONTENT)
        mEncodingFlags |= nsIDocumentEncoder::OutputNoScriptContent;
    if (aEncodingFlags & ENCODE_FLAGS_NOFRAMES_CONTENT)
        mEncodingFlags |= nsIDocumentEncoder::OutputNoFramesContent;
    
    if (aOutputContentType)
    {
        mContentType.AssignASCII(aOutputContentType);
    }

    rv = SaveDocumentInternal(aDocument, fileAsURI, datapathAsURI);

    // Now save the URIs that have been gathered

    if (NS_SUCCEEDED(rv) && datapathAsURI)
    {
        rv = SaveGatheredURIs(fileAsURI);
    }
    else if (mProgressListener)
    {
        // tell the listener we're done
        mProgressListener->OnStateChange(nullptr, nullptr,
                                         nsIWebProgressListener::STATE_START |
                                         nsIWebProgressListener::STATE_IS_NETWORK,
                                         NS_OK);
        mProgressListener->OnStateChange(nullptr, nullptr,
                                         nsIWebProgressListener::STATE_STOP |
                                         nsIWebProgressListener::STATE_IS_NETWORK,
                                         rv);
    }

    return rv;
}

/* void cancel(nsresult aReason); */
NS_IMETHODIMP nsWebBrowserPersist::Cancel(nsresult aReason)
{
    mCancel = true;
    EndDownload(aReason);
    return NS_OK;
}


/* void cancelSave(); */
NS_IMETHODIMP nsWebBrowserPersist::CancelSave()
{
    return Cancel(NS_BINDING_ABORTED);
}


nsresult
nsWebBrowserPersist::StartUpload(nsIStorageStream *storStream, 
    nsIURI *aDestinationURI, const nsACString &aContentType)
{
     // setup the upload channel if the destination is not local
    nsCOMPtr<nsIInputStream> inputstream;
    nsresult rv = storStream->NewInputStream(0, getter_AddRefs(inputstream));
    NS_ENSURE_TRUE(inputstream, NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    return StartUpload(inputstream, aDestinationURI, aContentType);
}

nsresult
nsWebBrowserPersist::StartUpload(nsIInputStream *aInputStream,
    nsIURI *aDestinationURI, const nsACString &aContentType)
{
    nsCOMPtr<nsIChannel> destChannel;
    CreateChannelFromURI(aDestinationURI, getter_AddRefs(destChannel));
    nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(destChannel));
    NS_ENSURE_TRUE(uploadChannel, NS_ERROR_FAILURE);

    // Set the upload stream
    // NOTE: ALL data must be available in "inputstream"
    nsresult rv = uploadChannel->SetUploadStream(aInputStream, aContentType, -1);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    rv = destChannel->AsyncOpen(this, nullptr);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // add this to the upload list
    nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(destChannel);
    nsISupportsKey key(keyPtr);
    mUploadList.Put(&key, new UploadData(aDestinationURI));

    return NS_OK;
}

nsresult
nsWebBrowserPersist::SaveGatheredURIs(nsIURI *aFileAsURI)
{
    nsresult rv = NS_OK;

    // Count how many URIs in the URI map require persisting
    uint32_t urisToPersist = 0;
    if (mURIMap.Count() > 0)
    {
        mURIMap.Enumerate(EnumCountURIsToPersist, &urisToPersist);
    }

    if (urisToPersist > 0)
    {
        // Persist each file in the uri map. The document(s)
        // will be saved after the last one of these is saved.
        mURIMap.Enumerate(EnumPersistURIs, this);
    }

    // if we don't have anything in mOutputMap (added from above enumeration)
    // then we build the doc list (SaveDocuments)
    if (mOutputMap.Count() == 0)
    {
        // There are no URIs to save, so just save the document(s)

        // State start notification
        uint32_t addToStateFlags = 0;
        if (mProgressListener)
        {
            if (mJustStartedLoading)
            {
                addToStateFlags |= nsIWebProgressListener::STATE_IS_NETWORK;
            }
            mProgressListener->OnStateChange(nullptr, nullptr,
                nsIWebProgressListener::STATE_START | addToStateFlags, NS_OK);
        }

        rv = SaveDocuments();
        if (NS_FAILED(rv))
            EndDownload(rv);
        else if (aFileAsURI)
        {
            // local files won't trigger OnStopRequest so we call EndDownload here
            bool isFile = false;
            aFileAsURI->SchemeIs("file", &isFile);
            if (isFile)
                EndDownload(NS_OK);
        }

        // State stop notification
        if (mProgressListener)
        {
            mProgressListener->OnStateChange(nullptr, nullptr,
                nsIWebProgressListener::STATE_STOP | addToStateFlags, rv);
        }
    }

    return rv;
}

// this method returns true if there is another file to persist and false if not
bool
nsWebBrowserPersist::SerializeNextFile()
{
    if (!mSerializingOutput)
    {
        return false;
    }

    nsresult rv = SaveGatheredURIs(nullptr);
    if (NS_FAILED(rv))
    {
        return false;
    }

    return (mURIMap.Count() 
        || mUploadList.Count()
        || mDocList.Length()
        || mOutputMap.Count());
}


//*****************************************************************************
// nsWebBrowserPersist::nsIRequestObserver
//*****************************************************************************

NS_IMETHODIMP nsWebBrowserPersist::OnStartRequest(
    nsIRequest* request, nsISupports *ctxt)
{
    if (mProgressListener)
    {
        uint32_t stateFlags = nsIWebProgressListener::STATE_START |
                              nsIWebProgressListener::STATE_IS_REQUEST;
        if (mJustStartedLoading)
        {
            stateFlags |= nsIWebProgressListener::STATE_IS_NETWORK;
        }
        mProgressListener->OnStateChange(nullptr, request, stateFlags, NS_OK);
    }

    mJustStartedLoading = false;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

    nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(request);
    nsISupportsKey key(keyPtr);
    OutputData *data = (OutputData *) mOutputMap.Get(&key);

    // NOTE: This code uses the channel as a hash key so it will not
    //       recognize redirected channels because the key is not the same.
    //       When that happens we remove and add the data entry to use the
    //       new channel as the hash key.
    if (!data)
    {
        UploadData *upData = (UploadData *) mUploadList.Get(&key);
        if (!upData)
        {
            // Redirect? Try and fixup the output table
            nsresult rv = FixRedirectedChannelEntry(channel);
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

            // Should be able to find the data after fixup unless redirects
            // are disabled.
            data = (OutputData *) mOutputMap.Get(&key);
            if (!data)
            {
                return NS_ERROR_FAILURE;
            }
        }
    }

    if (data && data->mFile)
    {
        // If PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION is set in mPersistFlags,
        // try to determine whether this channel needs to apply Content-Encoding
        // conversions.
        NS_ASSERTION(!((mPersistFlags & PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION) &&
                      (mPersistFlags & PERSIST_FLAGS_NO_CONVERSION)),
                     "Conflict in persist flags: both AUTODETECT and NO_CONVERSION set");
        if (mPersistFlags & PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION)
            SetApplyConversionIfNeeded(channel);

        if (data->mCalcFileExt && !(mPersistFlags & PERSIST_FLAGS_DONT_CHANGE_FILENAMES))
        {
            // this is the first point at which the server can tell us the mimetype
            CalculateAndAppendFileExt(data->mFile, channel, data->mOriginalLocation);

            // now make filename conformant and unique
            CalculateUniqueFilename(data->mFile);
        }

        // compare uris and bail before we add to output map if they are equal
        bool isEqual = false;
        if (NS_SUCCEEDED(data->mFile->Equals(data->mOriginalLocation, &isEqual))
            && isEqual)
        {
            // remove from output map
            delete data;
            mOutputMap.Remove(&key);

            // cancel; we don't need to know any more
            // stop request will get called
            request->Cancel(NS_BINDING_ABORTED);
        }
    }

    return NS_OK;
}
 
NS_IMETHODIMP nsWebBrowserPersist::OnStopRequest(
    nsIRequest* request, nsISupports *ctxt, nsresult status)
{
    nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(request);
    nsISupportsKey key(keyPtr);
    OutputData *data = (OutputData *) mOutputMap.Get(&key);
    if (data)
    {
        if (NS_SUCCEEDED(mPersistResult) && NS_FAILED(status))
            SendErrorStatusChange(true, status, request, data->mFile);

#if defined(XP_OS2)
        // delete 'data';  this will close the stream and let
        // us tag the file it created with its source URI
        nsCOMPtr<nsIURI> uriSource = data->mOriginalLocation;
        nsCOMPtr<nsIFile> localFile;
        GetLocalFileFromURI(data->mFile, getter_AddRefs(localFile));
        delete data;
        mOutputMap.Remove(&key);
        if (localFile)
        {
            nsCOMPtr<nsILocalFileOS2> localFileOS2 = do_QueryInterface(localFile);
            if (localFileOS2)
            {
                nsAutoCString url;
                uriSource->GetSpec(url);
                localFileOS2->SetFileSource(url);
            }
        }
#else
        // This will close automatically close the output stream
        delete data;
        mOutputMap.Remove(&key);
#endif
    }
    else
    {
        // if we didn't find the data in mOutputMap, try mUploadList
        UploadData *upData = (UploadData *) mUploadList.Get(&key);
        if (upData)
        {
            delete upData;
            mUploadList.Remove(&key);
        }
    }

    // ensure we call SaveDocuments if we:
    // 1) aren't canceling
    // 2) we haven't triggered the save (which we only want to trigger once)
    // 3) we aren't serializing (which will call it inside SerializeNextFile)
    if (mOutputMap.Count() == 0 && !mCancel && !mStartSaving && !mSerializingOutput)
    {
        nsresult rv = SaveDocuments();
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    }

    bool completed = false;
    if (mOutputMap.Count() == 0 && mUploadList.Count() == 0 && !mCancel)
    {
        // if no documents left in mDocList, --> done
        // if we have no files left to serialize and no error result, --> done
        if (mDocList.Length() == 0
            || (!SerializeNextFile() && NS_SUCCEEDED(mPersistResult)))
        {
            completed = true;
        }
    }

    if (completed)
    {
        // we're all done, do our cleanup
        EndDownload(status);
    }

    if (mProgressListener)
    {
        uint32_t stateFlags = nsIWebProgressListener::STATE_STOP |
                              nsIWebProgressListener::STATE_IS_REQUEST;
        if (completed)
        {
            stateFlags |= nsIWebProgressListener::STATE_IS_NETWORK;
        }
        mProgressListener->OnStateChange(nullptr, request, stateFlags, status);
    }
    if (completed)
    {
        mProgressListener = nullptr;
        mProgressListener2 = nullptr;
        mEventSink = nullptr;
    }

    return NS_OK;
}

//*****************************************************************************
// nsWebBrowserPersist::nsIStreamListener
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowserPersist::OnDataAvailable(
    nsIRequest* request, nsISupports *aContext, nsIInputStream *aIStream,
    uint64_t aOffset, uint32_t aLength)
{
    bool cancel = mCancel;
    if (!cancel)
    {
        nsresult rv = NS_OK;
        uint32_t bytesRemaining = aLength;

        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

        nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(request);
        nsISupportsKey key(keyPtr);
        OutputData *data = (OutputData *) mOutputMap.Get(&key);
        if (!data) {
            // might be uploadData; consume necko's buffer and bail...
            uint32_t n;
            return aIStream->ReadSegments(NS_DiscardSegment, nullptr, aLength, &n);
        }

        bool readError = true;

        // Make the output stream
        if (!data->mStream)
        {
            rv = MakeOutputStream(data->mFile, getter_AddRefs(data->mStream));
            if (NS_FAILED(rv))
            {
                readError = false;
                cancel = true;
            }
        }

        // Read data from the input and write to the output
        char buffer[8192];
        uint32_t bytesRead;
        while (!cancel && bytesRemaining)
        {
            readError = true;
            rv = aIStream->Read(buffer,
                                std::min(uint32_t(sizeof(buffer)), bytesRemaining),
                                &bytesRead);
            if (NS_SUCCEEDED(rv))
            {
                readError = false;
                // Write out the data until something goes wrong, or, it is
                // all written.  We loop because for some errors (e.g., disk
                // full), we get NS_OK with some bytes written, then an error.
                // So, we want to write again in that case to get the actual
                // error code.
                const char *bufPtr = buffer; // Where to write from.
                while (NS_SUCCEEDED(rv) && bytesRead)
                {
                    uint32_t bytesWritten = 0;
                    rv = data->mStream->Write(bufPtr, bytesRead, &bytesWritten);
                    if (NS_SUCCEEDED(rv))
                    {
                        bytesRead -= bytesWritten;
                        bufPtr += bytesWritten;
                        bytesRemaining -= bytesWritten;
                        // Force an error if (for some reason) we get NS_OK but
                        // no bytes written.
                        if (!bytesWritten)
                        {
                            rv = NS_ERROR_FAILURE;
                            cancel = true;
                        }
                    }
                    else
                    {
                        // Disaster - can't write out the bytes - disk full / permission?
                        cancel = true;
                    }
                }
            }
            else
            {
                // Disaster - can't read the bytes - broken link / file error?
                cancel = true;
            }
        }

        int64_t channelContentLength = -1;
        if (!cancel &&
            NS_SUCCEEDED(channel->GetContentLength(&channelContentLength)))
        {
            // if we get -1 at this point, we didn't get content-length header
            // assume that we got all of the data and push what we have; 
            // that's the best we can do now
            if ((-1 == channelContentLength) ||
                ((channelContentLength - (aOffset + aLength)) == 0))
            {
                NS_WARN_IF_FALSE(channelContentLength != -1,
                    "nsWebBrowserPersist::OnDataAvailable() no content length "
                    "header, pushing what we have");
                // we're done with this pass; see if we need to do upload
                nsAutoCString contentType;
                channel->GetContentType(contentType);
                // if we don't have the right type of output stream then it's a local file
                nsCOMPtr<nsIStorageStream> storStream(do_QueryInterface(data->mStream));
                if (storStream)
                {
                    data->mStream->Close();
                    data->mStream = nullptr; // null out stream so we don't close it later
                    rv = StartUpload(storStream, data->mFile, contentType);
                    if (NS_FAILED(rv))
                    {
                        readError = false;
                        cancel = true;
                    }
                }
            }
        }

        // Notify listener if an error occurred.
        if (cancel)
        {
            SendErrorStatusChange(readError, rv,
                readError ? request : nullptr, data->mFile);
        }
    }

    // Cancel reading?
    if (cancel)
    {
        EndDownload(NS_BINDING_ABORTED);
    }

    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist::nsIProgressEventSink
//*****************************************************************************

/* void onProgress (in nsIRequest request, in nsISupports ctxt,
    in unsigned long long aProgress, in unsigned long long aProgressMax); */
NS_IMETHODIMP nsWebBrowserPersist::OnProgress(
    nsIRequest *request, nsISupports *ctxt, uint64_t aProgress,
    uint64_t aProgressMax)
{
    if (!mProgressListener)
    {
        return NS_OK;
    }

    // Store the progress of this request
    nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(request);
    nsISupportsKey key(keyPtr);
    OutputData *data = (OutputData *) mOutputMap.Get(&key);
    if (data)
    {
        data->mSelfProgress = int64_t(aProgress);
        data->mSelfProgressMax = int64_t(aProgressMax);
    }
    else
    {
        UploadData *upData = (UploadData *) mUploadList.Get(&key);
        if (upData)
        {
            upData->mSelfProgress = int64_t(aProgress);
            upData->mSelfProgressMax = int64_t(aProgressMax);
        }
    }

    // Notify listener of total progress
    CalcTotalProgress();
    if (mProgressListener2)
    {
      mProgressListener2->OnProgressChange64(nullptr, request, aProgress,
            aProgressMax, mTotalCurrentProgress, mTotalMaxProgress);
    }
    else
    {
      // have to truncate 64-bit to 32bit
      mProgressListener->OnProgressChange(nullptr, request, uint64_t(aProgress),
              uint64_t(aProgressMax), mTotalCurrentProgress, mTotalMaxProgress);
    }

    // If our progress listener implements nsIProgressEventSink,
    // forward the notification
    if (mEventSink)
    {
        mEventSink->OnProgress(request, ctxt, aProgress, aProgressMax);
    }

    return NS_OK;
}

/* void onStatus (in nsIRequest request, in nsISupports ctxt,
    in nsresult status, in wstring statusArg); */
NS_IMETHODIMP nsWebBrowserPersist::OnStatus(
    nsIRequest *request, nsISupports *ctxt, nsresult status,
    const PRUnichar *statusArg)
{
    if (mProgressListener)
    {
        // We need to filter out non-error error codes.
        // Is the only NS_SUCCEEDED value NS_OK?
        switch ( status )
        {
        case NS_NET_STATUS_RESOLVING_HOST:
        case NS_NET_STATUS_RESOLVED_HOST:
        case NS_NET_STATUS_BEGIN_FTP_TRANSACTION:
        case NS_NET_STATUS_END_FTP_TRANSACTION:
        case NS_NET_STATUS_CONNECTING_TO:
        case NS_NET_STATUS_CONNECTED_TO:
        case NS_NET_STATUS_SENDING_TO:
        case NS_NET_STATUS_RECEIVING_FROM:
        case NS_NET_STATUS_WAITING_FOR:
        case NS_NET_STATUS_READING:
        case NS_NET_STATUS_WRITING:
            break;

        default:
            // Pass other notifications (for legitimate errors) along.
            mProgressListener->OnStatusChange(nullptr, request, status, statusArg);
            break;
        }

    }

    // If our progress listener implements nsIProgressEventSink,
    // forward the notification
    if (mEventSink)
    {
        mEventSink->OnStatus(request, ctxt, status, statusArg);
    }

    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist private methods
//*****************************************************************************

// Convert error info into proper message text and send OnStatusChange notification
// to the web progress listener.
nsresult nsWebBrowserPersist::SendErrorStatusChange( 
    bool aIsReadError, nsresult aResult, nsIRequest *aRequest, nsIURI *aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);

    if (!mProgressListener)
    {
        // Do nothing
        return NS_OK;
    }

    // Get the file path or spec from the supplied URI
    nsCOMPtr<nsIFile> file;
    GetLocalFileFromURI(aURI, getter_AddRefs(file));
    nsAutoString path;
    if (file)
    {
        file->GetPath(path);
    }
    else
    {
        nsAutoCString fileurl;
        aURI->GetSpec(fileurl);
        AppendUTF8toUTF16(fileurl, path);
    }
    
    nsAutoString msgId;
    switch(aResult)
    {
    case NS_ERROR_FILE_NAME_TOO_LONG:
        // File name too long.
        msgId.AssignLiteral("fileNameTooLongError");
        break;
    case NS_ERROR_FILE_ALREADY_EXISTS:
        // File exists with same name as directory.
        msgId.AssignLiteral("fileAlreadyExistsError");
        break;
    case NS_ERROR_FILE_DISK_FULL:
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
        // Out of space on target volume.
        msgId.AssignLiteral("diskFull");
        break;

    case NS_ERROR_FILE_READ_ONLY:
        // Attempt to write to read/only file.
        msgId.AssignLiteral("readOnly");
        break;

    case NS_ERROR_FILE_ACCESS_DENIED:
        // Attempt to write without sufficient permissions.
        msgId.AssignLiteral("accessError");
        break;

    default:
        // Generic read/write error message.
        if (aIsReadError)
            msgId.AssignLiteral("readError");
        else
            msgId.AssignLiteral("writeError");
        break;
    }
    // Get properties file bundle and extract status string.
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> s = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && s, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundle> bundle;
    rv = s->CreateBundle(kWebBrowserPersistStringBundle, getter_AddRefs(bundle));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && bundle, NS_ERROR_FAILURE);
    
    nsXPIDLString msgText;
    const PRUnichar *strings[1];
    strings[0] = path.get();
    rv = bundle->FormatStringFromName(msgId.get(), strings, 1, getter_Copies(msgText));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    mProgressListener->OnStatusChange(nullptr, aRequest, aResult, msgText);

    return NS_OK;
}

nsresult nsWebBrowserPersist::GetValidURIFromObject(nsISupports *aObject, nsIURI **aURI) const
{
    NS_ENSURE_ARG_POINTER(aObject);
    NS_ENSURE_ARG_POINTER(aURI);
    
    nsCOMPtr<nsIFile> objAsFile = do_QueryInterface(aObject);
    if (objAsFile)
    {
        return NS_NewFileURI(aURI, objAsFile);
    }
    nsCOMPtr<nsIURI> objAsURI = do_QueryInterface(aObject);
    if (objAsURI)
    {
        *aURI = objAsURI;
        NS_ADDREF(*aURI);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

nsresult nsWebBrowserPersist::GetLocalFileFromURI(nsIURI *aURI, nsIFile **aLocalFile) const
{
    nsresult rv;

    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIFile> file;
    rv = fileURL->GetFile(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
        rv = CallQueryInterface(file, aLocalFile);

    return rv;
}

nsresult nsWebBrowserPersist::AppendPathToURI(nsIURI *aURI, const nsAString & aPath) const
{
    NS_ENSURE_ARG_POINTER(aURI);

    nsAutoCString newPath;
    nsresult rv = aURI->GetPath(newPath);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Append a forward slash if necessary
    int32_t len = newPath.Length();
    if (len > 0 && newPath.CharAt(len - 1) != '/')
    {
        newPath.Append('/');
    }

    // Store the path back on the URI
    AppendUTF16toUTF8(aPath, newPath);
    aURI->SetPath(newPath);

    return NS_OK;
}

nsresult nsWebBrowserPersist::SaveURIInternal(
    nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer,
    nsIInputStream *aPostData, const char *aExtraHeaders,
    nsIURI *aFile, bool aCalcFileExt, bool aIsPrivate)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aFile);

    nsresult rv = NS_OK;
    
    mURI = aURI;

    nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
    if (mPersistFlags & PERSIST_FLAGS_BYPASS_CACHE)
    {
        loadFlags |= nsIRequest::LOAD_BYPASS_CACHE;
    }
    else if (mPersistFlags & PERSIST_FLAGS_FROM_CACHE)
    {
        loadFlags |= nsIRequest::LOAD_FROM_CACHE;
    }

    // Extract the cache key
    nsCOMPtr<nsISupports> cacheKey;
    if (aCacheKey)
    {
        // Test if the cache key is actually a web page descriptor (docshell)
        // or session history entry.
        nsCOMPtr<nsISHEntry> shEntry = do_QueryInterface(aCacheKey);
        if (!shEntry)
        {
            nsCOMPtr<nsIWebPageDescriptor> webPageDescriptor =
                do_QueryInterface(aCacheKey);
            if (webPageDescriptor)
            {
                nsCOMPtr<nsISupports> currentDescriptor;
                webPageDescriptor->GetCurrentDescriptor(getter_AddRefs(currentDescriptor));
                shEntry = do_QueryInterface(currentDescriptor);
            }
        }

        if (shEntry)
        {
            shEntry->GetCacheKey(getter_AddRefs(cacheKey));
        }
        else
        {
            // Assume a plain cache key
            cacheKey = aCacheKey;
        }
    }

    // Open a channel to the URI
    nsCOMPtr<nsIChannel> inputChannel;
    rv = NS_NewChannel(getter_AddRefs(inputChannel), aURI,
            nullptr, nullptr, static_cast<nsIInterfaceRequestor *>(this),
            loadFlags);

    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(inputChannel);
    if (pbChannel)
    {
        pbChannel->SetPrivate(aIsPrivate);
    }
    
    if (NS_FAILED(rv) || inputChannel == nullptr)
    {
        EndDownload(NS_ERROR_FAILURE);
        return NS_ERROR_FAILURE;
    }
    
    // Disable content conversion
    if (mPersistFlags & PERSIST_FLAGS_NO_CONVERSION)
    {
        nsCOMPtr<nsIEncodedChannel> encodedChannel(do_QueryInterface(inputChannel));
        if (encodedChannel)
        {
            encodedChannel->SetApplyConversion(false);
        }
    }

    if (mPersistFlags & PERSIST_FLAGS_FORCE_ALLOW_COOKIES) 
    {
        nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
                do_QueryInterface(inputChannel);
        if (httpChannelInternal)
            httpChannelInternal->SetForceAllowThirdPartyCookie(true);
    }

    // Set the referrer, post data and headers if any
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(inputChannel));
    if (httpChannel)
    {
        // Referrer
        if (aReferrer)
        {
            httpChannel->SetReferrer(aReferrer);
        }

        // Post data
        if (aPostData)
        {
            nsCOMPtr<nsISeekableStream> stream(do_QueryInterface(aPostData));
            if (stream)
            {
                // Rewind the postdata stream
                stream->Seek(nsISeekableStream::NS_SEEK_SET, 0);
                nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
                NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");
                // Attach the postdata to the http channel
                uploadChannel->SetUploadStream(aPostData, EmptyCString(), -1);
            }
        }

        // Cache key
        nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(httpChannel));
        if (cacheChannel && cacheKey)
        {
            cacheChannel->SetCacheKey(cacheKey);
        }

        // Headers
        if (aExtraHeaders)
        {
            nsAutoCString oneHeader;
            nsAutoCString headerName;
            nsAutoCString headerValue;
            int32_t crlf = 0;
            int32_t colon = 0;
            const char *kWhitespace = "\b\t\r\n ";
            nsAutoCString extraHeaders(aExtraHeaders);
            while (true)
            {
                crlf = extraHeaders.Find("\r\n", true);
                if (crlf == -1)
                    break;
                extraHeaders.Mid(oneHeader, 0, crlf);
                extraHeaders.Cut(0, crlf + 2);
                colon = oneHeader.Find(":");
                if (colon == -1)
                    break; // Should have a colon
                oneHeader.Left(headerName, colon);
                colon++;
                oneHeader.Mid(headerValue, colon, oneHeader.Length() - colon);
                headerName.Trim(kWhitespace);
                headerValue.Trim(kWhitespace);
                // Add the header (merging if required)
                rv = httpChannel->SetRequestHeader(headerName, headerValue, true);
                if (NS_FAILED(rv))
                {
                    EndDownload(NS_ERROR_FAILURE);
                    return NS_ERROR_FAILURE;
                }
            }
        }
    }
    return SaveChannelInternal(inputChannel, aFile, aCalcFileExt);
}

nsresult nsWebBrowserPersist::SaveChannelInternal(
    nsIChannel *aChannel, nsIURI *aFile, bool aCalcFileExt)
{
    NS_ENSURE_ARG_POINTER(aChannel);
    NS_ENSURE_ARG_POINTER(aFile);

    // The default behaviour of SaveChannelInternal is to download the source
    // into a storage stream and upload that to the target. MakeOutputStream
    // special-cases a file target and creates a file output stream directly.
    // We want to special-case a file source and create a file input stream,
    // but we don't need to do this in the case of a file target.
    nsCOMPtr<nsIFileChannel> fc(do_QueryInterface(aChannel));
    nsCOMPtr<nsIFileURL> fu(do_QueryInterface(aFile));
    if (fc && !fu) {
        nsCOMPtr<nsIInputStream> fileInputStream, bufferedInputStream;
        nsresult rv = aChannel->Open(getter_AddRefs(fileInputStream));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedInputStream),
                                       fileInputStream, BUFFERED_OUTPUT_SIZE);
        NS_ENSURE_SUCCESS(rv, rv);
        nsAutoCString contentType;
        aChannel->GetContentType(contentType);
        return StartUpload(bufferedInputStream, aFile, contentType);
    }

    // Read from the input channel
    nsresult rv = aChannel->AsyncOpen(this, nullptr);
    if (rv == NS_ERROR_NO_CONTENT)
    {
        // Assume this is a protocol such as mailto: which does not feed out
        // data and just ignore it.
        return NS_SUCCESS_DONT_FIXUP;
    }

    if (NS_FAILED(rv))
    {
        // Opening failed, but do we care?
        if (mPersistFlags & PERSIST_FLAGS_FAIL_ON_BROKEN_LINKS)
        {
            SendErrorStatusChange(true, rv, aChannel, aFile);
            EndDownload(NS_ERROR_FAILURE);
            return NS_ERROR_FAILURE;
        }
        return NS_SUCCESS_DONT_FIXUP;
    }

    // Add the output transport to the output map with the channel as the key
    nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(aChannel);
    nsISupportsKey key(keyPtr);
    mOutputMap.Put(&key, new OutputData(aFile, mURI, aCalcFileExt));

    return NS_OK;
}

nsresult
nsWebBrowserPersist::GetExtensionForContentType(const PRUnichar *aContentType, PRUnichar **aExt)
{
    NS_ENSURE_ARG_POINTER(aContentType);
    NS_ENSURE_ARG_POINTER(aExt);

    *aExt = nullptr;

    nsresult rv;
    if (!mMIMEService)
    {
        mMIMEService = do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
        NS_ENSURE_TRUE(mMIMEService, NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsIMIMEInfo> mimeInfo;
    nsAutoCString contentType;
    contentType.AssignWithConversion(aContentType);
    nsAutoCString ext;
    rv = mMIMEService->GetPrimaryExtension(contentType, EmptyCString(), ext);
    if (NS_SUCCEEDED(rv))
    {
        *aExt = UTF8ToNewUnicode(ext);
        NS_ENSURE_TRUE(*aExt, NS_ERROR_OUT_OF_MEMORY);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

nsresult
nsWebBrowserPersist::GetDocumentExtension(nsIDOMDocument *aDocument, PRUnichar **aExt)
{
    NS_ENSURE_ARG_POINTER(aDocument);
    NS_ENSURE_ARG_POINTER(aExt);

    nsXPIDLString contentType;
    nsresult rv = GetDocEncoderContentType(aDocument, nullptr, getter_Copies(contentType));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    return GetExtensionForContentType(contentType.get(), aExt);
}

nsresult
nsWebBrowserPersist::GetDocEncoderContentType(nsIDOMDocument *aDocument, const PRUnichar *aContentType, PRUnichar **aRealContentType)
{
    NS_ENSURE_ARG_POINTER(aDocument);
    NS_ENSURE_ARG_POINTER(aRealContentType);

    *aRealContentType = nullptr;

    nsAutoString defaultContentType(NS_LITERAL_STRING("text/html"));

    // Get the desired content type for the document, either by using the one
    // supplied or from the document itself.

    nsAutoString contentType;
    if (aContentType)
    {
        contentType.Assign(aContentType);
    }
    else
    {
        // Get the content type from the document
        nsAutoString type;
        if (NS_SUCCEEDED(aDocument->GetContentType(type)) && !type.IsEmpty())
            contentType.Assign(type);
    }

    // Check that an encoder actually exists for the desired output type. The
    // following content types will usually yield an encoder.
    //
    //   text/xml
    //   application/xml
    //   application/xhtml+xml
    //   image/svg+xml
    //   text/html
    //   text/plain

    if (!contentType.IsEmpty() &&
        !contentType.Equals(defaultContentType, nsCaseInsensitiveStringComparator()))
    {
        // Check if there is an encoder for the desired content type
        nsAutoCString contractID(NS_DOC_ENCODER_CONTRACTID_BASE);
        AppendUTF16toUTF8(contentType, contractID);

        nsCOMPtr<nsIComponentRegistrar> registrar;
        NS_GetComponentRegistrar(getter_AddRefs(registrar));
        if (registrar)
        {
            bool result;
            nsresult rv = registrar->IsContractIDRegistered(contractID.get(), &result);
            if (NS_SUCCEEDED(rv) && result)
            {
                *aRealContentType = ToNewUnicode(contentType);
            }
        }
    }

    // Use the default if no encoder exists for the desired one
    if (!*aRealContentType)
    {
        *aRealContentType = ToNewUnicode(defaultContentType);
    }
    
    NS_ENSURE_TRUE(*aRealContentType, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

nsresult nsWebBrowserPersist::SaveDocumentInternal(
    nsIDOMDocument *aDocument, nsIURI *aFile, nsIURI *aDataPath)
{
    NS_ENSURE_ARG_POINTER(aDocument);
    NS_ENSURE_ARG_POINTER(aFile);

    // See if we can get the local file representation of this URI
    nsCOMPtr<nsIFile> localFile;
    nsresult rv = GetLocalFileFromURI(aFile, getter_AddRefs(localFile));

    nsCOMPtr<nsIFile> localDataPath;
    if (NS_SUCCEEDED(rv) && aDataPath)
    {
        // See if we can get the local file representation of this URI
        rv = GetLocalFileFromURI(aDataPath, getter_AddRefs(localDataPath));
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsIDOMNode> docAsNode = do_QueryInterface(aDocument);

    // Persist the main document
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
    if (!doc) {
        return NS_ERROR_UNEXPECTED;
    }
    mURI = doc->GetDocumentURI();

    nsCOMPtr<nsIURI> oldBaseURI = mCurrentBaseURI;
    nsAutoCString oldCharset(mCurrentCharset);

    // Store the base URI and the charset
    mCurrentBaseURI = doc->GetBaseURI();
    mCurrentCharset = doc->GetDocumentCharacterSet();

    // Does the caller want to fixup the referenced URIs and save those too?
    if (aDataPath)
    {
        // Basic steps are these.
        //
        // 1. Iterate through the document (and subdocuments) building a list
        //    of unique URIs.
        // 2. For each URI create an OutputData entry and open a channel to save
        //    it. As each URI is saved, discover the mime type and fix up the
        //    local filename with the correct extension.
        // 3. Store the document in a list and wait for URI persistence to finish
        // 4. After URI persistence completes save the list of documents,
        //    fixing it up as it goes out to file.

        nsCOMPtr<nsIURI> oldDataPath = mCurrentDataPath;
        bool oldDataPathIsRelative = mCurrentDataPathIsRelative;
        nsCString oldCurrentRelativePathToData = mCurrentRelativePathToData;
        uint32_t oldThingsToPersist = mCurrentThingsToPersist;

        mCurrentDataPathIsRelative = false;
        mCurrentDataPath = aDataPath;
        mCurrentRelativePathToData = "";
        mCurrentThingsToPersist = 0;

        // Determine if the specified data path is relative to the
        // specified file, (e.g. c:\docs\htmldata is relative to
        // c:\docs\myfile.htm, but not to d:\foo\data.

        // Starting with the data dir work back through its parents
        // checking if one of them matches the base directory.

        if (localDataPath && localFile)
        {
            nsCOMPtr<nsIFile> baseDir;
            localFile->GetParent(getter_AddRefs(baseDir));

            nsAutoCString relativePathToData;
            nsCOMPtr<nsIFile> dataDirParent;
            dataDirParent = localDataPath;
            while (dataDirParent)
            {
                bool sameDir = false;
                dataDirParent->Equals(baseDir, &sameDir);
                if (sameDir)
                {
                    mCurrentRelativePathToData = relativePathToData;
                    mCurrentDataPathIsRelative = true;
                    break;
                }

                nsAutoString dirName;
                dataDirParent->GetLeafName(dirName);

                nsAutoCString newRelativePathToData;
                newRelativePathToData = NS_ConvertUTF16toUTF8(dirName)
                                      + NS_LITERAL_CSTRING("/")
                                      + relativePathToData;
                relativePathToData = newRelativePathToData;

                nsCOMPtr<nsIFile> newDataDirParent;
                rv = dataDirParent->GetParent(getter_AddRefs(newDataDirParent));
                dataDirParent = newDataDirParent;
            }
        }
        else
        {
            // generate a relative path if possible
            nsCOMPtr<nsIURL> pathToBaseURL(do_QueryInterface(aFile));
            if (pathToBaseURL)
            {
                nsAutoCString relativePath;  // nsACString
                if (NS_SUCCEEDED(pathToBaseURL->GetRelativeSpec(aDataPath, relativePath)))
                {
                    mCurrentDataPathIsRelative = true;
                    mCurrentRelativePathToData = relativePath;
                }
            }
        }

        // Store the document in a list so when URI persistence is done and the
        // filenames of saved URIs are known, the documents can be fixed up and
        // saved

        DocData *docData = new DocData;
        docData->mBaseURI = mCurrentBaseURI;
        docData->mCharset = mCurrentCharset;
        docData->mDocument = aDocument;
        docData->mFile = aFile;
        docData->mRelativePathToData = mCurrentRelativePathToData;
        docData->mDataPath = mCurrentDataPath;
        docData->mDataPathIsRelative = mCurrentDataPathIsRelative;
        mDocList.AppendElement(docData);

        // Walk the DOM gathering a list of externally referenced URIs in the uri map
        nsCOMPtr<nsIDOMTreeWalker> walker;
        rv = aDocument->CreateTreeWalker(docAsNode, 
            nsIDOMNodeFilter::SHOW_ELEMENT |
                nsIDOMNodeFilter::SHOW_DOCUMENT |
                nsIDOMNodeFilter::SHOW_PROCESSING_INSTRUCTION,
            nullptr, 1, getter_AddRefs(walker));
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        nsCOMPtr<nsIDOMNode> currentNode;
        walker->GetCurrentNode(getter_AddRefs(currentNode));
        while (currentNode)
        {
            OnWalkDOMNode(currentNode);
            walker->NextNode(getter_AddRefs(currentNode));
        }

        // If there are things to persist, create a directory to hold them
        if (mCurrentThingsToPersist > 0)
        {
            if (localDataPath)
            {
                bool exists = false;
                bool haveDir = false;

                localDataPath->Exists(&exists);
                if (exists)
                {
                    localDataPath->IsDirectory(&haveDir);
                }
                if (!haveDir)
                {
                    rv = localDataPath->Create(nsIFile::DIRECTORY_TYPE, 0755);
                    if (NS_SUCCEEDED(rv))
                        haveDir = true;
                    else
                        SendErrorStatusChange(false, rv, nullptr, aFile);
                }
                if (!haveDir)
                {
                    EndDownload(NS_ERROR_FAILURE);
                    mCurrentBaseURI = oldBaseURI;
                    mCurrentCharset = oldCharset;
                    return NS_ERROR_FAILURE;
                }
                if (mPersistFlags & PERSIST_FLAGS_CLEANUP_ON_FAILURE)
                {
                    // Add to list of things to delete later if all goes wrong
                    CleanupData *cleanupData = new CleanupData;
                    NS_ENSURE_TRUE(cleanupData, NS_ERROR_OUT_OF_MEMORY);
                    cleanupData->mFile = localDataPath;
                    cleanupData->mIsDirectory = true;
                    mCleanupList.AppendElement(cleanupData);
                }
#if defined(XP_OS2)
                // tag the directory with the URI that originated its contents
                nsCOMPtr<nsILocalFileOS2> localFileOS2 = do_QueryInterface(localDataPath);
                if (localFileOS2)
                {
                    nsAutoCString url;
                    mCurrentBaseURI->GetSpec(url);
                    localFileOS2->SetFileSource(url);
                }
#endif
            }
        }

        mCurrentThingsToPersist = oldThingsToPersist;
        mCurrentDataPath = oldDataPath;
        mCurrentDataPathIsRelative = oldDataPathIsRelative;
        mCurrentRelativePathToData = oldCurrentRelativePathToData;
    }
    else
    {
        // Set the document base to ensure relative links still work
        SetDocumentBase(aDocument, mCurrentBaseURI);

        // Get the content type to save with
        nsXPIDLString realContentType;
        GetDocEncoderContentType(aDocument,
            !mContentType.IsEmpty() ? mContentType.get() : nullptr,
            getter_Copies(realContentType));

        nsAutoCString contentType; contentType.AssignWithConversion(realContentType);
        nsAutoCString charType; // Empty

        // Save the document
        rv = SaveDocumentWithFixup(
            aDocument,
            nullptr,  // no dom fixup
            aFile,
            mReplaceExisting,
            contentType,
            charType,
            mEncodingFlags);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    }

    mCurrentBaseURI = oldBaseURI;
    mCurrentCharset = oldCharset;

    return NS_OK;
}

nsresult nsWebBrowserPersist::SaveDocuments()
{
    nsresult rv = NS_OK;

    mStartSaving = true;

    // Iterate through all queued documents, saving them to file and fixing
    // them up on the way.

    uint32_t i;
    for (i = 0; i < mDocList.Length(); i++)
    {
        DocData *docData = mDocList.ElementAt(i);
        if (!docData)
        {
            rv = NS_ERROR_FAILURE;
            break;
        }

        mCurrentBaseURI = docData->mBaseURI;
        mCurrentCharset = docData->mCharset;

        // Save the document, fixing it up with the new URIs as we do
        
        nsEncoderNodeFixup *nodeFixup;
        nodeFixup = new nsEncoderNodeFixup;
        if (nodeFixup)
            nodeFixup->mWebBrowserPersist = this;

        // Get the content type
        nsXPIDLString realContentType;
        GetDocEncoderContentType(docData->mDocument,
            !mContentType.IsEmpty() ? mContentType.get() : nullptr,
            getter_Copies(realContentType));

        nsAutoCString contentType; contentType.AssignWithConversion(realContentType.get());
        nsAutoCString charType; // Empty

        // Save the document, fixing up the links as it goes out
        rv = SaveDocumentWithFixup(
            docData->mDocument,
            nodeFixup,
            docData->mFile,
            mReplaceExisting,
            contentType,
            charType,
            mEncodingFlags);

        if (NS_FAILED(rv))
            break;

        // if we're serializing, bail after first iteration of loop
        if (mSerializingOutput)
            break;
    }

    // delete, cleanup regardless of errors (bug 132417)
    for (i = 0; i < mDocList.Length(); i++)
    {
        DocData *docData = mDocList.ElementAt(i);
        delete docData;
        if (mSerializingOutput)
        {
            mDocList.RemoveElementAt(i);
            break;
        }
    }

    if (!mSerializingOutput)
    {
        mDocList.Clear();
    }

    return rv;
}

void nsWebBrowserPersist::Cleanup()
{
    mURIMap.Enumerate(EnumCleanupURIMap, this);
    mURIMap.Reset();
    mOutputMap.Enumerate(EnumCleanupOutputMap, this);
    mOutputMap.Reset();
    mUploadList.Enumerate(EnumCleanupUploadList, this);
    mUploadList.Reset();
    uint32_t i;
    for (i = 0; i < mDocList.Length(); i++)
    {
        DocData *docData = mDocList.ElementAt(i);
        delete docData;
    }
    mDocList.Clear();
    for (i = 0; i < mCleanupList.Length(); i++)
    {
        CleanupData *cleanupData = mCleanupList.ElementAt(i);
        delete cleanupData;
    }
    mCleanupList.Clear();
    mFilenameList.Clear();
}

void nsWebBrowserPersist::CleanupLocalFiles()
{
    // Two passes, the first pass cleans up files, the second pass tests
    // for and then deletes empty directories. Directories that are not
    // empty after the first pass must contain files from something else
    // and are not deleted.
    int pass;
    for (pass = 0; pass < 2; pass++)
    {
        uint32_t i;
        for (i = 0; i < mCleanupList.Length(); i++)
        {
            CleanupData *cleanupData = mCleanupList.ElementAt(i);
            nsCOMPtr<nsIFile> file = cleanupData->mFile;

            // Test if the dir / file exists (something in an earlier loop
            // may have already removed it)
            bool exists = false;
            file->Exists(&exists);
            if (!exists)
                continue;

            // Test if the file has changed in between creation and deletion
            // in some way that means it should be ignored
            bool isDirectory = false;
            file->IsDirectory(&isDirectory);
            if (isDirectory != cleanupData->mIsDirectory)
                continue; // A file has become a dir or vice versa !

            if (pass == 0 && !isDirectory)
            {
                file->Remove(false);
            }
            else if (pass == 1 && isDirectory) // Directory
            {
                // Directories are more complicated. Enumerate through
                // children looking for files. Any files created by the
                // persist object would have been deleted by the first
                // pass so if there are any there at this stage, the dir
                // cannot be deleted because it has someone else's files
                // in it. Empty child dirs are deleted but they must be
                // recursed through to ensure they are actually empty.

                bool isEmptyDirectory = true;
                nsCOMArray<nsISimpleEnumerator> dirStack;
                int32_t stackSize = 0;

                // Push the top level enum onto the stack
                nsCOMPtr<nsISimpleEnumerator> pos;
                if (NS_SUCCEEDED(file->GetDirectoryEntries(getter_AddRefs(pos))))
                    dirStack.AppendObject(pos);

                while (isEmptyDirectory && (stackSize = dirStack.Count()))
                {
                    // Pop the last element
                    nsCOMPtr<nsISimpleEnumerator> curPos;
                    curPos = dirStack[stackSize-1];
                    dirStack.RemoveObjectAt(stackSize - 1);
                    
                    // Test if the enumerator has any more files in it
                    bool hasMoreElements = false;
                    curPos->HasMoreElements(&hasMoreElements);
                    if (!hasMoreElements)
                    {
                        continue;
                    }

                    // Child files automatically make this code drop out,
                    // while child dirs keep the loop going.
                    nsCOMPtr<nsISupports> child;
                    curPos->GetNext(getter_AddRefs(child));
                    NS_ASSERTION(child, "No child element, but hasMoreElements says otherwise");
                    if (!child)
                        continue;
                    nsCOMPtr<nsIFile> childAsFile = do_QueryInterface(child);
                    NS_ASSERTION(childAsFile, "This should be a file but isn't");

                    bool childIsSymlink = false;
                    childAsFile->IsSymlink(&childIsSymlink);
                    bool childIsDir = false;
                    childAsFile->IsDirectory(&childIsDir);                           
                    if (!childIsDir || childIsSymlink)
                    {
                        // Some kind of file or symlink which means dir
                        // is not empty so just drop out.
                        isEmptyDirectory = false;
                        break;
                    }
                    // Push parent enumerator followed by child enumerator
                    nsCOMPtr<nsISimpleEnumerator> childPos;
                    childAsFile->GetDirectoryEntries(getter_AddRefs(childPos));
                    dirStack.AppendObject(curPos);
                    if (childPos)
                        dirStack.AppendObject(childPos);

                }
                dirStack.Clear();

                // If after all that walking the dir is deemed empty, delete it
                if (isEmptyDirectory)
                {
                    file->Remove(true);
                }
            }
        }
    }
}

nsresult
nsWebBrowserPersist::CalculateUniqueFilename(nsIURI *aURI)
{
    nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
    NS_ENSURE_TRUE(url, NS_ERROR_FAILURE);

    bool nameHasChanged = false;
    nsresult rv;

    // Get the old filename
    nsAutoCString filename;
    rv = url->GetFileName(filename);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    nsAutoCString directory;
    rv = url->GetDirectory(directory);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Split the filename into a base and an extension.
    // e.g. "foo.html" becomes "foo" & ".html"
    //
    // The nsIURL methods GetFileBaseName & GetFileExtension don't
    // preserve the dot whereas this code does to save some effort
    // later when everything is put back together.
    int32_t lastDot = filename.RFind(".");
    nsAutoCString base;
    nsAutoCString ext;
    if (lastDot >= 0)
    {
        filename.Mid(base, 0, lastDot);
        filename.Mid(ext, lastDot, filename.Length() - lastDot); // includes dot
    }
    else
    {
        // filename contains no dot
        base = filename;
    }

    // Test if the filename is longer than allowed by the OS
    int32_t needToChop = filename.Length() - kDefaultMaxFilenameLength;
    if (needToChop > 0)
    {
        // Truncate the base first and then the ext if necessary
        if (base.Length() > (uint32_t) needToChop)
        {
            base.Truncate(base.Length() - needToChop);
        }
        else
        {
            needToChop -= base.Length() - 1;
            base.Truncate(1);
            if (ext.Length() > (uint32_t) needToChop)
            {
                ext.Truncate(ext.Length() - needToChop);
            }
            else
            {
                ext.Truncate(0);
            }
            // If kDefaultMaxFilenameLength were 1 we'd be in trouble here,
            // but that won't happen because it will be set to a sensible
            // value.
        }

        filename.Assign(base);
        filename.Append(ext);
        nameHasChanged = true;
    }

    // Ensure the filename is unique
    // Create a filename if it's empty, or if the filename / datapath is
    // already taken by another URI and create an alternate name.

    if (base.IsEmpty() || !mFilenameList.IsEmpty())
    {
        nsAutoCString tmpPath;
        nsAutoCString tmpBase;
        uint32_t duplicateCounter = 1;
        while (1)
        {
            // Make a file name,
            // Foo become foo_001, foo_002, etc.
            // Empty files become _001, _002 etc.

            if (base.IsEmpty() || duplicateCounter > 1)
            {
                char * tmp = PR_smprintf("_%03d", duplicateCounter);
                NS_ENSURE_TRUE(tmp, NS_ERROR_OUT_OF_MEMORY);
                if (filename.Length() < kDefaultMaxFilenameLength - 4)
                {
                    tmpBase = base;
                }
                else
                {
                    base.Mid(tmpBase, 0, base.Length() - 4);
                }
                tmpBase.Append(tmp);
                PR_smprintf_free(tmp);
            }
            else
            {
                tmpBase = base;
            }
        
            tmpPath.Assign(directory);
            tmpPath.Append(tmpBase);
            tmpPath.Append(ext);

            // Test if the name is a duplicate
            if (!mFilenameList.Contains(tmpPath))
            {
                if (!base.Equals(tmpBase))
                {
                    filename.Assign(tmpBase);
                    filename.Append(ext);
                    nameHasChanged = true;
                }
                break;
            }
            duplicateCounter++;
        }
    }

    // Add name to list of those already used
    nsAutoCString newFilepath(directory);
    newFilepath.Append(filename);
    mFilenameList.AppendElement(newFilepath);

    // Update the uri accordingly if the filename actually changed
    if (nameHasChanged)
    {
        // Final sanity test
        if (filename.Length() > kDefaultMaxFilenameLength)
        {
            NS_WARNING("Filename wasn't truncated less than the max file length - how can that be?");
            return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsIFile> localFile;
        GetLocalFileFromURI(aURI, getter_AddRefs(localFile));

        if (localFile)
        {
            nsAutoString filenameAsUnichar;
            filenameAsUnichar.AssignWithConversion(filename.get());
            localFile->SetLeafName(filenameAsUnichar);

            // Resync the URI with the file after the extension has been appended
            nsresult rv;
            nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
            fileURL->SetFile(localFile);  // this should recalculate uri
        }
        else
        {
            url->SetFileName(filename);
        }
    }

    return NS_OK;
}


nsresult
nsWebBrowserPersist::MakeFilenameFromURI(nsIURI *aURI, nsString &aFilename)
{
    // Try to get filename from the URI.
    nsAutoString fileName;

    // Get a suggested file name from the URL but strip it of characters
    // likely to cause the name to be illegal.

    nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
    if (url)
    {
        nsAutoCString nameFromURL;
        url->GetFileName(nameFromURL);
        if (mPersistFlags & PERSIST_FLAGS_DONT_CHANGE_FILENAMES)
        {
            fileName.AssignWithConversion(NS_UnescapeURL(nameFromURL).get());
            goto end;
        }
        if (!nameFromURL.IsEmpty())
        {
            // Unescape the file name (GetFileName escapes it)
            NS_UnescapeURL(nameFromURL);
            uint32_t nameLength = 0;
            const char *p = nameFromURL.get();
            for (;*p && *p != ';' && *p != '?' && *p != '#' && *p != '.'
                 ;p++)
            {
                if (nsCRT::IsAsciiAlpha(*p) || nsCRT::IsAsciiDigit(*p)
                    || *p == '.' || *p == '-' ||  *p == '_' || (*p == ' '))
                {
                    fileName.Append(PRUnichar(*p));
                    if (++nameLength == kDefaultMaxFilenameLength)
                    {
                        // Note:
                        // There is no point going any further since it will be
                        // truncated in CalculateUniqueFilename anyway.
                        // More importantly, certain implementations of
                        // nsIFile (e.g. the Mac impl) might truncate
                        // names in undesirable ways, such as truncating from
                        // the middle, inserting ellipsis and so on.
                        break;
                    }
                }
            }
        }
    }

    // Empty filenames can confuse the local file object later 
    // when it attempts to set the leaf name in CalculateUniqueFilename
    // for duplicates and ends up replacing the parent dir. To avoid
    // the problem, all filenames are made at least one character long.
    if (fileName.IsEmpty())
    {
        fileName.Append(PRUnichar('a')); // 'a' is for arbitrary
    }
 
end:
    aFilename = fileName;
    return NS_OK;
}


nsresult
nsWebBrowserPersist::CalculateAndAppendFileExt(nsIURI *aURI, nsIChannel *aChannel, nsIURI *aOriginalURIWithExtension)
{
    nsresult rv;

    if (!mMIMEService)
    {
        mMIMEService = do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
        NS_ENSURE_TRUE(mMIMEService, NS_ERROR_FAILURE);
    }

    nsAutoCString contentType;

    // Get the content type from the channel
    aChannel->GetContentType(contentType);

    // Get the content type from the MIME service
    if (contentType.IsEmpty())
    {
        nsCOMPtr<nsIURI> uri;
        aChannel->GetOriginalURI(getter_AddRefs(uri));
        mMIMEService->GetTypeFromURI(uri, contentType);
    }

    // Append the extension onto the file
    if (!contentType.IsEmpty())
    {
        nsCOMPtr<nsIMIMEInfo> mimeInfo;
        mMIMEService->GetFromTypeAndExtension(
            contentType, EmptyCString(), getter_AddRefs(mimeInfo));

        nsCOMPtr<nsIFile> localFile;
        GetLocalFileFromURI(aURI, getter_AddRefs(localFile));

        if (mimeInfo)
        {
            nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
            NS_ENSURE_TRUE(url, NS_ERROR_FAILURE);

            nsAutoCString newFileName;
            url->GetFileName(newFileName);

            // Test if the current extension is current for the mime type
            bool hasExtension = false;
            int32_t ext = newFileName.RFind(".");
            if (ext != -1)
            {
                mimeInfo->ExtensionExists(Substring(newFileName, ext + 1), &hasExtension);
            }

            // Append the mime file extension
            nsAutoCString fileExt;
            if (!hasExtension)
            {
                // Test if previous extension is acceptable
                nsCOMPtr<nsIURL> oldurl(do_QueryInterface(aOriginalURIWithExtension));
                NS_ENSURE_TRUE(oldurl, NS_ERROR_FAILURE);
                oldurl->GetFileExtension(fileExt);
                bool useOldExt = false;
                if (!fileExt.IsEmpty())
                {
                    mimeInfo->ExtensionExists(fileExt, &useOldExt);
                }

                // can't use old extension so use primary extension
                if (!useOldExt)
                {
                    mimeInfo->GetPrimaryExtension(fileExt);
                } 

                if (!fileExt.IsEmpty())
                {
                    uint32_t newLength = newFileName.Length() + fileExt.Length() + 1;
                    if (newLength > kDefaultMaxFilenameLength)
                    {
                        if (fileExt.Length() > kDefaultMaxFilenameLength/2)
                            fileExt.Truncate(kDefaultMaxFilenameLength/2);

                        uint32_t diff = kDefaultMaxFilenameLength - 1 -
                                        fileExt.Length();
                        if (newFileName.Length() > diff)
                            newFileName.Truncate(diff);
                    }
                    newFileName.Append(".");
                    newFileName.Append(fileExt);
                }

                if (localFile)
                {
                    localFile->SetLeafName(NS_ConvertUTF8toUTF16(newFileName));

                    // Resync the URI with the file after the extension has been appended
                    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
                    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
                    fileURL->SetFile(localFile);  // this should recalculate uri
                }
                else
                {
                    url->SetFileName(newFileName);
                }
            }

        }
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeOutputStream(
    nsIURI *aURI, nsIOutputStream **aOutputStream)
{
    nsresult rv;

    nsCOMPtr<nsIFile> localFile;
    GetLocalFileFromURI(aURI, getter_AddRefs(localFile));
    if (localFile)
        rv = MakeOutputStreamFromFile(localFile, aOutputStream);
    else
        rv = MakeOutputStreamFromURI(aURI, aOutputStream);

    return rv;
}

nsresult
nsWebBrowserPersist::MakeOutputStreamFromFile(
    nsIFile *aFile, nsIOutputStream **aOutputStream)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIFileOutputStream> fileOutputStream =
        do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // XXX brade:  get the right flags here!
    int32_t ioFlags = -1;
    if (mPersistFlags & nsIWebBrowserPersist::PERSIST_FLAGS_APPEND_TO_FILE)
      ioFlags = PR_APPEND | PR_CREATE_FILE | PR_WRONLY; 
    rv = fileOutputStream->Init(aFile, ioFlags, -1, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    *aOutputStream = NS_BufferOutputStream(fileOutputStream,
                                           BUFFERED_OUTPUT_SIZE).get();

    if (mPersistFlags & PERSIST_FLAGS_CLEANUP_ON_FAILURE)
    {
        // Add to cleanup list in event of failure
        CleanupData *cleanupData = new CleanupData;
        if (!cleanupData) {
          NS_RELEASE(*aOutputStream);
          return NS_ERROR_OUT_OF_MEMORY;
        }
        cleanupData->mFile = aFile;
        cleanupData->mIsDirectory = false;
        mCleanupList.AppendElement(cleanupData);
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeOutputStreamFromURI(
    nsIURI *aURI, nsIOutputStream  **aOutputStream)
{
    uint32_t segsize = 8192;
    uint32_t maxsize = uint32_t(-1);
    nsCOMPtr<nsIStorageStream> storStream;
    nsresult rv = NS_NewStorageStream(segsize, maxsize, getter_AddRefs(storStream));
    NS_ENSURE_SUCCESS(rv, rv);
    
    NS_ENSURE_SUCCESS(CallQueryInterface(storStream, aOutputStream), NS_ERROR_FAILURE);
    return NS_OK;
}

void
nsWebBrowserPersist::EndDownload(nsresult aResult)
{
    // Store the error code in the result if it is an error
    if (NS_SUCCEEDED(mPersistResult) && NS_FAILED(aResult))
    {
        mPersistResult = aResult;
    }

    // Do file cleanup if required
    if (NS_FAILED(aResult) && (mPersistFlags & PERSIST_FLAGS_CLEANUP_ON_FAILURE))
    {
        CleanupLocalFiles();
    }

    // Cleanup the channels
    mCompleted = true;
    Cleanup();
}

/* Hack class to get access to nsISupportsKey's protected mKey member */
class nsMyISupportsKey : public nsISupportsKey
{
public:
    nsMyISupportsKey(nsISupports *key) : nsISupportsKey(key)
    {
    }

    nsresult GetISupports(nsISupports **ret)
    {
        *ret = mKey;
        NS_IF_ADDREF(mKey);
        return NS_OK;
    }
};

struct MOZ_STACK_CLASS FixRedirectData
{
    nsCOMPtr<nsIChannel> mNewChannel;
    nsCOMPtr<nsIURI> mOriginalURI;
    nsISupportsKey *mMatchingKey;
};

nsresult
nsWebBrowserPersist::FixRedirectedChannelEntry(nsIChannel *aNewChannel)
{
    NS_ENSURE_ARG_POINTER(aNewChannel);
    nsCOMPtr<nsIURI> originalURI;

    // Enumerate through existing open channels looking for one with
    // a URI matching the one specified.

    FixRedirectData data;
    data.mMatchingKey = nullptr;
    data.mNewChannel = aNewChannel;
    data.mNewChannel->GetOriginalURI(getter_AddRefs(data.mOriginalURI));
    mOutputMap.Enumerate(EnumFixRedirect, (void *) &data);

    // If a match is found, remove the data entry with the old channel key
    // and re-add it with the new channel key.

    if (data.mMatchingKey)
    {
        OutputData *outputData = (OutputData *) mOutputMap.Get(data.mMatchingKey);
        NS_ENSURE_TRUE(outputData, NS_ERROR_FAILURE);
        mOutputMap.Remove(data.mMatchingKey);

        // Store data again with new channel unless told to ignore redirects
        if (!(mPersistFlags & PERSIST_FLAGS_IGNORE_REDIRECTED_DATA))
        {
            nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(aNewChannel);
            nsISupportsKey key(keyPtr);
            mOutputMap.Put(&key, outputData);
        }
    }

    return NS_OK;
}

bool
nsWebBrowserPersist::EnumFixRedirect(nsHashKey *aKey, void *aData, void* closure)
{
    FixRedirectData *data = (FixRedirectData *) closure;

    nsCOMPtr<nsISupports> keyPtr;
    ((nsMyISupportsKey *) aKey)->GetISupports(getter_AddRefs(keyPtr));

    nsCOMPtr<nsIChannel> thisChannel = do_QueryInterface(keyPtr);
    nsCOMPtr<nsIURI> thisURI;

    thisChannel->GetOriginalURI(getter_AddRefs(thisURI));

    // Compare this channel's URI to the one passed in.
    bool matchingURI = false;
    thisURI->Equals(data->mOriginalURI, &matchingURI);
    if (matchingURI)
    {
        data->mMatchingKey = (nsISupportsKey *) aKey;
        return false; // Stop enumerating
    }

    return true;
}

void
nsWebBrowserPersist::CalcTotalProgress()
{
    mTotalCurrentProgress = 0;
    mTotalMaxProgress = 0;

    if (mOutputMap.Count() > 0)
    {
        // Total up the progress of each output stream
        mOutputMap.Enumerate(EnumCalcProgress, this);
    }

    if (mUploadList.Count() > 0)
    {
        // Total up the progress of each upload
        mUploadList.Enumerate(EnumCalcUploadProgress, this);
    }

    // XXX this code seems pretty bogus and pointless
    if (mTotalCurrentProgress == 0 && mTotalMaxProgress == 0)
    {
        // No output streams so we must be complete
        mTotalCurrentProgress = 10000;
        mTotalMaxProgress = 10000;
    }
}

bool
nsWebBrowserPersist::EnumCalcProgress(nsHashKey *aKey, void *aData, void* closure)
{
    nsWebBrowserPersist *pthis = (nsWebBrowserPersist *) closure;
    OutputData *data = (OutputData *) aData;

    // only count toward total progress if destination file is local
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(data->mFile);
    if (fileURL)
    {
        pthis->mTotalCurrentProgress += data->mSelfProgress;
        pthis->mTotalMaxProgress += data->mSelfProgressMax;
    }
    return true;
}

bool
nsWebBrowserPersist::EnumCalcUploadProgress(nsHashKey *aKey, void *aData, void* closure)
{
    if (aData && closure)
    {
        nsWebBrowserPersist *pthis = (nsWebBrowserPersist *) closure;
        UploadData *data = (UploadData *) aData;
        pthis->mTotalCurrentProgress += data->mSelfProgress;
        pthis->mTotalMaxProgress += data->mSelfProgressMax;
    }
    return true;
}

bool
nsWebBrowserPersist::EnumCountURIsToPersist(nsHashKey *aKey, void *aData, void* closure)
{
    URIData *data = (URIData *) aData;
    uint32_t *count = (uint32_t *) closure;
    if (data->mNeedsPersisting && !data->mSaved)
    {
        (*count)++;
    }
    return true;
}

bool
nsWebBrowserPersist::EnumPersistURIs(nsHashKey *aKey, void *aData, void* closure)
{
    URIData *data = (URIData *) aData;
    if (!data->mNeedsPersisting || data->mSaved)
    {
        return true;
    }

    nsWebBrowserPersist *pthis = (nsWebBrowserPersist *) closure;
    nsresult rv;

    // Create a URI from the key
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), 
                   nsDependentCString(((nsCStringKey *) aKey)->GetString(),
                                      ((nsCStringKey *) aKey)->GetStringLength()),
                   data->mCharset.get());
    NS_ENSURE_SUCCESS(rv, false);

    // Make a URI to save the data to
    nsCOMPtr<nsIURI> fileAsURI;
    rv = data->mDataPath->Clone(getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, false);
    rv = pthis->AppendPathToURI(fileAsURI, data->mFilename);
    NS_ENSURE_SUCCESS(rv, false);

    rv = pthis->SaveURIInternal(uri, nullptr, nullptr, nullptr, nullptr, fileAsURI, true,
                                pthis->mIsPrivate);
    // if SaveURIInternal fails, then it will have called EndDownload,
    // which means that |aData| is no longer valid memory.  we MUST bail.
    NS_ENSURE_SUCCESS(rv, false);

    if (rv == NS_OK)
    {
        // Store the actual object because once it's persisted this
        // will be fixed up with the right file extension.

        data->mFile = fileAsURI;
        data->mSaved = true;
    }
    else
    {
        data->mNeedsFixup = false;
    }

    if (pthis->mSerializingOutput)
        return false;

    return true;
}

bool
nsWebBrowserPersist::EnumCleanupOutputMap(nsHashKey *aKey, void *aData, void* closure)
{
    nsCOMPtr<nsISupports> keyPtr;
    ((nsMyISupportsKey *) aKey)->GetISupports(getter_AddRefs(keyPtr));
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(keyPtr);
    if (channel)
    {
        channel->Cancel(NS_BINDING_ABORTED);
    }
    OutputData *data = (OutputData *) aData;
    delete data;
    return true;
}


bool
nsWebBrowserPersist::EnumCleanupURIMap(nsHashKey *aKey, void *aData, void* closure)
{
    URIData *data = (URIData *) aData;
    delete data; // Delete data associated with key
    return true;
}


bool
nsWebBrowserPersist::EnumCleanupUploadList(nsHashKey *aKey, void *aData, void* closure)
{
    nsCOMPtr<nsISupports> keyPtr;
    ((nsMyISupportsKey *) aKey)->GetISupports(getter_AddRefs(keyPtr));
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(keyPtr);
    if (channel)
    {
        channel->Cancel(NS_BINDING_ABORTED);
    }
    UploadData *data = (UploadData *) aData;
    delete data; // Delete data associated with key
    return true;
}

nsresult nsWebBrowserPersist::FixupXMLStyleSheetLink(nsIDOMProcessingInstruction *aPI, const nsAString &aHref)
{
    NS_ENSURE_ARG_POINTER(aPI);
    nsresult rv = NS_OK;

    nsAutoString data;
    rv = aPI->GetData(data);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsAutoString href;
    nsContentUtils::GetPseudoAttributeValue(data,
                                            nsGkAtoms::href,
                                            href);

    // Construct and set a new data value for the xml-stylesheet
    if (!aHref.IsEmpty() && !href.IsEmpty())
    {
        nsAutoString alternate;
        nsAutoString charset;
        nsAutoString title;
        nsAutoString type;
        nsAutoString media;

        nsContentUtils::GetPseudoAttributeValue(data,
                                                nsGkAtoms::alternate,
                                                alternate);
        nsContentUtils::GetPseudoAttributeValue(data,
                                                nsGkAtoms::charset,
                                                charset);
        nsContentUtils::GetPseudoAttributeValue(data,
                                                nsGkAtoms::title,
                                                title);
        nsContentUtils::GetPseudoAttributeValue(data,
                                                nsGkAtoms::type,
                                                type);
        nsContentUtils::GetPseudoAttributeValue(data,
                                                nsGkAtoms::media,
                                                media);

        NS_NAMED_LITERAL_STRING(kCloseAttr, "\" ");
        nsAutoString newData;
        newData += NS_LITERAL_STRING("href=\"") + aHref + kCloseAttr;
        if (!title.IsEmpty())
        {
            newData += NS_LITERAL_STRING("title=\"") + title + kCloseAttr;
        }
        if (!media.IsEmpty())
        {
            newData += NS_LITERAL_STRING("media=\"") + media + kCloseAttr;
        }
        if (!type.IsEmpty())
        {
            newData += NS_LITERAL_STRING("type=\"") + type + kCloseAttr;
        }
        if (!charset.IsEmpty())
        {
            newData += NS_LITERAL_STRING("charset=\"") + charset + kCloseAttr;
        }
        if (!alternate.IsEmpty())
        {
            newData += NS_LITERAL_STRING("alternate=\"") + alternate + kCloseAttr;
        }
        newData.Truncate(newData.Length() - 1);  // Remove the extra space on the end.
        aPI->SetData(newData);
    }

    return rv;
}

nsresult nsWebBrowserPersist::GetXMLStyleSheetLink(nsIDOMProcessingInstruction *aPI, nsAString &aHref)
{
    NS_ENSURE_ARG_POINTER(aPI);

    nsresult rv = NS_OK;
    nsAutoString data;
    rv = aPI->GetData(data);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::href, aHref);

    return NS_OK;
}

nsresult nsWebBrowserPersist::OnWalkDOMNode(nsIDOMNode *aNode)
{
    // Fixup xml-stylesheet processing instructions
    nsCOMPtr<nsIDOMProcessingInstruction> nodeAsPI = do_QueryInterface(aNode);
    if (nodeAsPI)
    {
        nsAutoString target;
        nodeAsPI->GetTarget(target);
        if (target.EqualsLiteral("xml-stylesheet"))
        {
            nsAutoString href;
            GetXMLStyleSheetLink(nodeAsPI, href);
            if (!href.IsEmpty())
            {
                StoreURI(NS_ConvertUTF16toUTF8(href).get());
            }
        }
        return NS_OK;
    }

    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (!content)
    {
        return NS_OK;
    }

    // Test the node to see if it's an image, frame, iframe, css, js
    nsCOMPtr<nsIDOMHTMLImageElement> nodeAsImage = do_QueryInterface(aNode);
    if (nodeAsImage)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }

    if (content->IsSVG(nsGkAtoms::img))
    {
        StoreURIAttributeNS(aNode, "http://www.w3.org/1999/xlink", "href");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLMediaElement> nodeAsMedia = do_QueryInterface(aNode);
    if (nodeAsMedia)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }
    nsCOMPtr<nsIDOMHTMLSourceElement> nodeAsSource = do_QueryInterface(aNode);
    if (nodeAsSource)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }

    if (content->IsHTML(nsGkAtoms::body)) {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    if (content->IsHTML(nsGkAtoms::table)) {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    if (content->IsHTML(nsGkAtoms::tr)) {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    if (content->IsHTML(nsGkAtoms::td) || content->IsHTML(nsGkAtoms::th)) {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLScriptElement> nodeAsScript = do_QueryInterface(aNode);
    if (nodeAsScript)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }

    if (content->IsSVG(nsGkAtoms::script))
    {
        StoreURIAttributeNS(aNode, "http://www.w3.org/1999/xlink", "href");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLEmbedElement> nodeAsEmbed = do_QueryInterface(aNode);
    if (nodeAsEmbed)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }
    
    nsCOMPtr<nsIDOMHTMLObjectElement> nodeAsObject = do_QueryInterface(aNode);
    if (nodeAsObject)
    {
        StoreURIAttribute(aNode, "data");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLAppletElement> nodeAsApplet = do_QueryInterface(aNode);
    if (nodeAsApplet)
    {
        // For an applet, relative URIs are resolved relative to the
        // codebase (which is resolved relative to the base URI).
        nsCOMPtr<nsIURI> oldBase = mCurrentBaseURI;
        nsAutoString codebase;
        nodeAsApplet->GetCodeBase(codebase);
        if (!codebase.IsEmpty()) {
            nsCOMPtr<nsIURI> baseURI;
            NS_NewURI(getter_AddRefs(baseURI), codebase,
                      mCurrentCharset.get(), mCurrentBaseURI);
            if (baseURI) {
                mCurrentBaseURI = baseURI;
            }
        }

        URIData *archiveURIData = nullptr;
        StoreURIAttribute(aNode, "archive", true, &archiveURIData);
        // We only store 'code' locally if there is no 'archive',
        // otherwise we assume the archive file(s) contains it (bug 430283).
        if (!archiveURIData)
            StoreURIAttribute(aNode, "code");

        // restore the base URI we really want to have
        mCurrentBaseURI = oldBase;
        return NS_OK;
    }
    
    nsCOMPtr<nsIDOMHTMLLinkElement> nodeAsLink = do_QueryInterface(aNode);
    if (nodeAsLink)
    {
        // Test if the link has a rel value indicating it to be a stylesheet
        nsAutoString linkRel;
        if (NS_SUCCEEDED(nodeAsLink->GetRel(linkRel)) && !linkRel.IsEmpty())
        {
            nsReadingIterator<PRUnichar> start;
            nsReadingIterator<PRUnichar> end;
            nsReadingIterator<PRUnichar> current;

            linkRel.BeginReading(start);
            linkRel.EndReading(end);

            // Walk through space delimited string looking for "stylesheet"
            for (current = start; current != end; ++current)
            {
                // Ignore whitespace
                if (nsCRT::IsAsciiSpace(*current))
                    continue;

                // Grab the next space delimited word
                nsReadingIterator<PRUnichar> startWord = current;
                do {
                    ++current;
                } while (current != end && !nsCRT::IsAsciiSpace(*current));

                // Store the link for fix up if it says "stylesheet"
                if (Substring(startWord, current)
                        .LowerCaseEqualsLiteral("stylesheet"))
                {
                    StoreURIAttribute(aNode, "href");
                    return NS_OK;
                }
                if (current == end)
                    break;
            }
        }
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLFrameElement> nodeAsFrame = do_QueryInterface(aNode);
    if (nodeAsFrame)
    {
        URIData *data = nullptr;
        StoreURIAttribute(aNode, "src", false, &data);
        if (data)
        {
            data->mIsSubFrame = true;
            // Save the frame content
            nsCOMPtr<nsIDOMDocument> content;
            nodeAsFrame->GetContentDocument(getter_AddRefs(content));
            if (content)
            {
                SaveSubframeContent(content, data);
            }
        }
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLIFrameElement> nodeAsIFrame = do_QueryInterface(aNode);
    if (nodeAsIFrame && !(mPersistFlags & PERSIST_FLAGS_IGNORE_IFRAMES))
    {
        URIData *data = nullptr;
        StoreURIAttribute(aNode, "src", false, &data);
        if (data)
        {
            data->mIsSubFrame = true;
            // Save the frame content
            nsCOMPtr<nsIDOMDocument> content;
            nodeAsIFrame->GetContentDocument(getter_AddRefs(content));
            if (content)
            {
                SaveSubframeContent(content, data);
            }
        }
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLInputElement> nodeAsInput = do_QueryInterface(aNode);
    if (nodeAsInput)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::GetNodeToFixup(nsIDOMNode *aNodeIn, nsIDOMNode **aNodeOut)
{
    if (!(mPersistFlags & PERSIST_FLAGS_FIXUP_ORIGINAL_DOM))
    {
        nsresult rv = aNodeIn->CloneNode(false, 1, aNodeOut);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
        NS_ADDREF(*aNodeOut = aNodeIn);
    }
    nsCOMPtr<nsIDOMHTMLElement> element(do_QueryInterface(*aNodeOut));
    if (element) {
        // Make sure this is not XHTML
        nsAutoString namespaceURI;
        element->GetNamespaceURI(namespaceURI);
        if (namespaceURI.IsEmpty()) {
            // This is a tag-soup node.  It may have a _base_href attribute
            // stuck on it by the parser, but since we're fixing up all URIs
            // relative to the overall document base that will screw us up.
            // Just remove the _base_href.
            element->RemoveAttribute(NS_LITERAL_STRING("_base_href"));
        }
    }
    return NS_OK;
}

nsresult
nsWebBrowserPersist::CloneNodeWithFixedUpAttributes(
    nsIDOMNode *aNodeIn, bool *aSerializeCloneKids, nsIDOMNode **aNodeOut)
{
    nsresult rv;
    *aNodeOut = nullptr;
    *aSerializeCloneKids = false;

    // Fixup xml-stylesheet processing instructions
    nsCOMPtr<nsIDOMProcessingInstruction> nodeAsPI = do_QueryInterface(aNodeIn);
    if (nodeAsPI)
    {
        nsAutoString target;
        nodeAsPI->GetTarget(target);
        if (target.EqualsLiteral("xml-stylesheet"))
        {
            rv = GetNodeToFixup(aNodeIn, aNodeOut);
            if (NS_SUCCEEDED(rv) && *aNodeOut)
            {
                nsCOMPtr<nsIDOMProcessingInstruction> outNode = do_QueryInterface(*aNodeOut);
                nsAutoString href;
                GetXMLStyleSheetLink(nodeAsPI, href);
                if (!href.IsEmpty())
                {
                    FixupURI(href);
                    FixupXMLStyleSheetLink(outNode, href);
                }
            }
        }
    }

    // BASE elements are replaced by a comment so relative links are not hosed.

    if (!(mPersistFlags & PERSIST_FLAGS_NO_BASE_TAG_MODIFICATIONS))
    {
        nsCOMPtr<nsIDOMHTMLBaseElement> nodeAsBase = do_QueryInterface(aNodeIn);
        if (nodeAsBase)
        {
            nsCOMPtr<nsIDOMDocument> ownerDocument;
            HTMLSharedElement* base = static_cast<HTMLSharedElement*>(nodeAsBase.get());
            base->GetOwnerDocument(getter_AddRefs(ownerDocument));
            if (ownerDocument)
            {
                nsAutoString href;
                base->GetHref(href); // Doesn't matter if this fails
                nsCOMPtr<nsIDOMComment> comment;
                nsAutoString commentText; commentText.AssignLiteral(" base ");
                if (!href.IsEmpty())
                {
                    commentText += NS_LITERAL_STRING("href=\"") + href + NS_LITERAL_STRING("\" ");
                }
                rv = ownerDocument->CreateComment(commentText, getter_AddRefs(comment));
                if (comment)
                {
                    return CallQueryInterface(comment, aNodeOut);
                }
            }
        }
    }

    nsCOMPtr<nsIContent> content = do_QueryInterface(aNodeIn);
    if (!content)
    {
        return NS_OK;
    }

    // Fix up href and file links in the elements

    nsCOMPtr<nsIDOMHTMLAnchorElement> nodeAsAnchor = do_QueryInterface(aNodeIn);
    if (nodeAsAnchor)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupAnchor(*aNodeOut);
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLAreaElement> nodeAsArea = do_QueryInterface(aNodeIn);
    if (nodeAsArea)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupAnchor(*aNodeOut);
        }
        return rv;
    }

    if (content->IsHTML(nsGkAtoms::body)) {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "background");
        }
        return rv;
    }

    if (content->IsHTML(nsGkAtoms::table)) {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "background");
        }
        return rv;
    }

    if (content->IsHTML(nsGkAtoms::tr)) {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "background");
        }
        return rv;
    }

    if (content->IsHTML(nsGkAtoms::td) || content->IsHTML(nsGkAtoms::th)) {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "background");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLImageElement> nodeAsImage = do_QueryInterface(aNodeIn);
    if (nodeAsImage)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            // Disable image loads
            nsCOMPtr<nsIImageLoadingContent> imgCon =
                do_QueryInterface(*aNodeOut);
            if (imgCon)
                imgCon->SetLoadingEnabled(false);

            FixupAnchor(*aNodeOut);
            FixupNodeAttribute(*aNodeOut, "src");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLMediaElement> nodeAsMedia = do_QueryInterface(aNodeIn);
    if (nodeAsMedia)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "src");
        }

        return rv;
    }

    nsCOMPtr<nsIDOMHTMLSourceElement> nodeAsSource = do_QueryInterface(aNodeIn);
    if (nodeAsSource)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "src");
        }

        return rv;
    }

    if (content->IsSVG(nsGkAtoms::img))
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            // Disable image loads
            nsCOMPtr<nsIImageLoadingContent> imgCon =
                do_QueryInterface(*aNodeOut);
            if (imgCon)
                imgCon->SetLoadingEnabled(false);

            // FixupAnchor(*aNodeOut);  // XXXjwatt: is this line needed?
            FixupNodeAttributeNS(*aNodeOut, "http://www.w3.org/1999/xlink", "href");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLScriptElement> nodeAsScript = do_QueryInterface(aNodeIn);
    if (nodeAsScript)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "src");
        }
        return rv;
    }

    if (content->IsSVG(nsGkAtoms::script))
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttributeNS(*aNodeOut, "http://www.w3.org/1999/xlink", "href");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLEmbedElement> nodeAsEmbed = do_QueryInterface(aNodeIn);
    if (nodeAsEmbed)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "src");
        }
        return rv;
    }
    
    nsCOMPtr<nsIDOMHTMLObjectElement> nodeAsObject = do_QueryInterface(aNodeIn);
    if (nodeAsObject)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "data");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLAppletElement> nodeAsApplet = do_QueryInterface(aNodeIn);
    if (nodeAsApplet)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            nsCOMPtr<nsIDOMHTMLAppletElement> newApplet =
                do_QueryInterface(*aNodeOut);
            // For an applet, relative URIs are resolved relative to the
            // codebase (which is resolved relative to the base URI).
            nsCOMPtr<nsIURI> oldBase = mCurrentBaseURI;
            nsAutoString codebase;
            nodeAsApplet->GetCodeBase(codebase);
            if (!codebase.IsEmpty()) {
                nsCOMPtr<nsIURI> baseURI;
                NS_NewURI(getter_AddRefs(baseURI), codebase,
                          mCurrentCharset.get(), mCurrentBaseURI);
                if (baseURI) {
                    mCurrentBaseURI = baseURI;
                }
            }
            // Unset the codebase too, since we'll correctly relativize the
            // code and archive paths.
            static_cast<HTMLSharedObjectElement*>(newApplet.get())->
              RemoveAttribute(NS_LITERAL_STRING("codebase"));
            FixupNodeAttribute(*aNodeOut, "code");
            FixupNodeAttribute(*aNodeOut, "archive");
            // restore the base URI we really want to have
            mCurrentBaseURI = oldBase;
        }
        return rv;
    }
    
    nsCOMPtr<nsIDOMHTMLLinkElement> nodeAsLink = do_QueryInterface(aNodeIn);
    if (nodeAsLink)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            // First see if the link represents linked content
            rv = FixupNodeAttribute(*aNodeOut, "href");
            if (NS_FAILED(rv))
            {
                // Perhaps this link is actually an anchor to related content
                FixupAnchor(*aNodeOut);
            }
            // TODO if "type" attribute == "text/css"
            //        fixup stylesheet
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLFrameElement> nodeAsFrame = do_QueryInterface(aNodeIn);
    if (nodeAsFrame)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "src");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLIFrameElement> nodeAsIFrame = do_QueryInterface(aNodeIn);
    if (nodeAsIFrame)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "src");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLInputElement> nodeAsInput = do_QueryInterface(aNodeIn);
    if (nodeAsInput)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            // Disable image loads
            nsCOMPtr<nsIImageLoadingContent> imgCon =
                do_QueryInterface(*aNodeOut);
            if (imgCon)
                imgCon->SetLoadingEnabled(false);

            FixupNodeAttribute(*aNodeOut, "src");

            nsAutoString valueStr;
            NS_NAMED_LITERAL_STRING(valueAttr, "value");
            // Update element node attributes with user-entered form state
            nsCOMPtr<nsIContent> content = do_QueryInterface(*aNodeOut);
            nsRefPtr<HTMLInputElement> outElt =
              HTMLInputElement::FromContentOrNull(content);
            nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(*aNodeOut);
            switch (formControl->GetType()) {
                case NS_FORM_INPUT_EMAIL:
                case NS_FORM_INPUT_SEARCH:
                case NS_FORM_INPUT_TEXT:
                case NS_FORM_INPUT_TEL:
                case NS_FORM_INPUT_URL:
                case NS_FORM_INPUT_NUMBER:
                case NS_FORM_INPUT_RANGE:
                case NS_FORM_INPUT_DATE:
                case NS_FORM_INPUT_TIME:
                case NS_FORM_INPUT_COLOR:
                    nodeAsInput->GetValue(valueStr);
                    // Avoid superfluous value="" serialization
                    if (valueStr.IsEmpty())
                      outElt->RemoveAttribute(valueAttr);
                    else
                      outElt->SetAttribute(valueAttr, valueStr);
                    break;
                case NS_FORM_INPUT_CHECKBOX:
                case NS_FORM_INPUT_RADIO:
                    bool checked;
                    nodeAsInput->GetChecked(&checked);
                    outElt->SetDefaultChecked(checked);
                    break;
                default:
                    break;
            }
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLTextAreaElement> nodeAsTextArea = do_QueryInterface(aNodeIn);
    if (nodeAsTextArea)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            // Tell the document encoder to serialize the text child we create below
            *aSerializeCloneKids = true;

            nsAutoString valueStr;
            nodeAsTextArea->GetValue(valueStr);
            
            (*aNodeOut)->SetTextContent(valueStr);
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLOptionElement> nodeAsOption = do_QueryInterface(aNodeIn);
    if (nodeAsOption)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {          
            nsCOMPtr<nsIDOMHTMLOptionElement> outElt = do_QueryInterface(*aNodeOut);
            bool selected;
            nodeAsOption->GetSelected(&selected);
            outElt->SetDefaultSelected(selected);
        }
        return rv;
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::StoreURI(
    const char *aURI, bool aNeedsPersisting, URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aURI);

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri),
                            nsDependentCString(aURI),
                            mCurrentCharset.get(),
                            mCurrentBaseURI);
    NS_ENSURE_SUCCESS(rv, rv);

    return StoreURI(uri, aNeedsPersisting, aData);
}

nsresult
nsWebBrowserPersist::StoreURI(
    nsIURI *aURI, bool aNeedsPersisting, URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aURI);
    if (aData)
    {
        *aData = nullptr;
    }

    // Test if this URI should be persisted. By default
    // we should assume the URI  is persistable.
    bool doNotPersistURI;
    nsresult rv = NS_URIChainHasFlags(aURI,
                                      nsIProtocolHandler::URI_NON_PERSISTABLE,
                                      &doNotPersistURI);
    if (NS_FAILED(rv))
    {
        doNotPersistURI = false;
    }

    if (doNotPersistURI)
    {
        return NS_OK;
    }

    URIData *data = nullptr;
    MakeAndStoreLocalFilenameInURIMap(aURI, aNeedsPersisting, &data);
    if (aData)
    {
        *aData = data;
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::StoreURIAttributeNS(
    nsIDOMNode *aNode, const char *aNamespaceURI, const char *aAttribute,
    bool aNeedsPersisting, URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aNode);
    NS_ENSURE_ARG_POINTER(aNamespaceURI);
    NS_ENSURE_ARG_POINTER(aAttribute);

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
    MOZ_ASSERT(element);

    // Find the named URI attribute on the (element) node and store
    // a reference to the URI that maps onto a local file name

    nsCOMPtr<nsIDOMMozNamedAttrMap> attrMap;
    nsresult rv = element->GetAttributes(getter_AddRefs(attrMap));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    NS_ConvertASCIItoUTF16 namespaceURI(aNamespaceURI);
    NS_ConvertASCIItoUTF16 attribute(aAttribute);
    nsCOMPtr<nsIDOMAttr> attr;
    rv = attrMap->GetNamedItemNS(namespaceURI, attribute, getter_AddRefs(attr));
    if (attr)
    {
        nsAutoString oldValue;
        attr->GetValue(oldValue);
        if (!oldValue.IsEmpty())
        {
            NS_ConvertUTF16toUTF8 oldCValue(oldValue);
            return StoreURI(oldCValue.get(), aNeedsPersisting, aData);
        }
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::FixupURI(nsAString &aURI)
{
    // get the current location of the file (absolutized)
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI, 
                            mCurrentCharset.get(), mCurrentBaseURI);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString spec;
    rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Search for the URI in the map and replace it with the local file
    nsCStringKey key(spec.get());
    if (!mURIMap.Exists(&key))
    {
        return NS_ERROR_FAILURE;
    }
    URIData *data = (URIData *) mURIMap.Get(&key);
    if (!data->mNeedsFixup)
    {
        return NS_OK;
    }
    nsCOMPtr<nsIURI> fileAsURI;
    if (data->mFile)
    {
        rv = data->mFile->Clone(getter_AddRefs(fileAsURI)); 
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
        rv = data->mDataPath->Clone(getter_AddRefs(fileAsURI));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = AppendPathToURI(fileAsURI, data->mFilename);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    nsAutoString newValue;

    // remove username/password if present
    fileAsURI->SetUserPass(EmptyCString());

    // reset node attribute 
    // Use relative or absolute links
    if (data->mDataPathIsRelative)
    {
        nsCOMPtr<nsIURL> url(do_QueryInterface(fileAsURI));
        if (!url)
          return NS_ERROR_FAILURE;
          
        nsAutoCString filename;
        url->GetFileName(filename);

        nsAutoCString rawPathURL(data->mRelativePathToData);
        rawPathURL.Append(filename);

        nsAutoCString buf;
        AppendUTF8toUTF16(NS_EscapeURL(rawPathURL, esc_FilePath, buf),
                          newValue);
    }
    else
    {
        nsAutoCString fileurl;
        fileAsURI->GetSpec(fileurl);
        AppendUTF8toUTF16(fileurl, newValue);
    }
    if (data->mIsSubFrame)
    {
        newValue.Append(data->mSubFrameExt);
    }

    aURI = newValue;
    return NS_OK;
}

nsresult
nsWebBrowserPersist::FixupNodeAttributeNS(nsIDOMNode *aNode,
                                          const char *aNamespaceURI,
                                          const char *aAttribute)
{
    NS_ENSURE_ARG_POINTER(aNode);
    NS_ENSURE_ARG_POINTER(aNamespaceURI);
    NS_ENSURE_ARG_POINTER(aAttribute);

    // Find the named URI attribute on the (element) node and change it to reference
    // a local file.

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
    MOZ_ASSERT(element);

    nsCOMPtr<nsIDOMMozNamedAttrMap> attrMap;
    nsresult rv = element->GetAttributes(getter_AddRefs(attrMap));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    NS_ConvertASCIItoUTF16 attribute(aAttribute);
    NS_ConvertASCIItoUTF16 namespaceURI(aNamespaceURI);
    nsCOMPtr<nsIDOMAttr> attr;
    rv = attrMap->GetNamedItemNS(namespaceURI, attribute, getter_AddRefs(attr));
    if (attr) {
        nsString uri;
        attr->GetValue(uri);
        rv = FixupURI(uri);
        if (NS_SUCCEEDED(rv))
        {
            attr->SetValue(uri);
        }
    }

    return rv;
}

nsresult
nsWebBrowserPersist::FixupAnchor(nsIDOMNode *aNode)
{
    NS_ENSURE_ARG_POINTER(aNode);

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
    MOZ_ASSERT(element);

    nsCOMPtr<nsIDOMMozNamedAttrMap> attrMap;
    nsresult rv = element->GetAttributes(getter_AddRefs(attrMap));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (mPersistFlags & PERSIST_FLAGS_DONT_FIXUP_LINKS)
    {
        return NS_OK;
    }

    // Make all anchor links absolute so they point off onto the Internet
    nsString attribute(NS_LITERAL_STRING("href"));
    nsCOMPtr<nsIDOMAttr> attr;
    rv = attrMap->GetNamedItem(attribute, getter_AddRefs(attr));
    if (attr)
    {
        nsString oldValue;
        attr->GetValue(oldValue);
        NS_ConvertUTF16toUTF8 oldCValue(oldValue);

        // Skip empty values and self-referencing bookmarks
        if (oldCValue.IsEmpty() || oldCValue.CharAt(0) == '#')
        {
            return NS_OK;
        }

        // if saving file to same location, we don't need to do any fixup
        bool isEqual = false;
        if (NS_SUCCEEDED(mCurrentBaseURI->Equals(mTargetBaseURI, &isEqual))
            && isEqual)
        {
            return NS_OK;
        }

        nsCOMPtr<nsIURI> relativeURI;
        relativeURI = (mPersistFlags & PERSIST_FLAGS_FIXUP_LINKS_TO_DESTINATION)
                      ? mTargetBaseURI : mCurrentBaseURI;
        // Make a new URI to replace the current one
        nsCOMPtr<nsIURI> newURI;
        rv = NS_NewURI(getter_AddRefs(newURI), oldCValue, 
                       mCurrentCharset.get(), relativeURI);
        if (NS_SUCCEEDED(rv) && newURI)
        {
            newURI->SetUserPass(EmptyCString());
            nsAutoCString uriSpec;
            newURI->GetSpec(uriSpec);
            attr->SetValue(NS_ConvertUTF8toUTF16(uriSpec));
        }
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::StoreAndFixupStyleSheet(nsIStyleSheet *aStyleSheet)
{
    // TODO go through the style sheet fixing up all links
    return NS_OK;
}

bool
nsWebBrowserPersist::DocumentEncoderExists(const PRUnichar *aContentType)
{
    // Check if there is an encoder for the desired content type.
    nsAutoCString contractID(NS_DOC_ENCODER_CONTRACTID_BASE);
    AppendUTF16toUTF8(aContentType, contractID);

    nsCOMPtr<nsIComponentRegistrar> registrar;
    NS_GetComponentRegistrar(getter_AddRefs(registrar));
    if (registrar)
    {
        bool result;
        nsresult rv = registrar->IsContractIDRegistered(contractID.get(),
                                                        &result);
        if (NS_SUCCEEDED(rv) && result)
        {
            return true;
        }
    }
    return false;
}

nsresult
nsWebBrowserPersist::SaveSubframeContent(
    nsIDOMDocument *aFrameContent, URIData *aData)
{
    NS_ENSURE_ARG_POINTER(aData);

    // Extract the content type for the frame's contents.
    nsCOMPtr<nsIDocument> frameDoc(do_QueryInterface(aFrameContent));
    NS_ENSURE_STATE(frameDoc);

    nsAutoString contentType;
    nsresult rv = frameDoc->GetContentType(contentType);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLString ext;
    GetExtensionForContentType(contentType.get(), getter_Copies(ext));

    // We must always have an extension so we will try to re-assign
    // the original extension if GetExtensionForContentType fails.
    if (ext.IsEmpty())
    {
        nsCOMPtr<nsIURL> url(do_QueryInterface(frameDoc->GetDocumentURI(),
                                               &rv));
        nsAutoCString extension;
        if (NS_SUCCEEDED(rv))
        {
            url->GetFileExtension(extension);
        }
        else
        {
            extension.AssignLiteral("htm");
        }
        aData->mSubFrameExt.Assign(PRUnichar('.'));
        AppendUTF8toUTF16(extension, aData->mSubFrameExt);
    }
    else
    {
        aData->mSubFrameExt.Assign(PRUnichar('.'));
        aData->mSubFrameExt.Append(ext);
    }

    nsString filenameWithExt = aData->mFilename;
    filenameWithExt.Append(aData->mSubFrameExt);

    // Work out the path for the subframe
    nsCOMPtr<nsIURI> frameURI;
    rv = mCurrentDataPath->Clone(getter_AddRefs(frameURI));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AppendPathToURI(frameURI, filenameWithExt);
    NS_ENSURE_SUCCESS(rv, rv);

    // Work out the path for the subframe data
    nsCOMPtr<nsIURI> frameDataURI;
    rv = mCurrentDataPath->Clone(getter_AddRefs(frameDataURI));
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoString newFrameDataPath(aData->mFilename);

    // Append _data
    newFrameDataPath.AppendLiteral("_data");
    rv = AppendPathToURI(frameDataURI, newFrameDataPath);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make frame document & data path conformant and unique
    rv = CalculateUniqueFilename(frameURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = CalculateUniqueFilename(frameDataURI);
    NS_ENSURE_SUCCESS(rv, rv);

    mCurrentThingsToPersist++;

    // We shouldn't use SaveDocumentInternal for the contents
    // of frames that are not documents, e.g. images.
    if (DocumentEncoderExists(contentType.get()))
    {
        rv = SaveDocumentInternal(aFrameContent, frameURI, frameDataURI);
    }
    else
    {
        rv = StoreURI(frameDoc->GetDocumentURI());
    }
    NS_ENSURE_SUCCESS(rv, rv);

    // Store the updated uri to the frame
    aData->mFile = frameURI;
    aData->mSubFrameExt.Truncate(); // we already put this in frameURI

    return NS_OK;
}

nsresult
nsWebBrowserPersist::CreateChannelFromURI(nsIURI *aURI, nsIChannel **aChannel)
{
    nsresult rv = NS_OK;
    *aChannel = nullptr;

    nsCOMPtr<nsIIOService> ioserv;
    ioserv = do_GetIOService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ioserv->NewChannelFromURI(aURI, aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(*aChannel);

    rv = (*aChannel)->SetNotificationCallbacks(static_cast<nsIInterfaceRequestor *>(this));
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
} 

nsresult
nsWebBrowserPersist::SaveDocumentWithFixup(
    nsIDOMDocument *aDocument, nsIDocumentEncoderNodeFixup *aNodeFixup,
    nsIURI *aFile, bool aReplaceExisting, const nsACString &aFormatType,
    const nsCString &aSaveCharset, uint32_t aFlags)
{
    NS_ENSURE_ARG_POINTER(aFile);
    
    nsresult  rv = NS_OK;
    nsCOMPtr<nsIFile> localFile;
    GetLocalFileFromURI(aFile, getter_AddRefs(localFile));
    if (localFile)
    {
        // if we're not replacing an existing file but the file
        // exists, something is wrong
        bool fileExists = false;
        rv = localFile->Exists(&fileExists);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        if (!aReplaceExisting && fileExists)
            return NS_ERROR_FAILURE;                // where are the file I/O errors?
    }
    
    nsCOMPtr<nsIOutputStream> outputStream;
    rv = MakeOutputStream(aFile, getter_AddRefs(outputStream));
    if (NS_FAILED(rv))
    {
        SendErrorStatusChange(false, rv, nullptr, aFile);
        return NS_ERROR_FAILURE;
    }
    NS_ENSURE_TRUE(outputStream, NS_ERROR_FAILURE);

    // Get a document encoder instance
    nsAutoCString contractID(NS_DOC_ENCODER_CONTRACTID_BASE);
    contractID.Append(aFormatType);
    
    nsCOMPtr<nsIDocumentEncoder> encoder = do_CreateInstance(contractID.get(), &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    NS_ConvertASCIItoUTF16 newContentType(aFormatType);
    rv = encoder->Init(aDocument, newContentType, aFlags);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    mTargetBaseURI = aFile;

    // Set the node fixup callback
    encoder->SetNodeFixup(aNodeFixup);

    if (mWrapColumn && (aFlags & ENCODE_FLAGS_WRAP))
        encoder->SetWrapColumn(mWrapColumn);

    nsAutoCString charsetStr(aSaveCharset);
    if (charsetStr.IsEmpty())
    {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
        NS_ASSERTION(doc, "Need a document");
        charsetStr = doc->GetDocumentCharacterSet();
    }

    rv = encoder->SetCharset(charsetStr);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    rv = encoder->EncodeToStream(outputStream);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    
    if (!localFile)
    {
        nsCOMPtr<nsIStorageStream> storStream(do_QueryInterface(outputStream));
        if (storStream)
        {
            outputStream->Close();
            rv = StartUpload(storStream, aFile, aFormatType);
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
        }
    }
#if defined(XP_OS2)
    else
    {
        // close the stream, then tag the file it created with its source URI
        outputStream->Close();
        nsCOMPtr<nsILocalFileOS2> localFileOS2 = do_QueryInterface(localFile);
        if (localFileOS2)
        {
            nsAutoCString url;
            mCurrentBaseURI->GetSpec(url);
            localFileOS2->SetFileSource(url);
        }
    }
#endif

    return rv;
}


// we store the current location as the key (absolutized version of domnode's attribute's value)
nsresult
nsWebBrowserPersist::MakeAndStoreLocalFilenameInURIMap(
    nsIURI *aURI, bool aNeedsPersisting, URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aURI);

    nsAutoCString spec;
    nsresult rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Create a sensibly named filename for the URI and store in the URI map
    nsCStringKey key(spec.get());
    URIData *data;
    if (mURIMap.Exists(&key))
    {
        data = (URIData *) mURIMap.Get(&key);
        if (aNeedsPersisting)
        {
          data->mNeedsPersisting = true;
        }
        if (aData)
        {
            *aData = data;
        }
        return NS_OK;
    }

    // Create a unique file name for the uri
    nsString filename;
    rv = MakeFilenameFromURI(aURI, filename);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Store the file name
    data = new URIData;
    NS_ENSURE_TRUE(data, NS_ERROR_OUT_OF_MEMORY);

    data->mNeedsPersisting = aNeedsPersisting;
    data->mNeedsFixup = true;
    data->mFilename = filename;
    data->mSaved = false;
    data->mIsSubFrame = false;
    data->mDataPath = mCurrentDataPath;
    data->mDataPathIsRelative = mCurrentDataPathIsRelative;
    data->mRelativePathToData = mCurrentRelativePathToData;
    data->mCharset = mCurrentCharset;

    if (aNeedsPersisting)
        mCurrentThingsToPersist++;

    mURIMap.Put(&key, data);
    if (aData)
    {
        *aData = data;
    }

    return NS_OK;
}

// Ordered so that typical documents work fastest.
//                                    strlen("blockquote")==10
static const char kSpecialXHTMLTags[][11] = {
    "body",
    "head",
    "img",
    "script",
    "a",
    "area",
    "link",
    "input",
    "frame",
    "iframe",
    "object",
    "applet",
    "form",
    "blockquote",
    "q",
    "del",
    "ins"
};

static bool IsSpecialXHTMLTag(nsIDOMNode *aNode)
{
    nsAutoString tmp;
    aNode->GetNamespaceURI(tmp);
    if (!tmp.EqualsLiteral("http://www.w3.org/1999/xhtml"))
        return false;

    aNode->GetLocalName(tmp);
    for (uint32_t i = 0; i < ArrayLength(kSpecialXHTMLTags); i++) {
        if (tmp.EqualsASCII(kSpecialXHTMLTags[i]))
        {
            // XXX This element MAY have URI attributes, but
            //     we are not actually checking if they are present.
            //     That would slow us down further, and I am not so sure
            //     how important that would be.
            return true;
        }
    }

    return false;
}

static bool HasSpecialXHTMLTags(nsIDOMNode *aParent)
{
    if (IsSpecialXHTMLTag(aParent))
        return true;

    nsCOMPtr<nsIDOMNodeList> list;
    aParent->GetChildNodes(getter_AddRefs(list));
    if (list)
    {
        uint32_t count;
        list->GetLength(&count);
        uint32_t i;
        for (i = 0; i < count; i++) {
            nsCOMPtr<nsIDOMNode> node;
            list->Item(i, getter_AddRefs(node));
            if (!node)
                break;
            uint16_t nodeType;
            node->GetNodeType(&nodeType);
            if (nodeType == nsIDOMNode::ELEMENT_NODE) {
                return HasSpecialXHTMLTags(node);
            }
        }
    }

    return false;
}

static bool NeedXHTMLBaseTag(nsIDOMDocument *aDocument)
{
    nsCOMPtr<nsIDOMElement> docElement;
    aDocument->GetDocumentElement(getter_AddRefs(docElement));

    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(docElement));
    if (node)
    {
        return HasSpecialXHTMLTags(node);
    }

    return false;
}

// Set document base. This could create an invalid XML document (still well-formed).
nsresult
nsWebBrowserPersist::SetDocumentBase(
    nsIDOMDocument *aDocument, nsIURI *aBaseURI)
{
    if (mPersistFlags & PERSIST_FLAGS_NO_BASE_TAG_MODIFICATIONS)
    {
        return NS_OK;
    }

    NS_ENSURE_ARG_POINTER(aBaseURI);

    nsCOMPtr<nsIDOMXMLDocument> xmlDoc;
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(aDocument);
    if (!htmlDoc)
    {
        xmlDoc = do_QueryInterface(aDocument);
        if (!xmlDoc)
        {
            return NS_ERROR_FAILURE;
        }
    }

    NS_NAMED_LITERAL_STRING(kXHTMLNS, "http://www.w3.org/1999/xhtml");
    NS_NAMED_LITERAL_STRING(kHead, "head");

    // Find the head element
    nsCOMPtr<nsIDOMElement> headElement;
    nsCOMPtr<nsIDOMNodeList> headList;
    if (xmlDoc)
    {
        // First see if there is XHTML content that needs base 
        // tags.
        if (!NeedXHTMLBaseTag(aDocument))
            return NS_OK;

        aDocument->GetElementsByTagNameNS(
            kXHTMLNS,
            kHead, getter_AddRefs(headList));
    }
    else
    {
        aDocument->GetElementsByTagName(
            kHead, getter_AddRefs(headList));
    }
    if (headList)
    {
        nsCOMPtr<nsIDOMNode> headNode;
        headList->Item(0, getter_AddRefs(headNode));
        headElement = do_QueryInterface(headNode);
    }
    if (!headElement)
    {
        // Create head and insert as first element
        nsCOMPtr<nsIDOMNode> firstChildNode;
        nsCOMPtr<nsIDOMNode> newNode;
        if (xmlDoc)
        {
            aDocument->CreateElementNS(
                kXHTMLNS,
                kHead, getter_AddRefs(headElement));
        }
        else
        {
            aDocument->CreateElement(
                kHead, getter_AddRefs(headElement));
        }
        nsCOMPtr<nsIDOMElement> documentElement;
        aDocument->GetDocumentElement(getter_AddRefs(documentElement));
        if (documentElement)
        {
            documentElement->GetFirstChild(getter_AddRefs(firstChildNode));
            documentElement->InsertBefore(headElement, firstChildNode, getter_AddRefs(newNode));
        }
    }
    if (!headElement)
    {
        return NS_ERROR_FAILURE;
    }

    // Find or create the BASE element
    NS_NAMED_LITERAL_STRING(kBase, "base");
    nsCOMPtr<nsIDOMElement> baseElement;
    nsCOMPtr<nsIDOMHTMLCollection> baseList;
    if (xmlDoc)
    {
        headElement->GetElementsByTagNameNS(
            kXHTMLNS,
            kBase, getter_AddRefs(baseList));
    }
    else
    {
        headElement->GetElementsByTagName(
            kBase, getter_AddRefs(baseList));
    }
    if (baseList)
    {
        nsCOMPtr<nsIDOMNode> baseNode;
        baseList->Item(0, getter_AddRefs(baseNode));
        baseElement = do_QueryInterface(baseNode);
    }

    // Add the BASE element
    if (!baseElement)
    {
      nsCOMPtr<nsIDOMNode> newNode;
      if (xmlDoc)
      {
          aDocument->CreateElementNS(
              kXHTMLNS,
              kBase, getter_AddRefs(baseElement));
      }
      else
      {
          aDocument->CreateElement(
              kBase, getter_AddRefs(baseElement));
      }
      headElement->AppendChild(baseElement, getter_AddRefs(newNode));
    }
    if (!baseElement)
    {
        return NS_ERROR_FAILURE;
    }
    nsAutoCString uriSpec;
    aBaseURI->GetSpec(uriSpec);
    NS_ConvertUTF8toUTF16 href(uriSpec);
    baseElement->SetAttribute(NS_LITERAL_STRING("href"), href);

    return NS_OK;
}

// Decide if we need to apply conversion to the passed channel.
void nsWebBrowserPersist::SetApplyConversionIfNeeded(nsIChannel *aChannel)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIEncodedChannel> encChannel = do_QueryInterface(aChannel, &rv);
    if (NS_FAILED(rv))
        return;

    // Set the default conversion preference:
    encChannel->SetApplyConversion(false);

    nsCOMPtr<nsIURI> thisURI;
    aChannel->GetURI(getter_AddRefs(thisURI));
    nsCOMPtr<nsIURL> sourceURL(do_QueryInterface(thisURI));
    if (!sourceURL)
        return;
    nsAutoCString extension;
    sourceURL->GetFileExtension(extension);

    nsCOMPtr<nsIUTF8StringEnumerator> encEnum;
    encChannel->GetContentEncodings(getter_AddRefs(encEnum));
    if (!encEnum)
        return;
    nsCOMPtr<nsIExternalHelperAppService> helperAppService =
        do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return;
    bool hasMore;
    rv = encEnum->HasMore(&hasMore);
    if (NS_SUCCEEDED(rv) && hasMore)
    {
        nsAutoCString encType;
        rv = encEnum->GetNext(encType);
        if (NS_SUCCEEDED(rv))
        {
            bool applyConversion = false;
            rv = helperAppService->ApplyDecodingForExtension(extension, encType,
                                                             &applyConversion);
            if (NS_SUCCEEDED(rv))
                encChannel->SetApplyConversion(applyConversion);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////


nsEncoderNodeFixup::nsEncoderNodeFixup() : mWebBrowserPersist(nullptr)
{
}


nsEncoderNodeFixup::~nsEncoderNodeFixup()
{
}


NS_IMPL_ADDREF(nsEncoderNodeFixup)
NS_IMPL_RELEASE(nsEncoderNodeFixup)


NS_INTERFACE_MAP_BEGIN(nsEncoderNodeFixup)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocumentEncoderNodeFixup)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentEncoderNodeFixup)
NS_INTERFACE_MAP_END


NS_IMETHODIMP nsEncoderNodeFixup::FixupNode(
    nsIDOMNode *aNode, bool *aSerializeCloneKids, nsIDOMNode **aOutNode)
{
    NS_ENSURE_ARG_POINTER(aNode);
    NS_ENSURE_ARG_POINTER(aOutNode);
    NS_ENSURE_TRUE(mWebBrowserPersist, NS_ERROR_FAILURE);

    *aOutNode = nullptr;
    
    // Test whether we need to fixup the node
    uint16_t type = 0;
    aNode->GetNodeType(&type);
    if (type == nsIDOMNode::ELEMENT_NODE ||
        type == nsIDOMNode::PROCESSING_INSTRUCTION_NODE)
    {
        return mWebBrowserPersist->CloneNodeWithFixedUpAttributes(aNode, aSerializeCloneKids, aOutNode);
    }

    return NS_OK;
}
