/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com>
 *
 */

#include "inBitmapDecoder.h"
#include "nsIInputStream.h"
#include "imgIContainer.h"
#include "imgIContainerObserver.h"
#include "nspr.h"
#include "nsIComponentManager.h"
#include "nsRect.h"

NS_IMPL_THREADSAFE_ADDREF(inBitmapDecoder);
NS_IMPL_THREADSAFE_RELEASE(inBitmapDecoder);

NS_INTERFACE_MAP_BEGIN(inBitmapDecoder)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIOutputStream)
   NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
   NS_INTERFACE_MAP_ENTRY(imgIDecoder)
NS_INTERFACE_MAP_END_THREADSAFE

inBitmapDecoder::inBitmapDecoder()
{
  NS_INIT_ISUPPORTS();
}

inBitmapDecoder::~inBitmapDecoder()
{ }


/** imgIDecoder methods **/

NS_IMETHODIMP inBitmapDecoder::Init(imgILoad *aLoad)
{
  mObserver = do_QueryInterface(aLoad);

  mImage = do_CreateInstance("@mozilla.org/image/container;1"); 
  if (!mImage) return NS_ERROR_FAILURE;
  aLoad->SetImage(mImage);                                                   

  mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
  if (!mFrame) return NS_ERROR_FAILURE;

  return NS_OK;
}


/** nsIOutputStream methods **/

NS_IMETHODIMP inBitmapDecoder::Close()
{
  if (mObserver) 
  {
    mObserver->OnStopFrame(nsnull, nsnull, mFrame);
    mObserver->OnStopContainer(nsnull, nsnull, mImage);
    mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
  }
  
  return NS_OK;
}

NS_IMETHODIMP inBitmapDecoder::Flush()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP inBitmapDecoder::Write(const char *buf, PRUint32 count, PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP inBitmapDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  nsresult rv;

  char *buf = (char *)PR_Malloc(count);
  if (!buf) return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
 
  // read the data from the input stram...
  PRUint32 readLen;
  rv = inStr->Read(buf, count, &readLen);

  char *data = buf;

  if (NS_FAILED(rv)) return rv;

  mObserver->OnStartDecode(nsnull, nsnull);
  
  PRUint32* wbuf = (PRUint32*) data;
  PRInt32 w, h;
  w = wbuf[0];
  h = wbuf[1];

  data = (char*) (wbuf+2);

  mImage->Init(w, h, mObserver);
  if (mObserver)
    mObserver->OnStartContainer(nsnull, nsnull, mImage);

  mFrame->Init(0, 0, w, h, gfxIFormats::RGB);
  mImage->AppendFrame(mFrame);
  if (mObserver)
    mObserver->OnStartFrame(nsnull, nsnull, mFrame);
  
  PRUint32 bpr;
  nscoord width, height;
  mFrame->GetImageBytesPerRow(&bpr);
  mFrame->GetWidth(&width);
  mFrame->GetHeight(&height);

  PRUint32 realBpr = width*3;
  
  PRUint32 i = 0;
  PRInt32 rownum = 0;
  do 
  {
    PRUint8 *line = (PRUint8*)data + i*realBpr;
    mFrame->SetImageData(line, realBpr, (rownum++)*bpr);

    nsRect r(0, rownum, width, 1);
    mObserver->OnDataAvailable(nsnull, nsnull, mFrame, &r);
    ++i;
  } while(rownum < height);


  PR_FREEIF(buf);

  return NS_OK;
}

NS_IMETHODIMP inBitmapDecoder::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP inBitmapDecoder::GetNonBlocking(PRBool *aNonBlocking)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP inBitmapDecoder::SetNonBlocking(PRBool aNonBlocking)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP inBitmapDecoder::GetObserver(nsIOutputStreamObserver * *aObserver)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP inBitmapDecoder::SetObserver(nsIOutputStreamObserver * aObserver)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
