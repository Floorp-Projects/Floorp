/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Christopher Blizzard.
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
                   PR_TRUE, PR_FALSE, 0, PR_UINT32_MAX);
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
