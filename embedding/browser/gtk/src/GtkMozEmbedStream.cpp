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
#include "nsIAllocator.h"

// nsIInputStream interface

NS_IMPL_ADDREF(GtkMozEmbedStream)
NS_IMPL_RELEASE(GtkMozEmbedStream)

NS_INTERFACE_MAP_BEGIN(GtkMozEmbedStream)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
NS_INTERFACE_MAP_END

GtkMozEmbedStream::GtkMozEmbedStream()
{
  NS_INIT_REFCNT();
  mLength = 0;
  mBuffer = 0;
}

GtkMozEmbedStream::~GtkMozEmbedStream()
{
  if (mBuffer)
    nsAllocator::Free(mBuffer);
}

NS_IMETHODIMP GtkMozEmbedStream::Available(PRUint32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mLength;
  return NS_OK;
}

NS_IMETHODIMP GtkMozEmbedStream::Read(char * aBuf, PRUint32 aCount, PRUint32 *_retval)
{
  PRUint32 bytesToRead;

  NS_ENSURE_ARG_POINTER(aBuf);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = 0;

  // shortcut?
  if (aCount == 0)
    return NS_OK;

  // check to see how many bytes we can read
  if (aCount > mLength)
    bytesToRead = mLength;
  else
    bytesToRead = aCount;

  // copy our data
  *_retval = bytesToRead;
  memcpy(aBuf, mBuffer, bytesToRead);

  // see if we can truncate the buffer entirely
  if (bytesToRead == mBufLen)
  {
    mBuffer = nsnull;
    mLength = 0;
    mBufLen = 0;
    return NS_OK;
  }
  // truncate it some, then
  memmove(mBuffer, &mBuffer[bytesToRead], (mLength - bytesToRead));
  mLength -= bytesToRead;
  
  return NS_OK;
}

// nsIBaseStream interface

NS_IMETHODIMP GtkMozEmbedStream::Close(void)
{
  if (mBuffer)
    nsAllocator::Free(mBuffer);
  mLength = 0;
  mBufLen = 0;
  return NS_OK;
}

NS_METHOD GtkMozEmbedStream::Append(const char *aData, PRUint32 aLen)
{
  // shortcut?
  if (aLen == 0 || aData == NULL)
    return NS_OK;
  mLength += aLen;
  // first time
  if (!mBuffer)
  {
    mBuffer = (char *)nsAllocator::Alloc(mLength);
    mBufLen = mLength;
    if (!mBuffer)
    {
      mLength = 0;
      mBufLen = 0;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    memcpy(mBuffer, aData, aLen);
    return NS_OK;
  }
  else
  {
    // see if we need to realloc
    if (mLength > mBufLen)
    {
      char *newBuffer;
      newBuffer = (char *)nsAllocator::Realloc(mBuffer, mLength);
      if (!newBuffer)
      {
	mLength = 0;
	mBufLen = 0;
	nsAllocator::Free(mBuffer);
	mBuffer = NULL;
	return NS_ERROR_OUT_OF_MEMORY;
      }
      mBuffer = newBuffer;
      mBufLen = mLength;
      memcpy(&mBuffer[mLength - aLen], aData, aLen);
    }
    else
      // no realloc required and don't have to update mBufLen, just
      // update
      memcpy(&mBuffer[mLength - aLen], aData, aLen);
  }

  return NS_OK;
}
