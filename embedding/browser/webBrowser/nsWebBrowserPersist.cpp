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

#include "nsNetUtil.h"
#include "nsFileStream.h"
#include "nsIFileTransportService.h"
#include "nsIHTTPChannel.h"
#include "nsEscape.h"

#include "nsCExternalHandlerService.h"

#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDiskDocument.h"

#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLDocument.h"

#include "nsWebBrowserPersist.h"

struct URIData
{
    PRBool   mNeedsPersisting;
    nsString mFilename;
};

nsWebBrowserPersist::nsWebBrowserPersist() :
    mFirstAndOnlyUse(PR_TRUE),
    mFileCounter(1),
    mFrameCounter(1),
    mTaskCounter(0)
{
    NS_INIT_REFCNT();
}

nsWebBrowserPersist::~nsWebBrowserPersist()
{
    CleanUp();
}

void nsWebBrowserPersist::CleanUp()
{    if (mInputStream)
    {
        mInputStream->Close();
        mInputStream = nsnull;
    }
    mInputChannel = nsnull;
    if (mOutputStream)
    {
        mOutputStream->Close();
        mOutputStream = nsnull;
    }
    mOutputChannel = nsnull;
}

NS_IMPL_ADDREF(nsWebBrowserPersist)
NS_IMPL_RELEASE(nsWebBrowserPersist)

NS_INTERFACE_MAP_BEGIN(nsWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersistProgress)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsWebBrowserPersist::nsIWebBrowserPersist
//*****************************************************************************

/* attribute nsIWebBrowserPersistProgress progressListener; */
NS_IMETHODIMP nsWebBrowserPersist::GetProgressListener(nsIWebBrowserPersistProgress * *aProgressListener)
{
    NS_ENSURE_ARG_POINTER(aProgressListener);
    *aProgressListener = mProgressListener;
    NS_IF_ADDREF(*aProgressListener);
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserPersist::SetProgressListener(nsIWebBrowserPersistProgress * aProgressListener)
{
    mProgressListener = aProgressListener;
    return NS_OK;
}

/* void saveURI (in nsIURI aURI, in string aFileName); */
NS_IMETHODIMP nsWebBrowserPersist::SaveURI(nsIURI *aURI, nsIInputStream *aPostData, const char *aFileName)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aFileName);
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);

    nsresult rv = NS_OK;
    
    mURI = aURI;
    mFirstAndOnlyUse = PR_FALSE; // Stop people from reusing this object!

    OnBeginDownload();

    // Open a channel to the URI
    nsCOMPtr<nsIChannel> inputChannel;
    rv = NS_OpenURI(getter_AddRefs(inputChannel), aURI, nsnull);
    
    if (NS_FAILED(rv) || inputChannel == nsnull)
    {
        OnEndDownload();
        return NS_ERROR_FAILURE;
    }
    
    // Post data
    if (aPostData)
    {
        nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(inputChannel));
        if (httpChannel)
        {
            nsCOMPtr<nsIRandomAccessStore> stream(do_QueryInterface(aPostData));
            if (stream)
            {
                // Rewind the postdata stream
                stream->Seek(PR_SEEK_SET, 0);
                // Attach the postdata to the http channel
                httpChannel->SetUploadStream(aPostData);
                nsCOMPtr<nsIAtom> method = NS_NewAtom("POST");
                httpChannel->SetRequestMethod(method);
            }
        }
    }

    // Query the content type
    nsXPIDLCString contentType;
    inputChannel->GetContentType(getter_Copies(contentType));
    
    NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    
    if (NS_FAILED(rv))
    {
        OnEndDownload();
        return NS_ERROR_FAILURE;
    }

    // Create a local file object
    nsString filename; filename.AssignWithConversion(aFileName);
    nsFileSpec target(filename);

    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(target, PR_FALSE, getter_AddRefs(file));
    if (NS_FAILED(rv))
    {
        OnEndDownload();
        return NS_ERROR_FAILURE;
    }
    
    // Open a channel on the local file
    nsCOMPtr<nsIChannel> outputChannel;
    rv = fts->CreateTransport(file, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                              0664, getter_AddRefs(outputChannel));
    if (NS_FAILED(rv))
    {
        OnEndDownload();
        return NS_ERROR_FAILURE;
    }
    
    mOutputChannel = outputChannel;
    
    // Read from the input channel
    rv = inputChannel->AsyncRead(this, nsnull);
    if (NS_FAILED(rv))
    {
        OnEndDownload();
        return NS_ERROR_FAILURE;
    }
    
    nsCOMPtr<nsIInputStream> inStream;
    rv = inputChannel->OpenInputStream(getter_AddRefs(inStream));
    if (NS_FAILED(rv))
    {
        OnEndDownload();
        return NS_ERROR_FAILURE;
    }
  
    mInputChannel = inputChannel;
    mInputStream = inStream;

    // Get the output channel ready for writing
    rv = outputChannel->AsyncWrite(inStream, NS_STATIC_CAST(nsIStreamObserver *, this), nsnull);
    if (NS_FAILED(rv))
    {
        OnEndDownload();
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/* void saveCurrentURI (in string aFileName); */
NS_IMETHODIMP nsWebBrowserPersist::SaveCurrentURI(const char *aFileName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void saveDocument (in nsIDOMDocument document); */
NS_IMETHODIMP nsWebBrowserPersist::SaveDocument(nsIDOMDocument *aDocument, const char *aFileName, const char *aDataPath)
{
    NS_ENSURE_ARG_POINTER(aDocument);
    NS_ENSURE_ARG_POINTER(aFileName);
    NS_ENSURE_TRUE(mFirstAndOnlyUse, NS_ERROR_FAILURE);

    mFirstAndOnlyUse = PR_FALSE; // Stop people from reusing this object!

    OnBeginDownload();

    nsresult rv;
    nsCOMPtr<nsIDOMNode> docAsNode = do_QueryInterface(aDocument);

    // Persist the main document
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
    mURI = do_QueryInterface(doc->GetDocumentURL());

    // Store the base URI
    doc->GetBaseURL(*getter_AddRefs(mBaseURI));

    // Does the caller want to fixup the referenced URIs and save those too?
    if (aDataPath)
    {
        // Sanity check & create the specified data path
        nsCOMPtr<nsILocalFile> dataPath;
        rv = NS_NewLocalFile(aDataPath, PR_FALSE, getter_AddRefs(dataPath));
        dataPath->Create(nsILocalFile::DIRECTORY_TYPE, 0664);
        PRBool exists = PR_FALSE;
        PRBool isDirectory = PR_FALSE;
        dataPath->Exists(&exists);
        dataPath->IsDirectory(&isDirectory);
        if (!exists || !isDirectory)
        {
            OnEndDownload();
            return NS_ERROR_FAILURE;
        }

        // Try and get the MIME lookup service
        if (!mMIMEService)
        {
            mMIMEService = do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
        }

        // Walk the DOM gathering a list of externally referenced URIs in the uri map
        mDataPath = aDataPath;
        nsDOMWalker walker;
        walker.WalkDOM(docAsNode, this);
        // Persist each file in the uri map
        mURIMap.Enumerate(PersistURIs, this);

        // Save the document, fixing it up with the new URIs as we do
        nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(docAsNode);
        nsString contentType; contentType.AssignWithConversion("text/html"); // TODO
        nsString charType; // Empty
        nsFileSpec fileSpec(aFileName);

        nsEncoderNodeFixup *nodeFixup;
        nodeFixup = new nsEncoderNodeFixup;
        nodeFixup->mWebBrowserPersist = this;

        // Set the document base to ensure relative links still work
        SetDocumentBase(aDocument, mBaseURI);
 
        // Save the document, fixing up the links as it goes out
        rv = SaveDocumentToFileWithFixup(
            doc,
            nodeFixup,
            &fileSpec,
            PR_TRUE  /* replace existing file */,
            PR_TRUE, /* save as a copy */
            contentType,
            charType,
            0);

        mURIMap.Enumerate(CleanupURIMap, this);
        mURIMap.Reset();
    }
    else
    {
        // Set the document base to ensure relative links still work
        SetDocumentBase(aDocument, mBaseURI);

        // Save the document
        nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(docAsNode);
        nsString contentType; contentType.AssignWithConversion("text/html"); // TODO
        nsString charType; // Empty
        nsFileSpec fileSpec(aFileName);
        rv = diskDoc->SaveFile(
            &fileSpec,
            PR_TRUE  /* replace existing file */,
            PR_TRUE, /* save as a copy */
            contentType,
            charType,
            0);
    }

    OnEndDownload();
    
    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist::nsIWebBrowserPersistProgress
//*****************************************************************************


/* void OnProgress (in unsigned long aStatus, in string aURI, out boolean aAbort); */
NS_IMETHODIMP nsWebBrowserPersist::OnProgress(PRUint32 aStatus, nsIURI *aURI, PRBool *aAbort)
{
    if (mProgressListener)
    {
        if (aStatus & PROGRESS_STARTED)
        {
            // Listener only needs to know if this is the first thing to start
            if (mTaskCounter == 0)
            {
                mProgressListener->OnProgress(PROGRESS_STARTED, nsnull, aAbort);
            }
            mTaskCounter++;
        }
        else if (aStatus & PROGRESS_FINISHED)
        {
            mTaskCounter--;
            // Listener only needs to know if this is the last thing to finish
            if (mTaskCounter == 0)
            {
                mProgressListener->OnProgress(PROGRESS_FINISHED, nsnull, aAbort);
                mProgressListener = nsnull;
            }
        }
        else
        {
            // Other notifications
            mProgressListener->OnProgress(aStatus, aURI, aAbort);
        }
    }
    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist::nsIStreamObserver
//*****************************************************************************


/* void onStartRequest (in nsIChannel channel, in nsISupports ctxt); */
NS_IMETHODIMP nsWebBrowserPersist::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
    nsresult rv = mOutputChannel->OpenOutputStream(getter_AddRefs(mOutputStream));
    return rv;
}
 
/* void onStopRequest (in nsIChannel channel, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP nsWebBrowserPersist::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *statusArg)
{
    OnEndDownload();
    CleanUp();
    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist::nsIStreamListener
//*****************************************************************************


NS_IMETHODIMP nsWebBrowserPersist::OnDataAvailable(nsIChannel *aChannel, nsISupports *aContext, nsIInputStream *aIStream, PRUint32 aOffset, PRUint32 aLength)
{
    nsresult rv = NS_OK;
    unsigned long bytesRemaining = aLength;
    PRBool cancel = PR_FALSE;

    while (bytesRemaining)
    {
        char buffer[8192];
        unsigned int bytesRead;
        
        rv = aIStream->Read(buffer, PR_MIN(sizeof(buffer), bytesRemaining), &bytesRead);
        if (NS_SUCCEEDED(rv))
        {
            unsigned int bytesWritten;
            rv = mOutputStream->Write(buffer, bytesRead, &bytesWritten);
            if (NS_SUCCEEDED(rv) && bytesWritten == bytesRead)
            {
                bytesRemaining -= bytesWritten;
            }
            else
            {
                // Disaster - can't write out the bytes - disk full / permission?
                cancel = PR_TRUE;
                break;
            }
        }
        else
        {
            // Disaster - can't read the bytes - broken link / file error?
            cancel = PR_TRUE;
            break;
        }
    }

    // Cancel reading?
    if (cancel)
    {
        if (aChannel)
        {
            aChannel->Cancel(NS_BINDING_ABORTED);
        }
        CleanUp();
        OnEndDownload();
    }

    return NS_OK;
}


//*****************************************************************************
// nsWebBrowserPersist private methods
//*****************************************************************************


void
nsWebBrowserPersist::OnBeginDownload()
{
    PRBool abortOperation = PR_FALSE;
    OnProgress(PROGRESS_STARTED, nsnull, &abortOperation);
    OnProgress(PROGRESS_START_URI, mURI, &abortOperation);
}


void
nsWebBrowserPersist::OnEndDownload()
{
    PRBool abortOperation = PR_FALSE;
    OnProgress(PROGRESS_END_URI, mURI, &abortOperation);
    OnProgress(PROGRESS_FINISHED, nsnull, &abortOperation);
}


PRBool PR_CALLBACK
nsWebBrowserPersist::PersistURIs(nsHashKey *aKey, void *aData, void* closure)
{
    URIData *data = (URIData *) aData;
    if (!data->mNeedsPersisting)
    {
        return PR_TRUE;
    }

    nsCString filename; filename.AssignWithConversion(data->mFilename);
    nsWebBrowserPersist *pthis = (nsWebBrowserPersist *) closure;
    nsresult rv;

    // Save the data to a local file
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), ((nsCStringKey *) aKey)->GetString(), pthis->mBaseURI);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsILocalFile> file;
    nsXPIDLCString filePath;
    rv = NS_NewLocalFile(pthis->mDataPath, PR_FALSE, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    file->AppendRelativePath(filename);
    file->GetPath(getter_Copies(filePath));

    // Create a persistence object to save the URI to
    nsWebBrowserPersist *persist = new nsWebBrowserPersist();
    if (persist == nsnull)
    {
        return PR_FALSE;
    }
    persist->AddRef();
    persist->SetProgressListener(NS_STATIC_CAST(nsIWebBrowserPersistProgress *, pthis));
    rv = persist->SaveURI(uri, nsnull, filePath);
    persist->Release();

    return PR_TRUE;
}


PRBool PR_CALLBACK
nsWebBrowserPersist::CleanupURIMap(nsHashKey *aKey, void *aData, void* closure)
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
    nsresult rv = NS_OK;

    // Test the node to see if it's an image, frame, iframe, css, js
    nsCOMPtr<nsIDOMHTMLImageElement> nodeAsImage = do_QueryInterface(aNode);
    if (nodeAsImage)
    {
        StoreURIAttribute(aNode, "src");
        StoreURIAttribute(aNode, "lowsrc");
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
        nsString filename;
        StoreURIAttribute(aNode, "src", PR_FALSE, &filename);
        // Save the frame content
        nsCOMPtr<nsIDOMDocument> content;
        nodeAsFrame->GetContentDocument(getter_AddRefs(content));
        if (content)
        {
            SaveSubframeContent(content, filename);
        }
        return NS_OK;
    }

    nsCOMPtr<nsIDOMHTMLIFrameElement> nodeAsIFrame = do_QueryInterface(aNode);
    if (nodeAsIFrame)
    {
        nsString filename;
        StoreURIAttribute(aNode, "src", PR_FALSE, &filename);
        // Save the frame content
        nsCOMPtr<nsIDOMDocument> content;
        nodeAsIFrame->GetContentDocument(getter_AddRefs(content));
        if (content)
        {
            SaveSubframeContent(content, filename);
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
nsWebBrowserPersist::CloneNodeWithFixedUpURIAttributes(nsIDOMNode *aNodeIn, nsIDOMNode **aNodeOut)
{
    nsresult rv = NS_OK;

    *aNodeOut = nsnull;

    // Test the node to see if it's an image, frame, iframe, css, js
    nsCOMPtr<nsIDOMHTMLImageElement> nodeAsImage = do_QueryInterface(aNodeIn);
    if (nodeAsImage)
    {
        aNodeIn->CloneNode(PR_FALSE, aNodeOut);
        FixupNodeAttribute(*aNodeOut, "src");
        FixupNodeAttribute(*aNodeOut, "lowsrc");
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
nsWebBrowserPersist::StoreURIAttribute(nsIDOMNode *aNode, char *aAttribute, PRBool aNeedsPersisting, nsString *aFilename)
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
        nsString oldValue;
        attrNode->GetNodeValue(oldValue);
        nsCString oldCValue; oldCValue.AssignWithConversion(oldValue);
        nsString filename;
        MakeAndStoreLocalFilenameInURIMap(oldCValue, filename, aNeedsPersisting);
        if (aFilename)
        {
            *aFilename = filename;
        }
    }

    return NS_OK;
}

nsresult
nsWebBrowserPersist::FixupNodeAttribute(nsIDOMNode *aNode, char *aAttribute)
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

        // Search for the URI in the map and replace it with the local file
        nsCStringKey key(oldCValue);
        if (mURIMap.Exists(&key))
        {
            nsString filename = ((URIData *) mURIMap.Get(&key))->mFilename;
            nsFileSpec filespec(mDataPath);
            filespec += filename;
            nsFileURL fileurl(filespec);
            nsString newValue; newValue.AssignWithConversion(fileurl.GetURLString());
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
nsWebBrowserPersist::SaveSubframeContent(nsIDOMDocument *aFrameContent, const nsString &aFilename)
{
    // Work out the path for the frame
    nsFileSpec frameFileSpec(mDataPath);
    frameFileSpec += aFilename;

    // Work out the path for the frame data
    nsFileSpec frameDatapathSpec(mDataPath);
    nsString dataname;
    char * tmp = PR_smprintf("subframe_%d", mFrameCounter++);
    if (tmp == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    dataname.AssignWithConversion(tmp);
    PR_smprintf_free(tmp);
    frameDatapathSpec += dataname;

    // Create a new persistence object to persist the frame
    nsWebBrowserPersist *framePersist = new nsWebBrowserPersist;
    if (framePersist)
    {
        framePersist->AddRef();
        framePersist->SetProgressListener(NS_STATIC_CAST(nsIWebBrowserPersistProgress *, this));
        framePersist->SaveDocument(aFrameContent, frameFileSpec.GetCString(), frameDatapathSpec.GetCString());
        framePersist->Release();
    }

    return NS_OK;
}


nsresult
nsWebBrowserPersist::SaveDocumentToFileWithFixup(
        nsIDocument    *aDocument,
        nsIDocumentEncoderNodeFixup *aNodeFixup,
        nsFileSpec*     aFileSpec,
        PRBool          aReplaceExisting,
        PRBool          aSaveCopy,
        const nsString& aFormatType,
        const nsString& aSaveCharset,
        PRUint32        aFlags)
{
    // NOTE: This function is essentially a copy of nsDocument::SaveFile
    //       with a single line added to set the node fixup call back.
    //       This line is marked.

    if (!aFileSpec)
    {
        return NS_ERROR_NULL_POINTER;
    }
    
    nsresult  rv = NS_OK;

    // if we're not replacing an existing file but the file
    // exists, somethine is wrong
    if (!aReplaceExisting && aFileSpec->Exists())
    {
        return NS_ERROR_FAILURE;        // where are the file I/O errors?
    }
  
    nsOutputFileStream    stream(*aFileSpec);
    // if the stream didn't open, something went wrong
    if (!stream.is_open())
    {
        return NS_BASE_STREAM_CLOSED;
    }

    // Get a document encoder instance:
    nsCOMPtr<nsIDocumentEncoder> encoder;
    char* contractid = (char *)nsMemory::Alloc(strlen(NS_DOC_ENCODER_CONTRACTID_BASE)
                                            + aFormatType.Length() + 1);
    if (! contractid)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    strcpy(contractid, NS_DOC_ENCODER_CONTRACTID_BASE);
    char* type = aFormatType.ToNewCString();
    strcat(contractid, type);
    nsCRT::free(type);
    rv = nsComponentManager::CreateInstance(contractid,
                                          nsnull,
                                          NS_GET_IID(nsIDocumentEncoder),
                                          getter_AddRefs(encoder));
    nsCRT::free(contractid);
    if (NS_FAILED(rv))
    {
        return rv;
    }

    rv = encoder->Init(aDocument, aFormatType, aFlags);
    if (NS_FAILED(rv))
    {
        return rv;
    }

    // BEGIN --- Node fixup callback
    encoder->SetNodeFixup(aNodeFixup);
    // END --- Node fixup callback

    nsAutoString charsetStr(aSaveCharset);
    if (charsetStr.Length() == 0)
    {
        rv = aDocument->GetDocumentCharacterSet(charsetStr);
        if(NS_FAILED(rv))
        {
            charsetStr.AssignWithConversion("ISO-8859-1"); 
        }
    }
    encoder->SetCharset(charsetStr);

    rv = encoder->EncodeToStream(stream.GetIStream());

    return rv;
}


nsresult
nsWebBrowserPersist::MakeAndStoreLocalFilenameInURIMap(const char *aURI, nsString &aFilename, PRBool aNeedsPersisting)
{
    NS_ENSURE_ARG_POINTER(aURI);

    // Create a sensibly named filename for the URI and store in the URI map

    nsCStringKey key(aURI);
    nsString filename;
    if (mURIMap.Exists(&key))
    {
        filename = ((URIData *) mURIMap.Get(&key))->mFilename;
    }
    else
    {
        nsresult rv;

        // Open a channel to the URI
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), aURI, mBaseURI);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);


        nsCOMPtr<nsIChannel> inputChannel;
        rv = NS_OpenURI(getter_AddRefs(inputChannel), uri, nsnull);
        if (NS_FAILED(rv) || inputChannel == nsnull)
        {
            return NS_ERROR_FAILURE;
        }

        // Create a unique file name for the uri
        MakeFilenameFromURI(uri, inputChannel, filename);

        // Query the content type
        nsXPIDLCString contentType;
        inputChannel->GetContentType(getter_Copies(contentType));
    
        // Strap on the file extension using the mime lookup service
        if (mMIMEService)
        {
            nsCOMPtr<nsIMIMEInfo> mimeInfo;
            mMIMEService->GetFromMIMEType(contentType, getter_AddRefs(mimeInfo));
            if (mimeInfo)
            {
                // Append the mime file extension
                nsXPIDLCString fileExtension;
                if (NS_SUCCEEDED(mimeInfo->FirstExtension(getter_Copies(fileExtension))))
                {
                    nsString newExt;
                    newExt.AssignWithConversion(".");
                    newExt.AppendWithConversion(fileExtension);
                    // TODO no need to append if an extension for the mime type is already there
                    filename.Append(newExt);
                }
            }
        }

        // Store the file name
        URIData *data = new URIData;
        if (!data)
        {
            return NS_ERROR_FAILURE;
        }
        data->mNeedsPersisting = aNeedsPersisting;
        data->mFilename = filename;
        mURIMap.Put(&key, data);
    }

    aFilename = filename;

    return NS_OK;
}

nsresult
nsWebBrowserPersist::MakeFilenameFromURI(nsIURI *aURI, nsIChannel *aChannel, nsString &aFilename)
{
    // Try to get filename from the URI.
    aFilename.Truncate(0);

    // TODO we can get a suggested file name from the http channel and
    // failing that from the uri with code like below, but how can we be
    // sure it will be a legal file name for the platform?

//    nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
//    if (url)
//    {
//        char *nameFromURL = nsnull;
//        url->GetFileBaseName(&nameFromURL);
//        if (nameFromURL)
//        {
//            // Unescape the file name (GetFileName escapes it).
//            aFilename.AssignWithConversion(nsUnescape(nameFromURL));
//            nsCRT::free(nameFromURL);
//        }
//    }

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
nsWebBrowserPersist::SetDocumentBase(nsIDOMDocument *aDocument, nsIURI *aBaseURI)
{
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(aDocument);
    if (!htmlDoc)
    {
        return NS_ERROR_FAILURE;
    }

    nsXPIDLCString uriSpec;
    aBaseURI->GetSpec(getter_Copies(uriSpec));

    // Find the head element
    nsCOMPtr<nsIDOMElement> headElement;
    nsCOMPtr<nsIDOMNodeList> headList;
    aDocument->GetElementsByTagName(NS_ConvertASCIItoUCS2("head"), getter_AddRefs(headList));
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
        aDocument->CreateElement(NS_ConvertASCIItoUCS2("head"), getter_AddRefs(headElement));
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
    headElement->GetElementsByTagName(NS_ConvertASCIItoUCS2("base"), getter_AddRefs(baseList));
    if (baseList)
    {
        nsCOMPtr<nsIDOMNode> baseNode;
        baseList->Item(0, getter_AddRefs(baseNode));
        baseElement = do_QueryInterface(baseNode);
    }
    if (!baseElement)
    {
        nsCOMPtr<nsIDOMNode> newNode;
        aDocument->CreateElement(NS_ConvertASCIItoUCS2("base"), getter_AddRefs(baseElement));
        headElement->AppendChild(baseElement, getter_AddRefs(newNode));
    }
    if (!baseElement)
    {
        return NS_ERROR_FAILURE;
    }

    nsString href; href.AssignWithConversion(uriSpec);
    baseElement->SetAttribute(NS_ConvertASCIItoUCS2("href"), href);

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


NS_IMETHODIMP nsEncoderNodeFixup::FixupNode(nsIDOMNode *aNode, nsIDOMNode **aOutNode)
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
