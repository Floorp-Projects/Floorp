/*
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "GtkMozEmbedStream.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

// nsIInputStream interface

NS_IMPL_ISUPPORTS1(GtkMozEmbedStream, nsIInputStream)

GtkMozEmbedStream::GtkMozEmbedStream()
{
  NS_INIT_REFCNT();
}

GtkMozEmbedStream::~GtkMozEmbedStream()
{
}

NS_METHOD GtkMozEmbedStream::Init()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIInputStream> bufInStream;
  nsCOMPtr<nsIOutputStream> bufOutStream;
  
  rv = NS_NewPipe(getter_AddRefs(bufInStream),
		  getter_AddRefs(bufOutStream));

  if (NS_FAILED(rv)) return rv;
  
  mInputStream  = do_QueryInterface(bufInStream);
  mOutputStream = do_QueryInterface(bufOutStream);
  return rv;
}

NS_METHOD GtkMozEmbedStream::Append(const char *aData, PRUint32 aLen)
{
  nsresult rv = NS_OK;
  PRUint32 bytesWritten = 0;
  rv = mOutputStream->Write(aData, aLen, &bytesWritten);
  if (NS_FAILED(rv))
    return rv;
  
  NS_ASSERTION(bytesWritten == aLen, "underlying byffer couldn't handle the write");
  return rv;
}

NS_IMETHODIMP GtkMozEmbedStream::Available(PRUint32 *_retval)
{
  return mInputStream->Available(_retval);
}

NS_IMETHODIMP GtkMozEmbedStream::Read(char * aBuf, PRUint32 aCount, PRUint32 *_retval)
{
  return mInputStream->Read(aBuf, aCount, _retval);
}

// nsIBaseStream interface

NS_IMETHODIMP GtkMozEmbedStream::Close(void)
{
  return mInputStream->Close();
  return NS_OK;
}

