/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *   Kathleen Brade <brade@netscape.com>
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

#include "nspr.h"

#include "nsIFileStreams.h"       // New Necko file streams

#ifdef XP_MAC
#include "nsILocalFileMac.h"
#endif

#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsIStorageStream.h"
#include "nsISeekableStream.h"
#include "nsIHttpChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIUploadChannel.h"
#include "nsICachingChannel.h"
#include "nsEscape.h"
#include "nsUnicharUtils.h"
#include "nsIStringEnumerator.h"
#include "nsCRT.h"
#include "nsSupportsArray.h"

#include "nsCExternalHandlerService.h"

#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMNode.h"
#include "nsIDOMComment.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNSDocument.h"
#include "nsIWebProgressListener.h"
#include "nsIAuthPrompt.h"
#include "nsIPrompt.h"
#include "nsISHEntry.h"
#include "nsIWebPageDescriptor.h"

#include "nsIDOMNodeFilter.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLDocument.h"

#include "ftpCore.h"
#include "nsITransport.h"
#include "nsISocketTransport.h"
#include "nsIStringBundle.h"

#include "nsWebBrowserPersist.h"


#define NS_SUCCESS_DONT_FIXUP NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 1)

// Information about a DOM document
struct DocData
{
    nsCOMPtr<nsIURI> mBaseURI;
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIURI> mFile;
    nsCOMPtr<nsIURI> mDataPath;
    PRPackedBool mDataPathIsRelative;
    nsCString mRelativePathToData;
    nsCString mCharset;
};

// Information about a URI
struct URIData
{
    PRPackedBool mNeedsPersisting;
    PRPackedBool mSaved;
    PRPackedBool mIsSubFrame;
    PRPackedBool mDataPathIsRelative;
    PRPackedBool mNeedsFixup;
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
    PRInt32 mSelfProgress;
    PRInt32 mSelfProgressMax;
    PRPackedBool mCalcFileExt;

    OutputData(nsIURI *aFile, nsIURI *aOriginalLocation, PRBool aCalcFileExt) :
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
    PRInt32 mSelfProgress;
    PRInt32 mSelfProgressMax;

    UploadData(nsIURI *aFile) :
        mFile(aFile),
        mSelfProgress(0),
        mSelfProgressMax(10000)
    {
    }
};

struct CleanupData
{
    nsCOMPtr<nsILocalFile> mFile;
    // Snapshot of what the file actually is at the time of creation so that if
    // it transmutes into something else later on it can be ignored. For example,
    // catch files that turn into dirs or vice versa.
    PRPackedBool mIsDirectory;
};

// Maximum file length constant. The max file name length is
// volume / server dependent but it is difficult to obtain
// that information. Instead this constant is a reasonable value that
// modern systems should able to cope with.

#ifdef XP_MAC
const PRUint32 kDefaultMaxFilenameLength = 31;
#else
const PRUint32 kDefaultMaxFilenameLength = 64;
#endif

// Schemes that cannot be saved because they contain no useful content
const char *kNonpersistableSchemes[] = {
    "about:",
    "news:", 
    "snews:",
    "ldap:",
    "ldaps:",
    "mailto:", 
    "finger:",
    "telnet:", 
    "gopher:", 
    "javascript:",
    "view-source:",
    "irc:",
    "mailbox:"
};
const PRUint32 kNonpersistableSchemesSize = sizeof(kNonpersistableSchemes) / sizeof(kNonpersistableSchemes[0]);

// Default flags for persistence
const PRUint32 kDefaultPersistFlags = 
    nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION |
    nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;

// String bundle where error messages come from
const char *kWebBrowserPersistStringBundle =
    "chrome://global/locale/nsWebBrowserPersist.properties";

nsWebBrowserPersist::nsWebBrowserPersist() :
    mCurrentThingsToPersist(0),
    mFirstAndOnlyUse(PR_TRUE),
    mCancel(PR_FALSE),
    mJustStartedLoading(PR_TRUE),
    mCompleted(PR_FALSE),
    mStartSaving(PR_FALSE),
    mReplaceExisting(PR_TRUE),
    mSerializingOutput(PR_FALSE),
    mPersistFlags(kDefaultPersistFlags),
    mPersistResult(NS_OK),
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

    *aIFace = nsnull;

    nsresult rv = QueryInterface(aIID, aIFace);
    if (NS_SUCCEEDED(rv) && *aIFace)
    {
        return rv;
    }
    
    if (mProgressListener && (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) 
                             || aIID.Equals(NS_GET_IID(nsIPrompt))))
    {
        nsCOMPtr<nsISupports> sup = do_QueryInterface(mProgressListener);
        if (sup)
        {
            sup->QueryInterface(aIID, aIFace);
            if (*aIFace)
                return NS_OK;
        }

        nsCOMPtr<nsIInterfaceRequestor> req = do_QueryInterface(mProgressListener);
        if (req)
        {
            req->GetInterface(aIID, aIFace);
            if (*aIFace)
            {
                return NS_OK;
            }
        }
    }

    return NS_ERROR_NO_INTERFACE;
}


//*****************************************************************************
// nsWebBrowserPersist::nsIWebBrowserPersist
//*****************************************************************************

/* attribute unsigned long persistFlags; */
NS_IMETHODIMP nsWebBrowserPersist::GetPersistFlags(PRUint32 *aPersistFlags)
{
    NS_ENSURE_ARG_POINTER(aPersistFlags);
    *aPersistFlags = mPersistFlags;
    return NS_OK;
}
NS_IMETHODIMP nsWebBrowserPersist::SetPersistFlags(PRUint32 aPersistFlags)
{
    mPersistFlags = aPersistFlags;
    mReplaceExisting = (mPersistFlags & PERSIST_FLAGS_REPLACE_EXISTING_FILES) ? PR_TRUE : PR_FALSE;
    mSerializingOutput = (mPersistFlags & PERSIST_FLAGS_SERIALIZE_OUTPUT) ? PR_TRUE : PR_FALSE;
    return NS_OK;
}

/* readonly attribute unsigned long currentState; */
NS_IMETHODIMP nsWebBrowserPersist::GetCurrentState(PRUint32 *aCurrentState)
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
NS_IMETHODIMP nsWebBrowserPersist::GetResult(PRUint32 *aResult)
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
    return NS_OK;
}

/* void saveURI (in nsIURI aURI, in nsISupports aCacheKey, in nsIURI aReferrer,
   in nsIInputStream aPostData, in wstring aExtraHeaders,
   in nsISupports aFile); */
NS_IMETHODIMP nsWebBrowserPersist::SaveURI(
    nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer, nsIInputStream *aPostData, const char *aExtraHeaders, nsISupports *aFile)
{
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);
    mFirstAndOnlyUse = PR_FALSE; // Stop people from reusing this object!

    nsCOMPtr<nsIURI> fileAsURI;
    nsresult rv;
    rv = GetValidURIFromObject(aFile, getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

    // SaveURI doesn't like broken uris.
    mPersistFlags |= PERSIST_FLAGS_FAIL_ON_BROKEN_LINKS;
    rv = SaveURIInternal(aURI, aCacheKey, aReferrer, aPostData, aExtraHeaders, fileAsURI, PR_FALSE);
    return NS_FAILED(rv) ? rv : NS_OK;
}

/* void saveChannel (in nsIChannel aChannel, in nsISupports aFile); */
NS_IMETHODIMP nsWebBrowserPersist::SaveChannel(
    nsIChannel *aChannel, nsISupports *aFile)
{
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);
    mFirstAndOnlyUse = PR_FALSE; // Stop people from reusing this object!

    nsCOMPtr<nsIURI> fileAsURI;
    nsresult rv;
    rv = GetValidURIFromObject(aFile, getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

    rv = aChannel->GetURI(getter_AddRefs(mURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // SaveURI doesn't like broken uris.
    mPersistFlags |= PERSIST_FLAGS_FAIL_ON_BROKEN_LINKS;
    rv = SaveChannelInternal(aChannel, fileAsURI, PR_FALSE);
    return NS_FAILED(rv) ? rv : NS_OK;
}


/* void saveDocument (in nsIDOMDocument aDocument, in nsIURI aFileURI,
   in nsIURI aDataPathURI, in string aOutputContentType,
   in unsigned long aEncodingFlags, in unsigned long aWrapColumn); */
NS_IMETHODIMP nsWebBrowserPersist::SaveDocument(
    nsIDOMDocument *aDocument, nsISupports *aFile, nsISupports *aDataPath,
    const char *aOutputContentType, PRUint32 aEncodingFlags, PRUint32 aWrapColumn)
{
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);
    mFirstAndOnlyUse = PR_FALSE; // Stop people from reusing this object!

    nsCOMPtr<nsIURI> fileAsURI;
    nsCOMPtr<nsIURI> datapathAsURI;
    nsresult rv;

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
        mContentType.AssignWithConversion(aOutputContentType);
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
        mProgressListener->OnStateChange(nsnull, nsnull,
                                         nsIWebProgressListener::STATE_START,
                                         NS_OK);
        mProgressListener->OnStateChange(nsnull, nsnull,
                                         nsIWebProgressListener::STATE_STOP,
                                         rv);
    }

    return rv;
}

/* void cancelSave(); */
NS_IMETHODIMP nsWebBrowserPersist::CancelSave()
{
    mCancel = PR_TRUE;
    EndDownload(NS_BINDING_ABORTED);
    return NS_OK;
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

    nsCOMPtr<nsIChannel> destChannel;
    rv = CreateChannelFromURI(aDestinationURI, getter_AddRefs(destChannel));
    nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(destChannel));
    NS_ENSURE_TRUE(uploadChannel, NS_ERROR_FAILURE);

    // Set the upload stream
    // NOTE: ALL data must be available in "inputstream"
    rv = uploadChannel->SetUploadStream(inputstream, aContentType, -1);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    rv = destChannel->AsyncOpen(this, nsnull);
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
    PRUint32 urisToPersist = 0;
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
        PRUint32 addToStateFlags = 0;
        if (mProgressListener)
        {
            if (mJustStartedLoading)
            {
                addToStateFlags |= nsIWebProgressListener::STATE_IS_NETWORK;
            }
            mProgressListener->OnStateChange(nsnull, nsnull,
                nsIWebProgressListener::STATE_START | addToStateFlags, NS_OK);
        }

        rv = SaveDocuments();
        if (NS_FAILED(rv))
            EndDownload(rv);
        else if (aFileAsURI)
        {
            // local files won't trigger OnStopRequest so we call EndDownload here
            PRBool isFile = PR_FALSE;
            aFileAsURI->SchemeIs("file", &isFile);
            if (isFile)
                EndDownload(NS_OK);
        }

        // State stop notification
        if (mProgressListener)
        {
            mProgressListener->OnStateChange(nsnull, nsnull,
                nsIWebProgressListener::STATE_STOP | addToStateFlags, rv);
        }
    }

    return rv;
}

// this method returns true if there is another file to persist and false if not
PRBool
nsWebBrowserPersist::SerializeNextFile()
{
    if (!mSerializingOutput)
    {
        return PR_FALSE;
    }

    nsresult rv = SaveGatheredURIs(nsnull);
    if (NS_FAILED(rv))
    {
        return PR_FALSE;
    }

    return (mURIMap.Count() 
        || mUploadList.Count()
        || mDocList.Count()
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
        PRUint32 stateFlags = nsIWebProgressListener::STATE_START |
                              nsIWebProgressListener::STATE_IS_REQUEST;
        if (mJustStartedLoading)
        {
            stateFlags |= nsIWebProgressListener::STATE_IS_NETWORK;
        }
        mProgressListener->OnStateChange(nsnull, request, stateFlags, NS_OK);
    }

    mJustStartedLoading = PR_FALSE;

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
        PRBool isEqual = PR_FALSE;
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
        // This will close automatically close the output stream
        delete data;
        mOutputMap.Remove(&key);
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

    PRBool completed = PR_FALSE;
    if (mOutputMap.Count() == 0 && mUploadList.Count() == 0 && !mCancel)
    {
        // if no documents left in mDocList, --> done
        // if we have no files left to serialize and no error result, --> done
        if (mDocList.Count() == 0
            || (!SerializeNextFile() && NS_SUCCEEDED(mPersistResult)))
        {
            completed = PR_TRUE;
        }
    }

    if (completed)
    {
        // we're all done, do our cleanup
        EndDownload(NS_OK);
    }

    if (mProgressListener)
    {
        PRUint32 stateFlags = nsIWebProgressListener::STATE_STOP |
                              nsIWebProgressListener::STATE_IS_REQUEST;
        if (completed)
        {
            stateFlags |= nsIWebProgressListener::STATE_IS_NETWORK;
        }
        mProgressListener->OnStateChange(nsnull, request, stateFlags, status);
    }

    return NS_OK;
}

//*****************************************************************************
// nsWebBrowserPersist::nsIStreamListener
//*****************************************************************************

static NS_METHOD DiscardSegments(nsIInputStream *input,
                                 void *closure,
                                 const char *buf,
                                 PRUint32 offset,
                                 PRUint32 count,
                                 PRUint32 *countRead)
{
    *countRead = count;
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserPersist::OnDataAvailable(
    nsIRequest* request, nsISupports *aContext, nsIInputStream *aIStream,
    PRUint32 aOffset, PRUint32 aLength)
{
    PRBool cancel = mCancel;
    if (!cancel)
    {
        nsresult rv = NS_OK;
        PRUint32 bytesRemaining = aLength;

        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

        nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(request);
        nsISupportsKey key(keyPtr);
        OutputData *data = (OutputData *) mOutputMap.Get(&key);
        if (!data) {
            // might be uploadData; consume necko's buffer and bail...
            PRUint32 n;
            return aIStream->ReadSegments(DiscardSegments, nsnull, aLength, &n);
        }

        PRBool readError = PR_TRUE;

        // Make the output stream
        if (!data->mStream)
        {
            rv = MakeOutputStream(data->mFile, getter_AddRefs(data->mStream));
            if (NS_FAILED(rv))
            {
                readError = PR_FALSE;
                cancel = PR_TRUE;
            }
        }

        // Read data from the input and write to the output
        char buffer[8192];
        PRUint32 bytesRead;
        while (!cancel && bytesRemaining)
        {
            readError = PR_TRUE;
            rv = aIStream->Read(buffer, PR_MIN(sizeof(buffer), bytesRemaining), &bytesRead);
            if (NS_SUCCEEDED(rv))
            {
                readError = PR_FALSE;
                // Write out the data until something goes wrong, or, it is
                // all written.  We loop because for some errors (e.g., disk
                // full), we get NS_OK with some bytes written, then an error.
                // So, we want to write again in that case to get the actual
                // error code.
                const char *bufPtr = buffer; // Where to write from.
                while (NS_SUCCEEDED(rv) && bytesRead)
                {
                    PRUint32 bytesWritten = 0;
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
                            cancel = PR_TRUE;
                        }
                    }
                    else
                    {
                        // Disaster - can't write out the bytes - disk full / permission?
                        cancel = PR_TRUE;
                    }
                }
            }
            else
            {
                // Disaster - can't read the bytes - broken link / file error?
                cancel = PR_TRUE;
            }
        }

        PRInt32 channelContentLength = -1;
        if (!cancel &&
            NS_SUCCEEDED(channel->GetContentLength(&channelContentLength)))
        {
            // if we get -1 at this point, we didn't get content-length header
            // assume that we got all of the data and push what we have; 
            // that's the best we can do now
            if ((-1 == channelContentLength) ||
                ((channelContentLength - (aOffset + aLength)) == 0))
            {
                NS_ASSERTION(channelContentLength != -1, "no content length");
                // we're done with this pass; see if we need to do upload
                nsCAutoString contentType;
                channel->GetContentType(contentType);
                // if we don't have the right type of output stream then it's a local file
                nsCOMPtr<nsIStorageStream> storStream(do_QueryInterface(data->mStream));
                if (storStream)
                {
                    data->mStream->Close();
                    data->mStream = nsnull; // null out stream so we don't close it later
                    rv = StartUpload(storStream, data->mFile, contentType);
                    if (NS_FAILED(rv))
                    {
                        cancel = PR_TRUE;
                    }
                }
            }
        }

        // Notify listener if an error occurred.
        if (cancel)
        {
            SendErrorStatusChange(readError, rv,
                readError ? request : nsnull, data->mFile);
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
    in unsigned long aProgress, in unsigned long aProgressMax); */
NS_IMETHODIMP nsWebBrowserPersist::OnProgress(
    nsIRequest *request, nsISupports *ctxt, PRUint32 aProgress,
    PRUint32 aProgressMax)
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
        data->mSelfProgress = aProgress;
        data->mSelfProgressMax = aProgressMax;
    }
    else
    {
        UploadData *upData = (UploadData *) mUploadList.Get(&key);
        if (upData)
        {
            upData->mSelfProgress = aProgress;
            upData->mSelfProgressMax = aProgressMax;
        }
    }

    // Notify listener of total progress
    CalcTotalProgress();
    mProgressListener->OnProgressChange(nsnull, request, aProgress,
            aProgressMax, mTotalCurrentProgress, mTotalMaxProgress);

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
        case NS_NET_STATUS_BEGIN_FTP_TRANSACTION:
        case NS_NET_STATUS_END_FTP_TRANSACTION:
        case NS_NET_STATUS_CONNECTING_TO:
        case NS_NET_STATUS_CONNECTED_TO:
        case NS_NET_STATUS_SENDING_TO:
        case NS_NET_STATUS_RECEIVING_FROM:
        case NS_NET_STATUS_WAITING_FOR:
        case nsITransport::STATUS_READING:
        case nsITransport::STATUS_WRITING:
            break;

        default:
            // Pass other notifications (for legitimate errors) along.
            mProgressListener->OnStatusChange(nsnull, request, status, statusArg);
            break;
        }

    }
    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist private methods
//*****************************************************************************

// Convert error info into proper message text and send OnStatusChange notification
// to the web progress listener.
nsresult nsWebBrowserPersist::SendErrorStatusChange( 
    PRBool aIsReadError, nsresult aResult, nsIRequest *aRequest, nsIURI *aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);

    if (!mProgressListener)
    {
        // Do nothing
        return NS_OK;
    }

    // Get the file path or spec from the supplied URI
    nsCOMPtr<nsILocalFile> file;
    GetLocalFileFromURI(aURI, getter_AddRefs(file));
    nsAutoString path;
    if (file)
    {
        file->GetPath(path);
    }
    else
    {
        nsCAutoString fileurl;
        aURI->GetSpec(fileurl);
        AppendUTF8toUTF16(fileurl, path);
    }
    
    nsAutoString msgId;
    switch(aResult)
    {
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

    mProgressListener->OnStatusChange(nsnull, aRequest, aResult, msgText);

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

nsresult nsWebBrowserPersist::GetLocalFileFromURI(nsIURI *aURI, nsILocalFile **aLocalFile) const
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

    nsCAutoString newPath;
    nsresult rv = aURI->GetPath(newPath);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Append a forward slash if necessary
    PRInt32 len = newPath.Length();
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
    nsIURI *aFile, PRBool aCalcFileExt)
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
        nsCOMPtr<nsIWebPageDescriptor> webPageDescriptor = do_QueryInterface(aCacheKey);
        if (webPageDescriptor)
        {
            nsCOMPtr<nsISupports> currentDescriptor;
            webPageDescriptor->GetCurrentDescriptor(getter_AddRefs(currentDescriptor));
            if (currentDescriptor)
            {
                // Descriptor is actually a session history entry
                nsCOMPtr<nsISHEntry> shEntry = do_QueryInterface(currentDescriptor);
                NS_ASSERTION(shEntry, "The descriptor is meant to be a session history entry");
                if (shEntry)
                {
                    shEntry->GetCacheKey(getter_AddRefs(cacheKey));
                }
            }
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
            nsnull, nsnull, NS_STATIC_CAST(nsIInterfaceRequestor *, this),
            loadFlags);
    
    if (NS_FAILED(rv) || inputChannel == nsnull)
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
            encodedChannel->SetApplyConversion(PR_FALSE);
        }
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
            nsCAutoString oneHeader;
            nsCAutoString headerName;
            nsCAutoString headerValue;
            PRInt32 crlf = 0;
            PRInt32 colon = 0;
            const char *kWhitespace = "\b\t\r\n ";
            nsCAutoString extraHeaders(aExtraHeaders);
            while (PR_TRUE)
            {
                crlf = extraHeaders.Find("\r\n", PR_TRUE);
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
                rv = httpChannel->SetRequestHeader(headerName, headerValue, PR_TRUE);
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
    nsIChannel *aChannel, nsIURI *aFile, PRBool aCalcFileExt)
{
    NS_ENSURE_ARG_POINTER(aChannel);
    NS_ENSURE_ARG_POINTER(aFile);

    // Read from the input channel
    nsresult rv = aChannel->AsyncOpen(this, nsnull);
    if (rv == NS_ERROR_NO_CONTENT)
    {
        // Assume this is a protocol such as mailto: which does not feed out
        // data and just ignore it.
        return NS_SUCCESS_DONT_FIXUP;
    }
    else if (NS_FAILED(rv))
    {
        // Opening failed, but do we care?
        if (mPersistFlags & PERSIST_FLAGS_FAIL_ON_BROKEN_LINKS)
        {
            EndDownload(NS_ERROR_FAILURE);
            return NS_ERROR_FAILURE;
        }
        return NS_SUCCESS_DONT_FIXUP;
    }
    else
    {
        // Add the output transport to the output map with the channel as the key
        nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(aChannel);
        nsISupportsKey key(keyPtr);
        mOutputMap.Put(&key, new OutputData(aFile, mURI, aCalcFileExt));
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::GetExtensionForContentType(const PRUnichar *aContentType, PRUnichar **aExt)
{
    NS_ENSURE_ARG_POINTER(aContentType);
    NS_ENSURE_ARG_POINTER(aExt);

    *aExt = nsnull;

    nsresult rv;
    if (!mMIMEService)
    {
        mMIMEService = do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
        NS_ENSURE_TRUE(mMIMEService, NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsIMIMEInfo> mimeInfo;
    nsCAutoString contentType;
    contentType.AssignWithConversion(aContentType);
    nsCAutoString ext;
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
    nsresult rv = GetDocEncoderContentType(aDocument, nsnull, getter_Copies(contentType));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    return GetExtensionForContentType(contentType.get(), aExt);
}

nsresult
nsWebBrowserPersist::GetDocEncoderContentType(nsIDOMDocument *aDocument, const PRUnichar *aContentType, PRUnichar **aRealContentType)
{
    NS_ENSURE_ARG_POINTER(aDocument);
    NS_ENSURE_ARG_POINTER(aRealContentType);

    *aRealContentType = nsnull;

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
        nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(aDocument);
        if (nsDoc)
        {
            nsAutoString type;
            if (NS_SUCCEEDED(nsDoc->GetContentType(type)) && !type.IsEmpty())
            {
                contentType.Assign(type);
            }
        }
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
        nsCAutoString contractID(NS_DOC_ENCODER_CONTRACTID_BASE);
        contractID.AppendWithConversion(contentType);

        nsCID cid;
        nsresult rv = nsComponentManager::ContractIDToClassID(contractID.get(), &cid);
        if (NS_SUCCEEDED(rv))
        {
            *aRealContentType = ToNewUnicode(contentType);
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
    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv = GetLocalFileFromURI(aFile, getter_AddRefs(localFile));

    nsCOMPtr<nsILocalFile> localDataPath;
    if (NS_SUCCEEDED(rv) && aDataPath)
    {
        // See if we can get the local file representation of this URI
        rv = GetLocalFileFromURI(aDataPath, getter_AddRefs(localDataPath));
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsIDOMNode> docAsNode = do_QueryInterface(aDocument);

    // Persist the main document
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
    mURI = doc->GetDocumentURI();

    nsCOMPtr<nsIURI> oldBaseURI = mCurrentBaseURI;
    nsCAutoString oldCharset(mCurrentCharset);

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
        PRBool oldDataPathIsRelative = mCurrentDataPathIsRelative;
        nsCString oldCurrentRelativePathToData = mCurrentRelativePathToData;
        PRUint32 oldThingsToPersist = mCurrentThingsToPersist;

        mCurrentDataPathIsRelative = PR_FALSE;
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

            nsCAutoString relativePathToData;
            nsCOMPtr<nsIFile> dataDirParent;
            dataDirParent = localDataPath;
            while (dataDirParent)
            {
                PRBool sameDir = PR_FALSE;
                dataDirParent->Equals(baseDir, &sameDir);
                if (sameDir)
                {
                    mCurrentRelativePathToData = relativePathToData;
                    mCurrentDataPathIsRelative = PR_TRUE;
                    break;
                }

                nsCAutoString dirName;
                dataDirParent->GetNativeLeafName(dirName);

                nsCAutoString newRelativePathToData;
                newRelativePathToData = dirName
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
                nsCAutoString relativePath;  // nsACString
                if (NS_SUCCEEDED(pathToBaseURL->GetRelativeSpec(aDataPath, relativePath)))
                {
                    mCurrentDataPathIsRelative = PR_TRUE;
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
        nsCOMPtr<nsIDOMDocumentTraversal> trav = do_QueryInterface(docData->mDocument, &rv);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
        nsCOMPtr<nsIDOMTreeWalker> walker;
        rv = trav->CreateTreeWalker(docAsNode, 
            nsIDOMNodeFilter::SHOW_ELEMENT |
                nsIDOMNodeFilter::SHOW_DOCUMENT |
                nsIDOMNodeFilter::SHOW_PROCESSING_INSTRUCTION,
            nsnull, PR_TRUE, getter_AddRefs(walker));
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
                PRBool exists = PR_FALSE;
                PRBool haveDir = PR_FALSE;

                localDataPath->Exists(&exists);
                if (exists)
                {
                    localDataPath->IsDirectory(&haveDir);
                }
                if (!haveDir && NS_SUCCEEDED(localDataPath->Create(nsILocalFile::DIRECTORY_TYPE, 0755)))
                {
                    haveDir = PR_TRUE;
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
                    cleanupData->mIsDirectory = PR_TRUE;
                    mCleanupList.AppendElement(cleanupData);
                }
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
            !mContentType.IsEmpty() ? mContentType.get() : nsnull,
            getter_Copies(realContentType));

        nsCAutoString contentType; contentType.AssignWithConversion(realContentType);
        nsCAutoString charType; // Empty

        // Save the document
        nsCOMPtr<nsIDocument> docAsDoc = do_QueryInterface(aDocument);
        rv = SaveDocumentWithFixup(
            docAsDoc,
            nsnull,  // no dom fixup
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

    mStartSaving = PR_TRUE;

    // Iterate through all queued documents, saving them to file and fixing
    // them up on the way.

    PRInt32 i;
    for (i = 0; i < mDocList.Count(); i++)
    {
        DocData *docData = (DocData *) mDocList.ElementAt(i);
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

        nsCOMPtr<nsIDocument> docAsDoc = do_QueryInterface(docData->mDocument);

        // Get the content type
        nsXPIDLString realContentType;
        GetDocEncoderContentType(docData->mDocument,
            !mContentType.IsEmpty() ? mContentType.get() : nsnull,
            getter_Copies(realContentType));

        nsCAutoString contentType; contentType.AssignWithConversion(realContentType.get());
        nsCAutoString charType; // Empty

        // Save the document, fixing up the links as it goes out
        rv = SaveDocumentWithFixup(
            docAsDoc,
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
    for (i = 0; i < mDocList.Count(); i++)
    {
        DocData *docData = (DocData *) mDocList.ElementAt(i);
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
    PRInt32 i;
    for (i = 0; i < mDocList.Count(); i++)
    {
        DocData *docData = (DocData *) mDocList.ElementAt(i);
        delete docData;
    }
    mDocList.Clear();
    for (i = 0; i < mCleanupList.Count(); i++)
    {
        CleanupData *cleanupData = (CleanupData *) mCleanupList.ElementAt(i);
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
        PRInt32 i;
        for (i = 0; i < mCleanupList.Count(); i++)
        {
            CleanupData *cleanupData = (CleanupData *) mCleanupList.ElementAt(i);
            nsCOMPtr<nsILocalFile> file = cleanupData->mFile;

            // Test if the dir / file exists (something in an earlier loop
            // may have already removed it)
            PRBool exists = PR_FALSE;
            file->Exists(&exists);
            if (!exists)
                continue;

            // Test if the file has changed in between creation and deletion
            // in some way that means it should be ignored
            PRBool isDirectory = PR_FALSE;
            file->IsDirectory(&isDirectory);
            if (isDirectory != cleanupData->mIsDirectory)
                continue; // A file has become a dir or vice versa !

            if (pass == 0 && !isDirectory)
            {
                file->Remove(PR_FALSE);
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

                PRBool isEmptyDirectory = PR_TRUE;
                nsSupportsArray dirStack;
                PRUint32 stackSize = 0;

                // Push the top level enum onto the stack
                nsCOMPtr<nsISimpleEnumerator> pos;
                if (NS_SUCCEEDED(file->GetDirectoryEntries(getter_AddRefs(pos))))
                    dirStack.AppendElement(pos);

                while (isEmptyDirectory &&
                    NS_SUCCEEDED(dirStack.Count(&stackSize)) && stackSize > 0)
                {
                    // Pop the last element
                    nsCOMPtr<nsISimpleEnumerator> curPos;
                    dirStack.GetElementAt(stackSize - 1, getter_AddRefs(curPos));
                    dirStack.RemoveElementAt(stackSize - 1);
                    
                    // Test if the enumerator has any more files in it
                    PRBool hasMoreElements = PR_FALSE;
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
                    nsCOMPtr<nsILocalFile> childAsFile = do_QueryInterface(child);
                    NS_ASSERTION(childAsFile, "This should be a file but isn't");

                    PRBool childIsSymlink = PR_FALSE;
                    childAsFile->IsSymlink(&childIsSymlink);
                    PRBool childIsDir = PR_FALSE;
                    childAsFile->IsDirectory(&childIsDir);                           
                    if (!childIsDir || childIsSymlink)
                    {
                        // Some kind of file or symlink which means dir
                        // is not empty so just drop out.
                        isEmptyDirectory = PR_FALSE;
                        break;
                    }
                    // Push parent enumerator followed by child enumerator
                    nsCOMPtr<nsISimpleEnumerator> childPos;
                    childAsFile->GetDirectoryEntries(getter_AddRefs(childPos));
                    dirStack.AppendElement(curPos);
                    if (childPos)
                        dirStack.AppendElement(childPos);

                }
                dirStack.Clear();

                // If after all that walking the dir is deemed empty, delete it
                if (isEmptyDirectory)
                {
                    file->Remove(PR_TRUE);
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

    PRBool nameHasChanged = PR_FALSE;
    nsresult rv;

    // Get the old filename
    nsCAutoString filename;
    rv = url->GetFileName(filename);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    nsCAutoString directory;
    rv = url->GetDirectory(directory);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Split the filename into a base and an extension.
    // e.g. "foo.html" becomes "foo" & ".html"
    //
    // The nsIURL methods GetFileBaseName & GetFileExtension don't
    // preserve the dot whereas this code does to save some effort
    // later when everything is put back together.
    PRInt32 lastDot = filename.RFind(".");
    nsCAutoString base;
    nsCAutoString ext;
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
    PRInt32 needToChop = filename.Length() - kDefaultMaxFilenameLength;
    if (needToChop > 0)
    {
        // Truncate the base first and then the ext if necessary
        if (base.Length() > (PRUint32) needToChop)
        {
            base.Truncate(base.Length() - needToChop);
        }
        else
        {
            needToChop -= base.Length() - 1;
            base.Truncate(1);
            if (ext.Length() > (PRUint32) needToChop)
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
        nameHasChanged = PR_TRUE;
    }

    // Ensure the filename is unique
    // Create a filename if it's empty, or if the filename / datapath is
    // already taken by another URI and create an alternate name.

    if (base.IsEmpty() || mFilenameList.Count() > 0)
    {
        nsCAutoString tmpPath;
        nsCAutoString tmpBase;
        PRUint32 duplicateCounter = 1;
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
            if (mFilenameList.IndexOf(tmpPath) < 0)
            {
                if (!base.Equals(tmpBase))
                {
                    filename.Assign(tmpBase);
                    filename.Append(ext);
                    nameHasChanged = PR_TRUE;
                }
                break;
            }
            duplicateCounter++;
        }
    }

    // Add name to list of those already used
    nsCAutoString newFilepath(directory);
    newFilepath.Append(filename);
    mFilenameList.AppendCString(newFilepath);

    // Update the uri accordingly if the filename actually changed
    if (nameHasChanged)
    {
        // Final sanity test
        if (filename.Length() > kDefaultMaxFilenameLength)
        {
            NS_WARNING("Filename wasn't truncated less than the max file length - how can that be?");
            return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsILocalFile> localFile;
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
        nsCAutoString nameFromURL;
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
            PRUint32 nameLength = 0;
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
                        // nsILocalFile (e.g. the Mac impl) might truncate
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

    nsCAutoString contentType;

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

        nsCOMPtr<nsILocalFile> localFile;
        GetLocalFileFromURI(aURI, getter_AddRefs(localFile));

        if (mimeInfo)
        {
            nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
            NS_ENSURE_TRUE(url, NS_ERROR_FAILURE);

            nsCAutoString newFileName;
            url->GetFileName(newFileName);

            // Test if the current extension is current for the mime type
            PRBool hasExtension = PR_FALSE;
            PRInt32 ext = newFileName.RFind(".");
            if (ext != -1)
            {
                mimeInfo->ExtensionExists(Substring(newFileName, ext + 1), &hasExtension);
            }

            // Append the mime file extension
            nsCAutoString fileExt;
            if (!hasExtension)
            {
                // Test if previous extension is acceptable
                nsCOMPtr<nsIURL> oldurl(do_QueryInterface(aOriginalURIWithExtension));
                NS_ENSURE_TRUE(oldurl, NS_ERROR_FAILURE);
                oldurl->GetFileExtension(fileExt);
                PRBool useOldExt = PR_FALSE;
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
                    PRUint32 newLength = newFileName.Length() + fileExt.Length() + 1;
                    if (newLength > kDefaultMaxFilenameLength)
                    {
                        newFileName.Truncate(newFileName.Length() - (newLength - kDefaultMaxFilenameLength));
                    }
                    newFileName.Append(".");
                    newFileName.Append(fileExt);
                }

                if (localFile)
                {
                    localFile->SetNativeLeafName(newFileName);

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

#ifdef  XP_MAC
        // Set appropriate Mac file type/creator for this mime type
        nsCOMPtr<nsILocalFileMac> macFile(do_QueryInterface(localFile));
        if (macFile)
        {
            macFile->SetFileTypeAndCreatorFromMIMEType(contentType.get());
        }
#endif            
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeOutputStream(
    nsIURI *aURI, nsIOutputStream **aOutputStream)
{
    nsresult rv;

    nsCOMPtr<nsILocalFile> localFile;
    GetLocalFileFromURI(aURI, getter_AddRefs(localFile));
    if (localFile)
        rv = MakeOutputStreamFromFile(localFile, aOutputStream);
    else
        rv = MakeOutputStreamFromURI(aURI, aOutputStream);

    return rv;
}

nsresult
nsWebBrowserPersist::MakeOutputStreamFromFile(
    nsILocalFile *aFile, nsIOutputStream **aOutputStream)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIFileOutputStream> fileOutputStream =
        do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // XXX brade:  get the right flags here!
    rv = fileOutputStream->Init(aFile, -1, -1, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_SUCCESS(CallQueryInterface(fileOutputStream, aOutputStream), NS_ERROR_FAILURE);

    if (mPersistFlags & PERSIST_FLAGS_CLEANUP_ON_FAILURE)
    {
        // Add to cleanup list in event of failure
        CleanupData *cleanupData = new CleanupData;
        NS_ENSURE_TRUE(cleanupData, NS_ERROR_OUT_OF_MEMORY);
        cleanupData->mFile = aFile;
        cleanupData->mIsDirectory = PR_FALSE;
        mCleanupList.AppendElement(cleanupData);
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeOutputStreamFromURI(
    nsIURI *aURI, nsIOutputStream  **aOutputStream)
{
    PRUint32 segsize = 8192;
    PRUint32 maxsize = PRUint32(-1);
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
    mCompleted = PR_TRUE;
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

struct FixRedirectData
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
    data.mMatchingKey = nsnull;
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

PRBool PR_CALLBACK
nsWebBrowserPersist::EnumFixRedirect(nsHashKey *aKey, void *aData, void* closure)
{
    FixRedirectData *data = (FixRedirectData *) closure;

    nsCOMPtr<nsISupports> keyPtr;
    ((nsMyISupportsKey *) aKey)->GetISupports(getter_AddRefs(keyPtr));

    nsCOMPtr<nsIChannel> thisChannel = do_QueryInterface(keyPtr);
    nsCOMPtr<nsIURI> thisURI;

    thisChannel->GetOriginalURI(getter_AddRefs(thisURI));

    // Compare this channel's URI to the one passed in.
    PRBool matchingURI = PR_FALSE;
    thisURI->Equals(data->mOriginalURI, &matchingURI);
    if (matchingURI)
    {
        data->mMatchingKey = (nsISupportsKey *) aKey;
        return PR_FALSE; // Stop enumerating
    }

    return PR_TRUE;
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

PRBool PR_CALLBACK
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
    return PR_TRUE;
}

PRBool PR_CALLBACK
nsWebBrowserPersist::EnumCalcUploadProgress(nsHashKey *aKey, void *aData, void* closure)
{
    if (aData && closure)
    {
        nsWebBrowserPersist *pthis = (nsWebBrowserPersist *) closure;
        UploadData *data = (UploadData *) aData;
        pthis->mTotalCurrentProgress += data->mSelfProgress;
        pthis->mTotalMaxProgress += data->mSelfProgressMax;
    }
    return PR_TRUE;
}

PRBool PR_CALLBACK
nsWebBrowserPersist::EnumCountURIsToPersist(nsHashKey *aKey, void *aData, void* closure)
{
    URIData *data = (URIData *) aData;
    PRUint32 *count = (PRUint32 *) closure;
    if (data->mNeedsPersisting && !data->mSaved)
    {
        (*count)++;
    }
    return PR_TRUE;
}

PRBool PR_CALLBACK
nsWebBrowserPersist::EnumPersistURIs(nsHashKey *aKey, void *aData, void* closure)
{
    URIData *data = (URIData *) aData;
    if (!data->mNeedsPersisting || data->mSaved)
    {
        return PR_TRUE;
    }

    nsWebBrowserPersist *pthis = (nsWebBrowserPersist *) closure;
    nsresult rv;

    // Create a URI from the key
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), 
                   nsDependentCString(((nsCStringKey *) aKey)->GetString(),
                                      ((nsCStringKey *) aKey)->GetStringLength()),
                   data->mCharset.get());
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // Make a URI to save the data to
    nsCOMPtr<nsIURI> fileAsURI;
    rv = data->mDataPath->Clone(getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = pthis->AppendPathToURI(fileAsURI, data->mFilename);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = pthis->SaveURIInternal(uri, nsnull, nsnull, nsnull, nsnull, fileAsURI, PR_TRUE);
    // if SaveURIInternal fails, then it will have called EndDownload,
    // which means that |aData| is no longer valid memory.  we MUST bail.
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (rv == NS_OK)
    {
        // Store the actual object because once it's persisted this
        // will be fixed up with the right file extension.

        data->mFile = fileAsURI;
        data->mSaved = PR_TRUE;
    }
    else
    {
        data->mNeedsFixup = PR_FALSE;
    }

    if (pthis->mSerializingOutput)
        return PR_FALSE;

    return PR_TRUE;
}

PRBool PR_CALLBACK
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
    if (data)
    {
        delete data;
    }
    return PR_TRUE;
}


PRBool PR_CALLBACK
nsWebBrowserPersist::EnumCleanupURIMap(nsHashKey *aKey, void *aData, void* closure)
{
    URIData *data = (URIData *) aData;
    if (data)
    {
        delete data; // Delete data associated with key
    }
    return PR_TRUE;
}


PRBool PR_CALLBACK
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
    if (data)
    {
        delete data; // Delete data associated with key
    }
    return PR_TRUE;
}


PRBool
nsWebBrowserPersist::GetQuotedAttributeValue(
    const nsAString &aSource, const nsAString &aAttribute, nsAString &aValue)
{  
    // NOTE: This code was lifted verbatim from nsParserUtils.cpp
    aValue.Truncate();
    nsAString::const_iterator start, end;
    aSource.BeginReading(start);
    aSource.EndReading(end);
    nsAString::const_iterator iter(end);

    while (start != end) {
        if (FindInReadable(aAttribute, start, iter))
        {
            // walk past any whitespace
            while (iter != end && nsCRT::IsAsciiSpace(*iter))
            {
                ++iter;
            }

            if (iter == end)
                break;
            
            // valid name="value" pair?
            if (*iter != '=')
            {
                start = iter;
                iter = end;
                continue;
            }
            // move past the =
            ++iter;

            while (iter != end && nsCRT::IsAsciiSpace(*iter))
            {
                ++iter;
            }

            if (iter == end)
                break;

            PRUnichar q = *iter;
            if (q != '"' && q != '\'')
            {
                start = iter;
                iter = end;
                continue;
            }

            // point to the first char of the value
            ++iter;
            start = iter;
            if (FindCharInReadable(q, iter, end))
            {
                aValue = Substring(start, iter);
                return PR_TRUE;
            }

            // we've run out of string.  Just return...
            break;
         }
    }
    return PR_FALSE;
}

nsresult nsWebBrowserPersist::FixupXMLStyleSheetLink(nsIDOMProcessingInstruction *aPI, const nsAString &aHref)
{
    NS_ENSURE_ARG_POINTER(aPI);
    nsresult rv = NS_OK;

    nsAutoString data;
    rv = aPI->GetData(data);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsAutoString href;
    GetQuotedAttributeValue(data, NS_LITERAL_STRING("href"), href);

    // Construct and set a new data value for the xml-stylesheet
    if (!aHref.IsEmpty() && !href.IsEmpty())
    {
        nsAutoString alternate;
        nsAutoString charset;
        nsAutoString title;
        nsAutoString type;
        nsAutoString media;

        GetQuotedAttributeValue(data, NS_LITERAL_STRING("alternate"), alternate);
        GetQuotedAttributeValue(data, NS_LITERAL_STRING("charset"), charset);
        GetQuotedAttributeValue(data, NS_LITERAL_STRING("title"), title);
        GetQuotedAttributeValue(data, NS_LITERAL_STRING("type"), type);
        GetQuotedAttributeValue(data, NS_LITERAL_STRING("media"), media);

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

    GetQuotedAttributeValue(data, NS_LITERAL_STRING("href"), aHref);

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
                StoreURI(NS_ConvertUCS2toUTF8(href).get());
            }
        }
    }

    // Test the node to see if it's an image, frame, iframe, css, js
    nsCOMPtr<nsIDOMHTMLImageElement> nodeAsImage = do_QueryInterface(aNode);
    if (nodeAsImage)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLBodyElement> nodeAsBody = do_QueryInterface(aNode);
    if (nodeAsBody)
    {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLTableElement> nodeAsTable = do_QueryInterface(aNode);
    if (nodeAsTable)
    {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLTableRowElement> nodeAsTableRow = do_QueryInterface(aNode);
    if (nodeAsTableRow)
    {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLTableCellElement> nodeAsTableCell = do_QueryInterface(aNode);
    if (nodeAsTableCell)
    {
        StoreURIAttribute(aNode, "background");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLScriptElement> nodeAsScript = do_QueryInterface(aNode);
    if (nodeAsScript)
    {
        StoreURIAttribute(aNode, "src");
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
        URIData *data = nsnull;
        StoreURIAttribute(aNode, "src", PR_FALSE, &data);
        if (data)
        {
            data->mIsSubFrame = PR_TRUE;
            // Save the frame content
            nsCOMPtr<nsIDOMDocument> content;
            nodeAsFrame->GetContentDocument(getter_AddRefs(content));
            if (content)
            {
                nsXPIDLString ext;
                GetDocumentExtension(content, getter_Copies(ext));
                data->mSubFrameExt.AssignLiteral(".");
                data->mSubFrameExt.Append(ext);
                SaveSubframeContent(content, data);
            }
        }
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLIFrameElement> nodeAsIFrame = do_QueryInterface(aNode);
    if (nodeAsIFrame && !(mPersistFlags & PERSIST_FLAGS_IGNORE_IFRAMES))
    {
        URIData *data = nsnull;
        StoreURIAttribute(aNode, "src", PR_FALSE, &data);
        if (data)
        {
            data->mIsSubFrame = PR_TRUE;
            // Save the frame content
            nsCOMPtr<nsIDOMDocument> content;
            nodeAsIFrame->GetContentDocument(getter_AddRefs(content));
            if (content)
            {
                nsXPIDLString ext;
                GetDocumentExtension(content, getter_Copies(ext));
                data->mSubFrameExt.AssignLiteral(".");
                data->mSubFrameExt.Append(ext);
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
        nsresult rv = aNodeIn->CloneNode(PR_FALSE, aNodeOut);
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
nsWebBrowserPersist::CloneNodeWithFixedUpURIAttributes(
    nsIDOMNode *aNodeIn, nsIDOMNode **aNodeOut)
{
    nsresult rv;
    *aNodeOut = nsnull;

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
            nodeAsBase->GetOwnerDocument(getter_AddRefs(ownerDocument));
            if (ownerDocument)
            {
                nsAutoString href;
                nodeAsBase->GetHref(href); // Doesn't matter if this fails
                nsCOMPtr<nsIDOMComment> comment;
                nsAutoString commentText; commentText.AssignLiteral(" base ");
                if (!href.IsEmpty())
                {
                    commentText += NS_LITERAL_STRING("href=\"") + href + NS_LITERAL_STRING("\" ");
                }
                rv = ownerDocument->CreateComment(commentText, getter_AddRefs(comment));
                if (comment)
                {
                    return comment->QueryInterface(NS_GET_IID(nsIDOMNode), (void **) aNodeOut);
                }
            }
        }
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

    nsCOMPtr<nsIDOMHTMLBodyElement> nodeAsBody = do_QueryInterface(aNodeIn);
    if (nodeAsBody)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "background");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLTableElement> nodeAsTable = do_QueryInterface(aNodeIn);
    if (nodeAsTable)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "background");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLTableRowElement> nodeAsTableRow = do_QueryInterface(aNodeIn);
    if (nodeAsTableRow)
    {
        rv = GetNodeToFixup(aNodeIn, aNodeOut);
        if (NS_SUCCEEDED(rv) && *aNodeOut)
        {
            FixupNodeAttribute(*aNodeOut, "background");
        }
        return rv;
    }

    nsCOMPtr<nsIDOMHTMLTableCellElement> nodeAsTableCell = do_QueryInterface(aNodeIn);
    if (nodeAsTableCell)
    {
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
            FixupAnchor(*aNodeOut);
            FixupNodeAttribute(*aNodeOut, "src");
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
            FixupNodeAttribute(*aNodeOut, "src");
        }
        return rv;
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::StoreURI(
    const char *aURI, PRBool aNeedsPersisting, URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aURI);
    if (aData)
        *aData = nsnull;
    
    // Test whether this URL should be persisted
    PRBool shouldPersistURI = PR_TRUE;
    for (PRUint32 i = 0; i < kNonpersistableSchemesSize; i++)
    {
        PRUint32 schemeLen = strlen(kNonpersistableSchemes[i]);
        if (nsCRT::strncasecmp(aURI, kNonpersistableSchemes[i], schemeLen) == 0)
        {
            shouldPersistURI = PR_FALSE;
            break;
        }
    }
    if (shouldPersistURI)
    {
        URIData *data = nsnull;
        MakeAndStoreLocalFilenameInURIMap(aURI, aNeedsPersisting, &data);
        if (aData)
        {
            *aData = data;
        }
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::StoreURIAttribute(
    nsIDOMNode *aNode, const char *aAttribute, PRBool aNeedsPersisting,
    URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aNode);
    NS_ENSURE_ARG_POINTER(aAttribute);

    nsresult rv = NS_OK;

    // Find the named URI attribute on the (element) node and store
    // a reference to the URI that maps onto a local file name

    nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
    nsCOMPtr<nsIDOMNode> attrNode;
    rv = aNode->GetAttributes(getter_AddRefs(attrMap));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsString attribute; attribute.AssignWithConversion(aAttribute);
    rv = attrMap->GetNamedItem(attribute, getter_AddRefs(attrNode));
    if (attrNode)
    {
        nsAutoString oldValue;
        attrNode->GetNodeValue(oldValue);
        if (!oldValue.IsEmpty())
        {
            NS_ConvertUCS2toUTF8 oldCValue(oldValue);
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
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    nsCAutoString spec;
    rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

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
        NS_ENSURE_SUCCESS(rv, PR_FALSE);
    }
    else
    {
        rv = data->mDataPath->Clone(getter_AddRefs(fileAsURI));
        NS_ENSURE_SUCCESS(rv, PR_FALSE);
        rv = AppendPathToURI(fileAsURI, data->mFilename);
        NS_ENSURE_SUCCESS(rv, PR_FALSE);
    }
    nsAutoString newValue;

    // remove username/password if present
    fileAsURI->SetUserPass(EmptyCString());

    // reset node attribute 
    // Use relative or absolute links
    if (data->mDataPathIsRelative)
    {
        nsCOMPtr<nsIURL> url(do_QueryInterface(fileAsURI));
        NS_ENSURE_TRUE(url, NS_ERROR_FAILURE);
        nsCAutoString filename;
        url->GetFileName(filename);

        nsCAutoString rawPathURL(data->mRelativePathToData);
        rawPathURL.Append(filename);

        nsCAutoString buf;
        AppendUTF8toUTF16(NS_EscapeURL(rawPathURL, esc_FilePath, buf),
                          newValue);
    }
    else
    {
        nsCAutoString fileurl;
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
nsWebBrowserPersist::FixupNodeAttribute(nsIDOMNode *aNode,
                                        const char *aAttribute)
{
    NS_ENSURE_ARG_POINTER(aNode);
    NS_ENSURE_ARG_POINTER(aAttribute);

    nsresult rv = NS_OK;

    // Find the named URI attribute on the (element) node and change it to reference
    // a local file.

    nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
    nsCOMPtr<nsIDOMNode> attrNode;
    rv = aNode->GetAttributes(getter_AddRefs(attrMap));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsString attribute; attribute.AssignWithConversion(aAttribute);
    rv = attrMap->GetNamedItem(attribute, getter_AddRefs(attrNode));
    if (attrNode)
    {
        nsString uri;
        attrNode->GetNodeValue(uri);
        rv = FixupURI(uri);
        if (NS_SUCCEEDED(rv))
        {
            attrNode->SetNodeValue(uri);
        }
    }

    return rv;
}

nsresult
nsWebBrowserPersist::FixupAnchor(nsIDOMNode *aNode)
{
    NS_ENSURE_ARG_POINTER(aNode);

    nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
    nsCOMPtr<nsIDOMNode> attrNode;
    nsresult rv = aNode->GetAttributes(getter_AddRefs(attrMap));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (mPersistFlags & PERSIST_FLAGS_DONT_FIXUP_LINKS)
    {
        return NS_OK;
    }

    // Make all anchor links absolute so they point off onto the Internet
    nsString attribute(NS_LITERAL_STRING("href"));
    rv = attrMap->GetNamedItem(attribute, getter_AddRefs(attrNode));
    if (attrNode)
    {
        nsString oldValue;
        attrNode->GetNodeValue(oldValue);
        NS_ConvertUCS2toUTF8 oldCValue(oldValue);

        // Skip empty values and self-referencing bookmarks
        if (oldCValue.IsEmpty() || oldCValue.CharAt(0) == '#')
        {
            return NS_OK;
        }

        // if saving file to same location, we don't need to do any fixup
        PRBool isEqual = PR_FALSE;
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
            nsCAutoString uriSpec;
            newURI->GetSpec(uriSpec);
            attrNode->SetNodeValue(NS_ConvertUTF8toUCS2(uriSpec));
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

nsresult
nsWebBrowserPersist::SaveSubframeContent(
    nsIDOMDocument *aFrameContent, URIData *aData)
{
    NS_ENSURE_ARG_POINTER(aData);
    nsresult rv;

    nsString filenameWithExt = aData->mFilename;
    filenameWithExt.Append(aData->mSubFrameExt);

    // Work out the path for the subframe
    nsCOMPtr<nsIURI> frameURI;
    rv = mCurrentDataPath->Clone(getter_AddRefs(frameURI));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = AppendPathToURI(frameURI, filenameWithExt);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // Work out the path for the subframe data
    nsCOMPtr<nsIURI> frameDataURI;
    rv = mCurrentDataPath->Clone(getter_AddRefs(frameDataURI));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    nsAutoString newFrameDataPath(aData->mFilename);

    // Append _data
    newFrameDataPath.AppendLiteral("_data");
    rv = AppendPathToURI(frameDataURI, newFrameDataPath);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // Make frame document & data path conformant and unique
    rv = CalculateUniqueFilename(frameURI);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = CalculateUniqueFilename(frameDataURI);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    mCurrentThingsToPersist++;
    SaveDocumentInternal(aFrameContent, frameURI, frameDataURI);

    // Store the updated uri to the frame
    aData->mFile = frameURI;
    aData->mSubFrameExt.Truncate(); // we already put this in frameURI

    return NS_OK;
}

nsresult
nsWebBrowserPersist::CreateChannelFromURI(nsIURI *aURI, nsIChannel **aChannel)
{
    nsresult rv = NS_OK;
    *aChannel = nsnull;

    nsCOMPtr<nsIIOService> ioserv;
    ioserv = do_GetIOService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ioserv->NewChannelFromURI(aURI, aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(*aChannel);

    rv = (*aChannel)->SetNotificationCallbacks(NS_STATIC_CAST(nsIInterfaceRequestor *, this));
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
} 

nsresult
nsWebBrowserPersist::SaveDocumentWithFixup(
    nsIDocument *aDocument, nsIDocumentEncoderNodeFixup *aNodeFixup,
    nsIURI *aFile, PRBool aReplaceExisting, const nsACString &aFormatType,
    const nsCString &aSaveCharset, PRUint32 aFlags)
{
    NS_ENSURE_ARG_POINTER(aFile);
    
    nsresult  rv = NS_OK;
    nsCOMPtr<nsILocalFile> localFile;
    GetLocalFileFromURI(aFile, getter_AddRefs(localFile));
    if (localFile)
    {
        // if we're not replacing an existing file but the file
        // exists, something is wrong
        PRBool fileExists = PR_FALSE;
        rv = localFile->Exists(&fileExists);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        if (!aReplaceExisting && fileExists)
            return NS_ERROR_FAILURE;                // where are the file I/O errors?
    }
    
    nsCOMPtr<nsIOutputStream> outputStream;
    rv = MakeOutputStream(aFile, getter_AddRefs(outputStream));
    if (NS_FAILED(rv))
    {
        SendErrorStatusChange(PR_FALSE, rv, nsnull, aFile);
        return NS_ERROR_FAILURE;
    }
    NS_ENSURE_TRUE(outputStream, NS_ERROR_FAILURE);

    // Get a document encoder instance
    nsCAutoString contractID(NS_DOC_ENCODER_CONTRACTID_BASE);
    contractID.Append(aFormatType);
    
    nsCOMPtr<nsIDocumentEncoder> encoder = do_CreateInstance(contractID.get(), &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    NS_ConvertASCIItoUCS2 newContentType(aFormatType);
    rv = encoder->Init(aDocument, newContentType, aFlags);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    mTargetBaseURI = aFile;

    // Set the node fixup callback
    encoder->SetNodeFixup(aNodeFixup);

    if (mWrapColumn && (aFlags & ENCODE_FLAGS_WRAP))
        encoder->SetWrapColumn(mWrapColumn);

    nsCAutoString charsetStr(aSaveCharset);
    if (charsetStr.IsEmpty())
        charsetStr = aDocument->GetDocumentCharacterSet();

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

    return rv;
}


// we store the current location as the key (absolutized version of domnode's attribute's value)
nsresult
nsWebBrowserPersist::MakeAndStoreLocalFilenameInURIMap(
    const char *aURI, PRBool aNeedsPersisting, URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aURI);

    nsresult rv;

    // Make a URI
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURI), 
                   mCurrentCharset.get(), mCurrentBaseURI);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    nsCAutoString spec;
    rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Create a sensibly named filename for the URI and store in the URI map
    nsCStringKey key(spec.get());
    if (mURIMap.Exists(&key))
    {
        if (aData)
        {
            *aData = (URIData *) mURIMap.Get(&key);
        }
        return NS_OK;
    }

    // Create a unique file name for the uri
    nsString filename;
    rv = MakeFilenameFromURI(uri, filename);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Store the file name
    URIData *data = new URIData;
    NS_ENSURE_TRUE(data, NS_ERROR_OUT_OF_MEMORY);

    data->mNeedsPersisting = aNeedsPersisting;
    data->mNeedsFixup = PR_TRUE;
    data->mFilename = filename;
    data->mSaved = PR_FALSE;
    data->mIsSubFrame = PR_FALSE;
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

struct SpecialXHTMLTags {
    PRUnichar name[sizeof(PRUnichar)*11]; // strlen("blockquote")==10
};

static PRBool IsSpecialXHTMLTag(nsIDOMNode *aNode)
{
    nsAutoString ns;
    aNode->GetNamespaceURI(ns);
    if (!ns.EqualsLiteral("http://www.w3.org/1999/xhtml"))
        return PR_FALSE;

    // Ordered so that typical documents work fastest
    static SpecialXHTMLTags tags[] = {
        {'b','o','d','y',0},
        {'h','e','a','d',0},
        {'i','m','g',0},
        {'s','c','r','i','p','t',0},
        {'a',0},
        {'a','r','e','a',0},
        {'l','i','n','k',0},
        {'i','n','p','u','t',0},
        {'f','r','a','m','e',0},
        {'i','f','r','a','m','e',0},
        {'o','b','j','e','c','t',0},
        {'a','p','p','l','e','t',0},
        {'f','o','r','m',0},
        {'b','l','o','c','k','q','u','o','t','e',0},
        {'q',0},
        {'d','e','l',0},
        {'i','n','s',0},
        {0}};

    nsAutoString localName;
    aNode->GetLocalName(localName);
    PRInt32 i;
    for (i = 0; tags[i].name[0]; i++) {
        if (localName.Equals(tags[i].name))
        {
            // XXX This element MAY have URI attributes, but
            //     we are not actually checking if they are present.
            //     That would slow us down further, and I am not so sure
            //     how important that would be.
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

static PRBool HasSpecialXHTMLTags(nsIDOMNode *aParent)
{
    if (IsSpecialXHTMLTag(aParent))
        return PR_TRUE;

    nsCOMPtr<nsIDOMNodeList> list;
    aParent->GetChildNodes(getter_AddRefs(list));
    if (list)
    {
        PRUint32 count;
        list->GetLength(&count);
        PRUint32 i;
        for (i = 0; i < count; i++) {
            nsCOMPtr<nsIDOMNode> node;
            list->Item(i, getter_AddRefs(node));
            if (!node)
                break;
            PRUint16 nodeType;
            node->GetNodeType(&nodeType);
            if (nodeType == nsIDOMNode::ELEMENT_NODE) {
                return HasSpecialXHTMLTags(node);
            }
        }
    }

    return PR_FALSE;
}

static PRBool NeedXHTMLBaseTag(nsIDOMDocument *aDocument)
{
    nsCOMPtr<nsIDOMElement> docElement;
    aDocument->GetDocumentElement(getter_AddRefs(docElement));

    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(docElement));
    if (node)
    {
        return HasSpecialXHTMLTags(node);
    }

    return PR_FALSE;
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
    nsCOMPtr<nsIDOMNodeList> baseList;
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
    }
    if (!baseElement)
    {
        return NS_ERROR_FAILURE;
    }
    nsCAutoString uriSpec;
    aBaseURI->GetSpec(uriSpec);
    NS_ConvertUTF8toUCS2 href(uriSpec);
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
    encChannel->SetApplyConversion(PR_FALSE);

    nsCOMPtr<nsIURI> thisURI;
    aChannel->GetURI(getter_AddRefs(thisURI));
    nsCOMPtr<nsIURL> sourceURL(do_QueryInterface(thisURI));
    if (!sourceURL)
        return;
    nsCAutoString extension;
    sourceURL->GetFileExtension(extension);

    nsCOMPtr<nsIUTF8StringEnumerator> encEnum;
    encChannel->GetContentEncodings(getter_AddRefs(encEnum));
    if (!encEnum)
        return;
    nsCOMPtr<nsIExternalHelperAppService> helperAppService =
        do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return;
    PRBool hasMore;
    rv = encEnum->HasMore(&hasMore);
    if (NS_SUCCEEDED(rv) && hasMore)
    {
        nsCAutoString encType;
        rv = encEnum->GetNext(encType);
        if (NS_SUCCEEDED(rv))
        {
            PRBool applyConversion = PR_FALSE;
            rv = helperAppService->ApplyDecodingForExtension(extension, encType,
                                                             &applyConversion);
            if (NS_SUCCEEDED(rv))
                encChannel->SetApplyConversion(applyConversion);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////


nsEncoderNodeFixup::nsEncoderNodeFixup() : mWebBrowserPersist(nsnull)
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
    nsIDOMNode *aNode, nsIDOMNode **aOutNode)
{
    NS_ENSURE_ARG_POINTER(aNode);
    NS_ENSURE_ARG_POINTER(aOutNode);
    NS_ENSURE_TRUE(mWebBrowserPersist, NS_ERROR_FAILURE);

    *aOutNode = nsnull;
    
    // Test whether we need to fixup the node
    PRUint16 type = 0;
    aNode->GetNodeType(&type);
    if (type == nsIDOMNode::ELEMENT_NODE ||
        type == nsIDOMNode::PROCESSING_INSTRUCTION_NODE)
    {
        return mWebBrowserPersist->CloneNodeWithFixedUpURIAttributes(aNode, aOutNode);
    }

    return NS_OK;
}
