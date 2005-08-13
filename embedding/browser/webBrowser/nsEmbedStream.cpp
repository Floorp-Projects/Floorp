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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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

#include <nsIPipe.h>
#include <nsIInputStream.h>
#include <nsIOutputStream.h>
#include <nsIContentViewerContainer.h>
#include <nsIDocumentLoaderFactory.h>
#include <nsNetUtil.h>
#include <prmem.h>

#include "nsXPCOMCID.h"
#include "nsICategoryManager.h"

#include "nsIContentViewer.h"

#include "nsEmbedStream.h"
#include "nsReadableUtils.h"

// nsIInputStream interface

NS_IMPL_ISUPPORTS1(nsEmbedStream, nsIInputStream)

nsEmbedStream::nsEmbedStream()
{
  mOwner       = nsnull;
  mOffset      = 0;
  mDoingStream = PR_FALSE;
}

nsEmbedStream::~nsEmbedStream()
{
}

void
nsEmbedStream::InitOwner(nsIWebBrowser *aOwner)
{
  mOwner = aOwner;
}

NS_METHOD
nsEmbedStream::Init(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIInputStream> bufInStream;
  nsCOMPtr<nsIOutputStream> bufOutStream;

  rv = NS_NewPipe(getter_AddRefs(bufInStream),
                  getter_AddRefs(bufOutStream));

  if (NS_FAILED(rv)) return rv;
 
  mInputStream  = bufInStream;
  mOutputStream = bufOutStream;

  return NS_OK;
}

NS_METHOD
nsEmbedStream::OpenStream(nsIURI *aBaseURI, const nsACString& aContentType)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);
  NS_ENSURE_TRUE(IsASCII(aContentType), NS_ERROR_INVALID_ARG);

  // if we're already doing a stream, return an error
  if (mDoingStream)
    return NS_ERROR_IN_PROGRESS;

  // set our state
  mDoingStream = PR_TRUE;

  // initialize our streams
  nsresult rv = Init();
  if (NS_FAILED(rv))
    return rv;

  // get the viewer container
  nsCOMPtr<nsIContentViewerContainer> viewerContainer;
  viewerContainer = do_GetInterface(mOwner);

  // create a new load group
  rv = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), nsnull);
  if (NS_FAILED(rv))
    return rv;

  // create a new input stream channel
  rv = NS_NewInputStreamChannel(getter_AddRefs(mChannel), aBaseURI,
				NS_STATIC_CAST(nsIInputStream *, this),
				aContentType);
  if (NS_FAILED(rv))
    return rv;

  // set the channel's load group
  rv = mChannel->SetLoadGroup(mLoadGroup);
  if (NS_FAILED(rv))
    return rv;

  // find a document loader for this content type

  const nsCString& flatContentType = PromiseFlatCString(aContentType);

  nsXPIDLCString docLoaderContractID;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = catMan->GetCategoryEntry("Gecko-Content-Viewers",
                                flatContentType.get(),
                                getter_Copies(docLoaderContractID));
  // Note: When the category manager does not find an entry for the requested
  // contract ID, it will return NS_ERROR_NOT_AVAILABLE. This conveniently
  // matches what this method wants to return in that case.
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;  
  docLoaderFactory = do_GetService(docLoaderContractID.get(), &rv);
  if (NS_FAILED(rv))
    return rv;

  // ok, create an instance of the content viewer for that command and
  // mime type
  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = docLoaderFactory->CreateInstance("view", mChannel, mLoadGroup,
					flatContentType.get(), viewerContainer,
					nsnull,
					getter_AddRefs(mStreamListener),
					getter_AddRefs(contentViewer));
  if (NS_FAILED(rv))
    return rv;

  // set the container viewer container for this content view
  rv = contentViewer->SetContainer(viewerContainer);
  if (NS_FAILED(rv))
    return rv;

  // embed this sucker
  rv = viewerContainer->Embed(contentViewer, "view", nsnull);
  if (NS_FAILED(rv))
    return rv;

  // start our request
  rv = mStreamListener->OnStartRequest(mChannel, NULL);
  if (NS_FAILED(rv))
    return rv;
  
  return NS_OK;
}

NS_METHOD
nsEmbedStream::AppendToStream(const PRUint8 *aData, PRUint32 aLen)
{
  nsresult rv;

  // append the data
  rv = Append(aData, aLen);
  if (NS_FAILED(rv))
    return rv;

  // notify our listeners
  rv = mStreamListener->OnDataAvailable(mChannel,
					NULL,
					NS_STATIC_CAST(nsIInputStream *, this),
					mOffset, /* offset */
					aLen); /* len */
  // move our counter
  mOffset += aLen;
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

NS_METHOD
nsEmbedStream::CloseStream(void)
{
  nsresult rv = NS_OK;

  // NS_ENSURE_STATE returns NS_ERROR_UNEXPECTED if the condition isn't
  // satisfied; this is exactly what we want to return.
  NS_ENSURE_STATE(mDoingStream);
  mDoingStream = PR_FALSE;

  rv = mStreamListener->OnStopRequest(mChannel, NULL, NS_OK);
  if (NS_FAILED(rv))
    return rv;

  mLoadGroup = nsnull;
  mChannel = nsnull;
  mStreamListener = nsnull;
  mOffset = 0;

  return rv;
}

NS_METHOD
nsEmbedStream::Append(const PRUint8 *aData, PRUint32 aLen)
{
  PRUint32 bytesWritten = 0;
  nsresult rv = mOutputStream->Write(NS_REINTERPRET_CAST(const char*, aData),
                                     aLen, &bytesWritten);
  if (NS_FAILED(rv))
    return rv;
  
  NS_ASSERTION(bytesWritten == aLen,
	       "underlying byffer couldn't handle the write");
  return rv;
}

NS_IMETHODIMP
nsEmbedStream::Available(PRUint32 *_retval)
{
  return mInputStream->Available(_retval);
}

NS_IMETHODIMP
nsEmbedStream::Read(char * aBuf, PRUint32 aCount, PRUint32 *_retval)
{
  return mInputStream->Read(aBuf, aCount, _retval);
}

NS_IMETHODIMP nsEmbedStream::Close(void)
{
  return mInputStream->Close();
}

NS_IMETHODIMP
nsEmbedStream::ReadSegments(nsWriteSegmentFun aWriter, void * aClosure,
			    PRUint32 aCount, PRUint32 *_retval)
{
  return mInputStream->ReadSegments(aWriter, aClosure, aCount, _retval);
}

NS_IMETHODIMP
nsEmbedStream::IsNonBlocking(PRBool *aNonBlocking)
{
    return mInputStream->IsNonBlocking(aNonBlocking);
}
