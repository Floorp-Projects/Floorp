/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "inSearchItemImage.h"

///////////////////////////////////////////////////////////////////////////////

inSearchItemImage::inSearchItemImage(nsAutoString* aURL)
{
  NS_INIT_ISUPPORTS();

  mURL = *aURL;
}

inSearchItemImage::~inSearchItemImage()
{
}

NS_IMPL_ISUPPORTS1(inSearchItemImage, inISearchItem);

///////////////////////////////////////////////////////////////////////////////
// inISearchItem

NS_IMETHODIMP 
inSearchItemImage::GetDescription(PRUnichar** aDescription)
{
  *aDescription = mURL.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP 
inSearchItemImage::GetIconURL(PRUnichar** aURL)
{
  nsAutoString url;
  url.AssignWithConversion("chrome://inspector/content/search/ImageSearchItem.gif");
  *aURL = url.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP 
inSearchItemImage::IsViewable(PRBool* _retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP 
inSearchItemImage::IsEditable(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
inSearchItemImage::ViewItem(nsIDOMElement **_retval)
{
  return NS_OK;
}

NS_IMETHODIMP 
inSearchItemImage::EditItem()
{
  return NS_OK;
}
