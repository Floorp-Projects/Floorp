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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImageContainer.h"

#include "nsUnitConverters.h"

NS_IMPL_ISUPPORTS1(nsImageContainer, nsIImageContainer)

nsImageContainer::nsImageContainer()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mCurrentFrame = 0;
}

nsImageContainer::~nsImageContainer()
{
  /* destructor code */
  mFrames.Clear();
}




/* void init (in gfx_dimension aWidth, in gfx_dimension aHeight); */
NS_IMETHODIMP nsImageContainer::Init(gfx_dimension aWidth, gfx_dimension aHeight)
{
  if (aWidth <= 0 || aHeight <= 0) {
    printf("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mSize.SizeTo(aWidth, aHeight);

  return NS_OK;
}

/* readonly attribute gfx_dimension width; */
NS_IMETHODIMP nsImageContainer::GetWidth(gfx_dimension *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute gfx_dimension height; */
NS_IMETHODIMP nsImageContainer::GetHeight(gfx_dimension *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}


/* readonly attribute nsIImageFrame currentFrame; */
NS_IMETHODIMP nsImageContainer::GetCurrentFrame(nsIImageFrame * *aCurrentFrame)
{
  return this->GetFrameAt(mCurrentFrame, aCurrentFrame);
}

/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP nsImageContainer::GetNumFrames(PRUint32 *aNumFrames)
{
  return mFrames.Count(aNumFrames);
}

/* nsIImageFrame getFrameAt (in unsigned long index); */
NS_IMETHODIMP nsImageContainer::GetFrameAt(PRUint32 index, nsIImageFrame **_retval)
{
  nsISupports *sup = mFrames.ElementAt(index);
  if (!sup)
    return NS_ERROR_FAILURE;

  *_retval = NS_REINTERPRET_CAST(nsIImageFrame *, sup);
  return NS_OK;
}

/* void appendFrame (in nsIImageFrame item); */
NS_IMETHODIMP nsImageContainer::AppendFrame(nsIImageFrame *item)
{
    return mFrames.AppendElement(NS_REINTERPRET_CAST(nsISupports*, item));
}

/* void removeFrame (in nsIImageFrame item); */
NS_IMETHODIMP nsImageContainer::RemoveFrame(nsIImageFrame *item)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator enumerate (); */
NS_IMETHODIMP nsImageContainer::Enumerate(nsIEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void clear (); */
NS_IMETHODIMP nsImageContainer::Clear()
{
  return mFrames.Clear();
}
