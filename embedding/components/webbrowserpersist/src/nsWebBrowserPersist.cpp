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

#include "nspr.h"

#define NO_XPCOM_FILE_STREAMS
#include "nsIFileStream.h"        // Old XPCOM file streams
#undef NO_XPCOM_FILE_STREAMS

#include "nsIFileStreams.h"       // New Necko file streams

#include "nsNetUtil.h"
#include "nsIFileTransportService.h"
#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsEscape.h"

#include "nsCExternalHandlerService.h"

#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDiskDocument.h"
#include "nsIWebProgressListener.h"
#include "nsIAuthPrompt.h"

#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLDocument.h"

#include "nsWebBrowserPersist.h"


// Information about a DOM document
struct DocData
{
    nsCOMPtr<nsIURI> mBaseURI;
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIURI> mFile;
    nsCOMPtr<nsIURI> mDataPath;
    PRBool mDataPathIsRelative;
    nsCString mRelativePathToData;
};

// Information about a URI
struct URIData
{
    PRBool   mNeedsPersisting;
    PRBool   mSaved;
    PRBool   mIsSubFrame;
    nsString mFilename;
    nsString mSubFrameExt;
    nsCOMPtr<nsIURI> mFile;
    nsCOMPtr<nsIURI> mDataPath;
    PRBool mDataPathIsRelative;
    nsCString mRelativePathToData;
};

// Information about the output stream
struct OutputData
{
    nsCOMPtr<nsIURI> mFile;
    PRBool mCalcFileExt;
    nsCOMPtr<nsIOutputStream> mStream;
    PRInt32 mSelfProgress;
    PRInt32 mSelfProgressMax;

    OutputData(nsIURI *aFile, PRBool aCalcFileExt) :
        mFile(aFile),
        mCalcFileExt(aCalcFileExt),
        mSelfProgress(0),
        mSelfProgressMax(10000)
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

// Default flags for persistence
const PRUint32 kDefaultPersistFlags = 
    nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION;

nsWebBrowserPersist::nsWebBrowserPersist() :
    mFileCounter(1),
    mFrameCounter(1),
    mFirstAndOnlyUse(PR_TRUE),
    mCancel(PR_FALSE),
    mJustStartedLoading(PR_TRUE),
    mCompleted(PR_FALSE),
    mPersistFlags(kDefaultPersistFlags),
    mPersistResult(NS_OK),
    mReplaceExisting(PR_TRUE),
    mEncodingFlags(0),
    mWrapColumn(72)
{
    NS_INIT_REFCNT();
}

nsWebBrowserPersist::~nsWebBrowserPersist()
{
    CleanUp();
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
    
    if (mProgressListener && aIID.Equals(NS_GET_IID(nsIAuthPrompt)))
    {
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

/* void saveURI (in nsIURI aURI, in string aFileName); */
NS_IMETHODIMP nsWebBrowserPersist::SaveURI(
    nsIURI *aURI, nsIInputStream *aPostData, nsISupports *aFile)
{
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);
    mFirstAndOnlyUse = PR_FALSE; // Stop people from reusing this object!

    nsCOMPtr<nsIURI> fileAsURI;
    nsresult rv;
    rv = GetValidURIFromObject(aFile, getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

    return SaveURIInternal(aURI, aPostData, fileAsURI, PR_FALSE);
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
    if (aEncodingFlags & ENCODE_FLAGS_ENCODE_ENTITIES)
        mEncodingFlags |= nsIDocumentEncoder::OutputEncodeEntities;
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

    if (datapathAsURI)
    {
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
        else
        {
            // There are no URIs to save, so just save the document(s)

            // State start notification
            if (mProgressListener)
            {
                mProgressListener->OnStateChange(nsnull, nsnull,
                    nsIWebProgressListener::STATE_START |
                        nsIWebProgressListener::STATE_IS_NETWORK,
                    NS_OK);
            }

            EndDownload(NS_OK);

            // State stop notification
            if (mProgressListener)
            {
                mProgressListener->OnStateChange(nsnull, nsnull,
                    nsIWebProgressListener::STATE_STOP |
                        nsIWebProgressListener::STATE_IS_NETWORK,
                    NS_OK);
            }
        }
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
    PRBool completed = PR_FALSE;
    if (mOutputMap.Count() == 0)
    {
        completed = PR_TRUE;
    }

    if (completed)
    {
        // Save the documents now all the URIs are saved
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
        mProgressListener->OnStateChange(nsnull, request, stateFlags, NS_OK);
    }

    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist::nsIStreamListener
//*****************************************************************************

NS_IMETHODIMP nsWebBrowserPersist::OnDataAvailable(
    nsIRequest* request, nsISupports *aContext, nsIInputStream *aIStream,
    PRUint32 aOffset, PRUint32 aLength)
{
    PRBool cancel = mCancel;
    if (!cancel)
    {
        nsresult rv = NS_OK;
        unsigned long bytesRemaining = aLength;

        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

        // NOTE: This code uses the channel as a hash key so it will not
        //       recognize redirected channels because the key is not the same.
        //       When that happens we remove and add the data entry to use the
        //       new channel as the hash key.

        nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(request);
        nsISupportsKey key(keyPtr);
        OutputData *data = (OutputData *) mOutputMap.Get(&key);
        if (!data)
        {
            // Redirect? Try and fixup the output table
            rv = FixRedirectedChannelEntry(channel);
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

            // Should be able to find the data after fixup unless redirects
            // are disabled.
            data = (OutputData *) mOutputMap.Get(&key);
            if (!data)
            {
                return NS_ERROR_FAILURE;
            }
        }

        // Make the output stream
        if (!data->mStream)
        {
            if (data->mCalcFileExt)
            {
                CalculateAndAppendFileExt(data->mFile, channel);
            }
            rv = MakeOutputStream(data->mFile,  channel, getter_AddRefs(data->mStream));
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
        }

        // Read data from the input and write to the output
        char buffer[8192];
        unsigned int bytesRead;
        while (!cancel && bytesRemaining)
        {
            rv = aIStream->Read(buffer, PR_MIN(sizeof(buffer), bytesRemaining), &bytesRead);
            if (NS_SUCCEEDED(rv))
            {
                unsigned int bytesWritten;
                rv = data->mStream->Write(buffer, bytesRead, &bytesWritten);
                if (NS_SUCCEEDED(rv) && bytesWritten == bytesRead)
                {
                    bytesRemaining -= bytesWritten;
                }
                else
                {
                    // Disaster - can't write out the bytes - disk full / permission?
                    cancel = PR_TRUE;
                }
            }
            else
            {
                // Disaster - can't read the bytes - broken link / file error?
                cancel = PR_TRUE;
            }
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
        mProgressListener->OnStatusChange(nsnull, request, status, statusArg);
    }
    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist private methods
//*****************************************************************************

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
        PRBool isFile = PR_FALSE;
        objAsURI->SchemeIs("file", &isFile);
        NS_ENSURE_TRUE(isFile, NS_ERROR_FAILURE);
        nsCOMPtr<nsIFileURL> objAsFileURL = do_QueryInterface(objAsURI);
        if (objAsURI)
        {
            *aURI = objAsURI;
            NS_ADDREF(*aURI);
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

nsresult nsWebBrowserPersist::GetLocalFileFromURI(nsIURI *aURI, nsILocalFile **aLocalFile) const
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aLocalFile);

    *aLocalFile = nsnull;
    nsresult rv = NS_OK;
    
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
    if (NS_FAILED(rv) || !fileURL)
    {
        return NS_ERROR_MALFORMED_URI;
    }

    nsCOMPtr<nsIFile> file;
    rv = fileURL->GetFile(getter_AddRefs(file));
    if (NS_FAILED(rv) || !file)
    {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    if (NS_FAILED(rv) || !localFile)
    {
        return NS_ERROR_FAILURE;
    }

    *aLocalFile = localFile;
    NS_ADDREF(*aLocalFile);
    return NS_OK;
}

nsresult nsWebBrowserPersist::AppendPathToURI(nsIURI *aURI, const nsAString & aPath) const
{
    NS_ENSURE_ARG_POINTER(aURI);

    nsXPIDLCString oldPath;
    nsresult rv = aURI->GetPath(getter_Copies(oldPath));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsCAutoString newPath(oldPath);

    // Append a forward slash if necessary
    PRInt32 len = newPath.Length();
    if (len > 0 && newPath.CharAt(len - 1) != '/')
    {
        newPath.Append('/');
    }

    // Store the path back on the URI
    newPath.AppendWithConversion(aPath);
    aURI->SetPath(newPath.get());

    return NS_OK;
}

nsresult nsWebBrowserPersist::SaveURIInternal(
    nsIURI *aURI, nsIInputStream *aPostData, nsIURI *aFile, PRBool aCalcFileExt)
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

    // Open a channel to the URI
    nsCOMPtr<nsIChannel> inputChannel;
    rv = NS_OpenURI(getter_AddRefs(inputChannel), aURI,
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
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(inputChannel));
        if (httpChannel)
        {
            httpChannel->SetApplyConversion(PR_FALSE);
        }
    }

    // Post data
    if (aPostData)
    {
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(inputChannel));
        if (httpChannel)
        {
            nsCOMPtr<nsISeekableStream> stream(do_QueryInterface(aPostData));
            if (stream)
            {
                // Rewind the postdata stream
                stream->Seek(nsISeekableStream::NS_SEEK_SET, 0);
                nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
                NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");
                // Attach the postdata to the http channel
                uploadChannel->SetUploadStream(aPostData, nsnull, -1);
            }
        }
    }

    // Add the output transport to the output map with the channel as the key
    nsCOMPtr<nsISupports> keyPtr = do_QueryInterface(inputChannel);
    nsISupportsKey key(keyPtr);
    mOutputMap.Put(&key, new OutputData(aFile, aCalcFileExt));
    
    // Read from the input channel
    rv = inputChannel->AsyncOpen(this, nsnull);
    if (rv == NS_ERROR_NO_CONTENT)
    {
        // Assume this is a protocol such as mailto: which does not feed out
        // data and just ignore it.
    }
    else if (NS_FAILED(rv))
    {
        EndDownload(NS_ERROR_FAILURE);
        return NS_ERROR_FAILURE;
    }

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
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // See if we can get the local file representation of this URI
    nsCOMPtr<nsILocalFile> localDataPath;
    if (aDataPath)
    {
        rv = GetLocalFileFromURI(aDataPath, getter_AddRefs(localDataPath));
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsIDOMNode> docAsNode = do_QueryInterface(aDocument);

    // Persist the main document
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
    doc->GetDocumentURL(getter_AddRefs(mURI));

    nsCOMPtr<nsIURI> oldBaseURI = mCurrentBaseURI;

    // Store the base URI
    doc->GetBaseURL(*getter_AddRefs(mCurrentBaseURI));

    // Does the caller want to fixup the referenced URIs and save those too?
    if (aDataPath)
    {
        // Basic steps are these.
        //
        // 1. Iterate throught the document (and subdocuments) building a list
        //    of unique URIs.
        // 2. For each URI create an OutputData entry and open a channel to save
        //    it. As each URI is saved, discover the mime type and fix up the
        //    local filename with the correcte extension.
        // 3. Store the document in a list and wait for URI persistence to finish
        // 4. After URI persistence completes save the list of documents,
        //    fixing it up as it goes out to file.

        if (localDataPath)
        {
            localDataPath->Create(nsILocalFile::DIRECTORY_TYPE, 0755);
            PRBool exists = PR_FALSE;
            PRBool isDirectory = PR_FALSE;
            localDataPath->Exists(&exists);
            localDataPath->IsDirectory(&isDirectory);
            if (!exists || !isDirectory)
            {
                EndDownload(NS_ERROR_FAILURE);
                mCurrentBaseURI = oldBaseURI;
                return NS_ERROR_FAILURE;
            }
        }

        // Test if the data path is relative to the base directory -
        // the one that the document is saved into.
        
        nsCOMPtr<nsIURI> oldDataPath = mCurrentDataPath;
        PRBool oldDataPathIsRelative = mCurrentDataPathIsRelative;
        nsCString oldCurrentRelativePathToData = mCurrentRelativePathToData;

        mCurrentDataPathIsRelative = PR_FALSE;
        mCurrentDataPath = aDataPath;
        mCurrentRelativePathToData = "";

        // Determine if the specified data path is relative to the
        // specified file, (e.g. c:\docs\htmldata is relative to
        // c:\docs\myfile.htm, but not to d:\foo\data.

        // Starting with the data dir work back through it's parents
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

                nsXPIDLCString dirName;
                dataDirParent->GetLeafName(getter_Copies(dirName));

                nsCAutoString newRelativePathToData;
                newRelativePathToData = dirName.get();
                newRelativePathToData.Append("/");
                newRelativePathToData.Append(relativePathToData);
                relativePathToData = newRelativePathToData;

                nsCOMPtr<nsIFile> newDataDirParent;
                rv = dataDirParent->GetParent(getter_AddRefs(newDataDirParent));
                dataDirParent = newDataDirParent;
            }
        }

        // Store the document in a list so when URI persistence is done and the
        // filenames of saved URIs are known, the documents can be fixed up and
        // saved,

        DocData *docData = new DocData;
        docData->mBaseURI = mCurrentBaseURI;
        docData->mDocument = aDocument;
        docData->mFile = aFile;
        docData->mRelativePathToData = mCurrentRelativePathToData;
        docData->mDataPath = mCurrentDataPath;
        docData->mDataPathIsRelative = mCurrentDataPathIsRelative;
        mDocList.AppendElement(docData);

        // Walk the DOM gathering a list of externally referenced URIs in the uri map
        nsDOMWalker walker;
        walker.WalkDOM(docAsNode, this);

        mCurrentDataPath = oldDataPath;
        mCurrentDataPathIsRelative = oldDataPathIsRelative;
        mCurrentRelativePathToData = oldCurrentRelativePathToData;
    }
    else
    {
        // Set the document base to ensure relative links still work
        SetDocumentBase(aDocument, mCurrentBaseURI);

        // Save the document
        nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(docAsNode);

        nsAutoString contentType; 
        if (mContentType.Length() > 0)
        {
            contentType = mContentType;
        }
        else
        {
            // TODO infer the other content type - from the DOM document maybe?
            contentType = NS_LITERAL_STRING("text/html");
        }
        nsAutoString charType; // Empty

        rv = diskDoc->SaveFile(
            aFile,
            mReplaceExisting,
            PR_TRUE, // save as a copy (i.e. don't change it's dirty status)
            contentType.get(),
            charType.get(),
            mEncodingFlags,
            mWrapColumn);
    }

    mCurrentBaseURI = oldBaseURI;

    return NS_OK;
}

nsresult nsWebBrowserPersist::SaveDocuments()
{
    nsresult rv;

    // Iterate through all queued documents, saving them to file and fixing
    // them up on the way.

    PRInt32 i;
    for (i = 0; i < mDocList.Count(); i++)
    {
        DocData *docData = (DocData *) mDocList.ElementAt(i);
        NS_ENSURE_TRUE(docData, NS_ERROR_FAILURE);

        mCurrentBaseURI = docData->mBaseURI;

        // Save the document, fixing it up with the new URIs as we do
        nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(docData->mDocument);
        nsAutoString charType; // Empty
        
        nsEncoderNodeFixup *nodeFixup;
        nodeFixup = new nsEncoderNodeFixup;
        nodeFixup->mWebBrowserPersist = this;

        // Remove document base so relative links work on the persisted version
        SetDocumentBase(docData->mDocument, nsnull);

        nsCOMPtr<nsIDocument> docAsDoc = do_QueryInterface(docData->mDocument);

        nsAutoString contentType;
        if (mContentType.Length() > 0)
        {
            contentType = mContentType;
        }
        else
        {
            // TODO infer the other content type - from the DOM document maybe?
            contentType.AssignWithConversion("text/html");
        }

        // Save the document, fixing up the links as it goes out
        rv = SaveDocumentWithFixup(
            docAsDoc,
            nodeFixup,
            docData->mFile,
            mReplaceExisting,
            contentType,
            charType,
            mEncodingFlags);

        // Restore the document's BASE URL
        SetDocumentBase(docData->mDocument, docData->mBaseURI);

        delete docData;
    }

    // Empty list
    mDocList.Clear();

    return NS_OK;
}

void nsWebBrowserPersist::CleanUp()
{
    mURIMap.Enumerate(EnumCleanupURIMap, this);
    mURIMap.Reset();
    mOutputMap.Enumerate(EnumCleanupOutputMap, this);
    mOutputMap.Reset();
    PRInt32 i;
    for (i = 0; i < mDocList.Count(); i++)
    {
        DocData *docData = (DocData *) mDocList.ElementAt(i);
        delete docData;
    }
    mDocList.Clear();
}


nsresult
nsWebBrowserPersist::CalculateAndAppendFileExt(nsIURI *aURI, nsIChannel *aChannel)
{
    nsresult rv;

    if (!mMIMEService)
    {
        mMIMEService = do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
        NS_ENSURE_TRUE(mMIMEService, NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsILocalFile> localFile;
    GetLocalFileFromURI(aURI, getter_AddRefs(localFile));
    NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);

    nsXPIDLCString contentType;

    // Get the content type from the channel
    aChannel->GetContentType(getter_Copies(contentType));

    // Get the content type from the MIME service
    if (contentType.Length() == 0)
    {
        nsCOMPtr<nsIURI> uri;
        aChannel->GetOriginalURI(getter_AddRefs(uri));
        rv = mMIMEService->GetTypeFromURI(uri, getter_Copies(contentType));
    }

    // Append the extension onto the file
    if (contentType.Length())
    {
        nsCOMPtr<nsIMIMEInfo> mimeInfo;
        mMIMEService->GetFromMIMEType(
            contentType.get(), getter_AddRefs(mimeInfo));

        if (mimeInfo)
        {
            nsXPIDLCString fileName;
            localFile->GetLeafName(getter_Copies(fileName));

            nsCString newFileName;
            newFileName.Assign(fileName);

            // Test if the current extension is current for the mime type
            PRBool hasExtension = PR_FALSE;
            PRInt32 ext = newFileName.RFind(".");
            if (ext != -1)
            {
                mimeInfo->ExtensionExists(newFileName.get() + ext + 1, &hasExtension);
            }

            // Append the mime file extension
            nsXPIDLCString fileExt;
            if (!hasExtension &&
                NS_SUCCEEDED(mimeInfo->GetPrimaryExtension(getter_Copies(fileExt))))
            {
                newFileName.Append(".");
                newFileName.Append(fileExt.get());
                localFile->SetLeafName(newFileName.get());
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

    // Resync the URI with the file after the extension has been appended
    
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    fileURL->SetFile(localFile);

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeOutputStream(
    nsIURI *aURI, nsIChannel *aChannel, nsIOutputStream **aOutputStream)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aChannel);
    NS_ENSURE_ARG_POINTER(aOutputStream);

    nsCOMPtr<nsILocalFile> localFile;
    GetLocalFileFromURI(aURI, getter_AddRefs(localFile));
    NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);

    nsresult rv = MakeOutputStreamFromFile(localFile, aChannel, aOutputStream);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeOutputStreamFromFile(
    nsILocalFile *aFile, nsIChannel *aChannel, nsIOutputStream **aOutputStream)
{
    nsresult rv = NS_OK;

    NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
    nsCOMPtr<nsIFileTransportService> fts =
        do_GetService(kFileTransportServiceCID, &rv);

    if (NS_FAILED(rv))
    {
        return NS_ERROR_FAILURE;
    }

    // Open a channel on the local file
    nsCOMPtr<nsITransport> outputChannel;
    rv = fts->CreateTransport(aFile, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
        0664, getter_AddRefs(outputChannel));
    if (NS_FAILED(rv))
    {
        return NS_ERROR_FAILURE;
    }

    // Create an output stream to store the data
    outputChannel->OpenOutputStream(0, -1, 0, aOutputStream);
    NS_ENSURE_TRUE(*aOutputStream, NS_ERROR_FAILURE);

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

    // Save the documents
    if (NS_SUCCEEDED(aResult))
    {
        SaveDocuments();
    }

    // Cleanup the channels
    mCompleted = PR_TRUE;
    CleanUp();
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
    if (mOutputMap.Count() > 0)
    {
        // Total up the progress of each output stream
        mTotalCurrentProgress = 0;
        mTotalMaxProgress = 0;
        mOutputMap.Enumerate(EnumCalcProgress, this);
    }
    else
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
    pthis->mTotalCurrentProgress += data->mSelfProgress;
    pthis->mTotalMaxProgress += data->mSelfProgressMax;
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
    rv = NS_NewURI(getter_AddRefs(uri), ((nsCStringKey *) aKey)->GetString());
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Make a URI to save the data to
    nsCOMPtr<nsIURI> fileAsURI;
    rv = data->mDataPath->Clone(getter_AddRefs(fileAsURI));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = pthis->AppendPathToURI(fileAsURI, data->mFilename);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = pthis->SaveURIInternal(uri, nsnull, fileAsURI, PR_TRUE);

    // Store the actual object because once it's persisted this
    // will be fixed up with the right file extension.

    data->mFile = fileAsURI;
    data->mSaved = PR_TRUE;

    NS_ENSURE_SUCCESS(rv, PR_FALSE);

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


nsresult
nsWebBrowserPersist::OnWalkDOMNode(nsIDOMNode *aNode, PRBool *aAbort)
{
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
    
    nsCOMPtr<nsIDOMHTMLScriptElement> nodeAsScript = do_QueryInterface(aNode);
    if (nodeAsScript)
    {
        StoreURIAttribute(aNode, "src");
        return NS_OK;
    }
    
    nsCOMPtr<nsIDOMHTMLLinkElement> nodeAsLink = do_QueryInterface(aNode);
    if (nodeAsLink)
    {
        StoreURIAttribute(aNode, "href");
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
            // TODO how do we get the proper extension (or MIME type) from a DOM document?
            data->mSubFrameExt.Assign(NS_LITERAL_STRING(".htm"));
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
        URIData *data = nsnull;
        StoreURIAttribute(aNode, "src", PR_FALSE, &data);
        if (data)
        {
            data->mIsSubFrame = PR_TRUE;
            // TODO how do we get the proper extension (or MIME type) from a DOM document?
            data->mSubFrameExt.Assign(NS_LITERAL_STRING(".htm"));
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
nsWebBrowserPersist::CloneNodeWithFixedUpURIAttributes(
    nsIDOMNode *aNodeIn, nsIDOMNode **aNodeOut)
{
    *aNodeOut = nsnull;

    // Fix up href and file links in the elements

    nsCOMPtr<nsIDOMHTMLAnchorElement> nodeAsAnchor = do_QueryInterface(aNodeIn);
    if (nodeAsAnchor)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupAnchor(*aNodeOut);
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLAreaElement> nodeAsArea = do_QueryInterface(aNodeIn);
    if (nodeAsArea)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupAnchor(*aNodeOut);
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLBodyElement> nodeAsBody = do_QueryInterface(aNodeIn);
    if (nodeAsBody)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupNodeAttribute(*aNodeOut, "background");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLImageElement> nodeAsImage = do_QueryInterface(aNodeIn);
    if (nodeAsImage)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupAnchor(*aNodeOut);
        FixupNodeAttribute(*aNodeOut, "src");
        return NS_OK;
    }
    
    nsCOMPtr<nsIDOMHTMLScriptElement> nodeAsScript = do_QueryInterface(aNodeIn);
    if (nodeAsScript)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupNodeAttribute(*aNodeOut, "src");
        return NS_OK;
    }
    
    nsCOMPtr<nsIDOMHTMLLinkElement> nodeAsLink = do_QueryInterface(aNodeIn);
    if (nodeAsLink)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupNodeAttribute(*aNodeOut, "href");
        // TODO if "type" attribute == "text/css"
        //        fixup stylesheet
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLFrameElement> nodeAsFrame = do_QueryInterface(aNodeIn);
    if (nodeAsFrame)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupNodeAttribute(*aNodeOut, "src");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLIFrameElement> nodeAsIFrame = do_QueryInterface(aNodeIn);
    if (nodeAsIFrame)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupNodeAttribute(*aNodeOut, "src");
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLInputElement> nodeAsInput = do_QueryInterface(aNodeIn);
    if (nodeAsInput)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupNodeAttribute(*aNodeOut, "src");
        return NS_OK;
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
        nsCAutoString oldCValue; oldCValue.AssignWithConversion(oldValue);

        // Test whether this URL should be persisted
        PRBool shouldPersistURI = PR_TRUE;
        if (oldCValue.EqualsWithConversion("about:", PR_TRUE, 6) ||
            oldCValue.EqualsWithConversion("news:", PR_TRUE, 5) ||
            oldCValue.EqualsWithConversion("snews:", PR_TRUE, 6) ||
            oldCValue.EqualsWithConversion("ldap:", PR_TRUE, 5) ||
            oldCValue.EqualsWithConversion("ldaps:", PR_TRUE, 6) ||
            oldCValue.EqualsWithConversion("mailto:", PR_TRUE, 7) ||
            oldCValue.EqualsWithConversion("finger:", PR_TRUE, 7) ||
            oldCValue.EqualsWithConversion("telnet:", PR_TRUE, 7) ||
            oldCValue.EqualsWithConversion("gopher:", PR_TRUE, 7) ||
            oldCValue.EqualsWithConversion("javascript:", PR_TRUE, 11) ||
            oldCValue.EqualsWithConversion("view-source:", PR_TRUE, 12) ||
            oldCValue.EqualsWithConversion("irc:", PR_TRUE, 4) ||
            oldCValue.EqualsWithConversion("mailbox:", PR_TRUE, 8))
        {
            shouldPersistURI = PR_FALSE;
        }

        if (shouldPersistURI)
        {
            URIData *data = nsnull;
            MakeAndStoreLocalFilenameInURIMap(oldCValue.get(), aNeedsPersisting, &data);
            if (aData)
            {
                *aData = data;
            }
        }
    }

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
        nsString oldValue;
        attrNode->GetNodeValue(oldValue);
        nsCString oldCValue; oldCValue.AssignWithConversion(oldValue);

        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), oldCValue.get(), mCurrentBaseURI);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
        nsXPIDLCString spec;
        rv = uri->GetSpec(getter_Copies(spec));
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        // Search for the URI in the map and replace it with the local file
        nsCStringKey key(spec.get());
        if (mURIMap.Exists(&key))
        {
            URIData *data = (URIData *) mURIMap.Get(&key);
            nsCOMPtr<nsIURI> fileAsURI = data->mFile;
            if (!fileAsURI)
            {
                rv = data->mDataPath->Clone(getter_AddRefs(fileAsURI));
                NS_ENSURE_SUCCESS(rv, PR_FALSE);
                rv = AppendPathToURI(fileAsURI, data->mFilename);
                NS_ENSURE_SUCCESS(rv, PR_FALSE);
            }
            nsAutoString newValue;

            // Use relative or absolute links
            nsCOMPtr<nsILocalFile> localFile;
            GetLocalFileFromURI(fileAsURI, getter_AddRefs(localFile));
            if (data->mDataPathIsRelative && localFile)
            {
                nsXPIDLCString filename;
                localFile->GetLeafName(getter_Copies(filename));
                nsCAutoString rawPathURL;
                rawPathURL.Assign(data->mRelativePathToData);
                rawPathURL.Append(filename.get());
                newValue.AssignWithConversion(nsEscape(rawPathURL.get(), url_Path));
            }
            else
            {
                nsXPIDLCString fileurl;
                NS_GetURLSpecFromFile(localFile, getter_Copies(fileurl));
                newValue.AssignWithConversion(fileurl);
            }
            if (data->mIsSubFrame)
            {
                newValue.Append(data->mSubFrameExt);
            }
            attrNode->SetNodeValue(newValue);
        }
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::FixupAnchor(nsIDOMNode *aNode)
{
    NS_ENSURE_ARG_POINTER(aNode);

    nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
    nsCOMPtr<nsIDOMNode> attrNode;
    nsresult rv = aNode->GetAttributes(getter_AddRefs(attrMap));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // Make all anchor links absolute so they point off onto the Internet
    nsString attribute(NS_LITERAL_STRING("href"));
    rv = attrMap->GetNamedItem(attribute, getter_AddRefs(attrNode));
    if (attrNode)
    {
        nsString oldValue;
        attrNode->GetNodeValue(oldValue);
        nsCString oldCValue; oldCValue.AssignWithConversion(oldValue);

        // Skip self-referencing bookmarks
        if (oldCValue.Length() > 0 && oldCValue.CharAt(0) == '#')
        {
            return NS_OK;
        }

        // Make a new URI to replace the current one
        nsCOMPtr<nsIURI> newURI;
        rv = NS_NewURI(getter_AddRefs(newURI), oldCValue.get(), mCurrentBaseURI);
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLCString uriSpec;
            newURI->GetSpec(getter_Copies(uriSpec));
            nsAutoString newValue; newValue.AssignWithConversion(uriSpec);
            attrNode->SetNodeValue(newValue);
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
    newFrameDataPath.Append(NS_LITERAL_STRING("_data"));
    rv = AppendPathToURI(frameDataURI, newFrameDataPath);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    SaveDocumentInternal(aFrameContent, frameURI, frameDataURI);

    return NS_OK;
}


nsresult
nsWebBrowserPersist::SaveDocumentWithFixup(
    nsIDocument *aDocument, nsIDocumentEncoderNodeFixup *aNodeFixup,
    nsIURI *aFile, PRBool aReplaceExisting, const nsString &aFormatType,
    const nsString &aSaveCharset, PRUint32 aFlags)
{
    // NOTE: This function is based off of nsDocument::SaveFile

    NS_ENSURE_ARG_POINTER(aFile);
    
    nsresult  rv = NS_OK;
    nsCOMPtr<nsIOutputStream> outputStream;

    nsCOMPtr<nsILocalFile> localFile;
    GetLocalFileFromURI(aFile, getter_AddRefs(localFile));
    NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);

    // if we're not replacing an existing file but the file
    // exists, something is wrong
    PRBool fileExists = PR_FALSE;
    rv = localFile->Exists(&fileExists);
    if (NS_FAILED(rv)) return rv;

    if (!aReplaceExisting && fileExists)
      return NS_ERROR_FAILURE;				// where are the file I/O errors?

    nsCOMPtr<nsIFileOutputStream> fileOutputStream =
        do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fileOutputStream->Init(localFile, -1, -1);
    if (NS_FAILED(rv)) return rv;

    outputStream = do_QueryInterface(fileOutputStream);

    // Get a document encoder instance
    nsCAutoString contractID(NS_DOC_ENCODER_CONTRACTID_BASE);
    contractID.AppendWithConversion(aFormatType);
    
    nsCOMPtr<nsIDocumentEncoder> encoder = do_CreateInstance(contractID.get(), &rv);
    if (NS_FAILED(rv))
      return rv;

    rv = encoder->Init(aDocument, aFormatType, aFlags);
    if (NS_FAILED(rv))
    {
        return rv;
    }

    // Set the node fixup callback
    encoder->SetNodeFixup(aNodeFixup);

    nsAutoString charsetStr(aSaveCharset);
    if (charsetStr.Length() == 0)
    {
        rv = aDocument->GetDocumentCharacterSet(charsetStr);
        if(NS_FAILED(rv))
        {
            charsetStr.Assign(NS_LITERAL_STRING("ISO-8859-1")); 
        }
    }
    encoder->SetCharset(charsetStr);

    rv = encoder->EncodeToStream(outputStream);

    return rv;
}


nsresult
nsWebBrowserPersist::MakeAndStoreLocalFilenameInURIMap(
    const char *aURI, PRBool aNeedsPersisting, URIData **aData)
{
    NS_ENSURE_ARG_POINTER(aURI);

    nsresult rv;

    // Make a URI
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), aURI, mCurrentBaseURI);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    nsXPIDLCString spec;
    rv = uri->GetSpec(getter_Copies(spec));
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
    MakeFilenameFromURI(uri, filename);

    // Store the file name
    URIData *data = new URIData;
    if (!data)
    {
        return NS_ERROR_FAILURE;
    }
    data->mNeedsPersisting = aNeedsPersisting;
    data->mFilename = filename;
    data->mSaved = PR_FALSE;
    data->mDataPath = mCurrentDataPath;
    data->mDataPathIsRelative = mCurrentDataPathIsRelative;
    data->mRelativePathToData = mCurrentRelativePathToData;

    mURIMap.Put(&key, data);
    if (aData)
    {
        *aData = data;
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeFilenameFromURI(nsIURI *aURI, nsString &aFilename)
{
    // Try to get filename from the URI.
    aFilename.Truncate(0);

    // Get a suggested file name from the URL but strip it of characters
    // likely to cause the name to be illegal.

    nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
    if (url)
    {
        char *nameFromURL = nsnull;
        url->GetFileName(&nameFromURL);
        if (nameFromURL)
        {
            const PRInt32 kMaxFileNameLength = 20;
            // Unescape the file name (GetFileName escapes it)
            nsString fileName;
            PRInt32 length = 0;
            char *p = nsUnescape(nameFromURL);
            for (;*p && *p != ';' && *p != '?' && *p != '#' && *p != '.' &&
                  length < kMaxFileNameLength
                 ;p++, length++)
            {
                if (nsCRT::IsAsciiAlpha(*p) || nsCRT::IsAsciiDigit(*p)
                    || *p == '.'
#ifdef WIN32
                    || *p == '-'
                    || (*p == ' ' && length != kMaxFileNameLength - 1)
#endif
                    )
                {
                    fileName.AppendWithConversion(*p);
                }
            }
            aFilename = fileName;
            nsCRT::free(nameFromURL);
        }
    }

    // A last resort effort if the URL is junk
    if (aFilename.Length() == 0)
    {
        // file_X is a dumb name but it's better than nothing
        char * tmp = PR_smprintf("file_%d", mFileCounter++);
        if (tmp == nsnull)
        {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        aFilename.AssignWithConversion(tmp);
        PR_smprintf_free(tmp);
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::SetDocumentBase(
    nsIDOMDocument *aDocument, nsIURI *aBaseURI)
{
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(aDocument);
    if (!htmlDoc)
    {
        return NS_ERROR_FAILURE;
    }

    // Find the head element
    nsCOMPtr<nsIDOMElement> headElement;
    nsCOMPtr<nsIDOMNodeList> headList;
    aDocument->GetElementsByTagName(
        NS_ConvertASCIItoUCS2("head"), getter_AddRefs(headList));
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
        aDocument->CreateElement(
            NS_ConvertASCIItoUCS2("head"), getter_AddRefs(headElement));
        aDocument->GetFirstChild(getter_AddRefs(firstChildNode));
        aDocument->InsertBefore(headElement, firstChildNode, getter_AddRefs(newNode));
    }
    if (!headElement)
    {
        return NS_ERROR_FAILURE;
    }

    // Find or create the BASE element
    nsCOMPtr<nsIDOMElement> baseElement;
    nsCOMPtr<nsIDOMNodeList> baseList;
    headElement->GetElementsByTagName(
        NS_ConvertASCIItoUCS2("base"), getter_AddRefs(baseList));
    if (baseList)
    {
        nsCOMPtr<nsIDOMNode> baseNode;
        baseList->Item(0, getter_AddRefs(baseNode));
        baseElement = do_QueryInterface(baseNode);
    }

    // Add or remove the BASE element
    if (aBaseURI)
    {
        if (!baseElement)
        {
            nsCOMPtr<nsIDOMNode> newNode;
            aDocument->CreateElement(
                NS_ConvertASCIItoUCS2("base"), getter_AddRefs(baseElement));
            headElement->AppendChild(baseElement, getter_AddRefs(newNode));
        }
        if (!baseElement)
        {
            return NS_ERROR_FAILURE;
        }
        nsXPIDLCString uriSpec;
        aBaseURI->GetSpec(getter_Copies(uriSpec));
        nsString href; href.AssignWithConversion(uriSpec);
        baseElement->SetAttribute(NS_ConvertASCIItoUCS2("href"), href);
    }
    else
    {
        if (baseElement)
        {
            nsCOMPtr<nsIDOMNode> node;
            headElement->RemoveChild(baseElement, getter_AddRefs(node));
        }
    }

    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////


nsEncoderNodeFixup::nsEncoderNodeFixup() : mWebBrowserPersist(nsnull)
{
    NS_INIT_REFCNT();
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
    if (type == nsIDOMNode::ELEMENT_NODE)
    {
        mWebBrowserPersist->CloneNodeWithFixedUpURIAttributes(aNode, aOutNode);
    }

    return NS_OK;
}
