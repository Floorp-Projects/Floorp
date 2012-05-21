/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAsyncInputStream.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPipe.h"

#include "nsEmbedStream.h"
#include "nsNetError.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS0(nsEmbedStream)

nsEmbedStream::nsEmbedStream()
{
  mOwner = nsnull;
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
  return NS_OK;
}

NS_METHOD
nsEmbedStream::OpenStream(nsIURI *aBaseURI, const nsACString& aContentType)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aBaseURI);
  NS_ENSURE_TRUE(IsASCII(aContentType), NS_ERROR_INVALID_ARG);

  // if we're already doing a stream, return an error
  if (mOutputStream)
    return NS_ERROR_IN_PROGRESS;

  nsCOMPtr<nsIAsyncInputStream> inputStream;
  nsCOMPtr<nsIAsyncOutputStream> outputStream;
  rv = NS_NewPipe2(getter_AddRefs(inputStream),
                   getter_AddRefs(outputStream),
                   true, false, 0, PR_UINT32_MAX);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mOwner);
  rv = docShell->LoadStream(inputStream, aBaseURI, aContentType,
                            EmptyCString(), nsnull);
  if (NS_FAILED(rv))
    return rv;

  mOutputStream = outputStream;
  return rv;
}

NS_METHOD
nsEmbedStream::AppendToStream(const PRUint8 *aData, PRUint32 aLen)
{
  nsresult rv;
  NS_ENSURE_STATE(mOutputStream);

  PRUint32 bytesWritten = 0;
  rv = mOutputStream->Write(reinterpret_cast<const char*>(aData),
                            aLen, &bytesWritten);
  if (NS_FAILED(rv))
    return rv;

  NS_ASSERTION(bytesWritten == aLen,
               "underlying buffer couldn't handle the write");
  return rv;
}

NS_METHOD
nsEmbedStream::CloseStream(void)
{
  nsresult rv = NS_OK;

  // NS_ENSURE_STATE returns NS_ERROR_UNEXPECTED if the condition isn't
  // satisfied; this is exactly what we want to return.
  NS_ENSURE_STATE(mOutputStream);
  mOutputStream->Close();
  mOutputStream = 0;

  return rv;
}
