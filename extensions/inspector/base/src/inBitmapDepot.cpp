/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#include "inBitmapDepot.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "inIBitmap.h"

///////////////////////////////////////////////////////////////////////////////

inBitmapDepot::inBitmapDepot()
{
  NS_INIT_REFCNT();
}

inBitmapDepot::~inBitmapDepot()
{
}

NS_IMPL_ISUPPORTS1(inBitmapDepot, inIBitmapDepot);

///////////////////////////////////////////////////////////////////////////////
// inIBitmapDepot

NS_IMETHODIMP
inBitmapDepot::Put(inIBitmap* aBitmap, const nsAReadableString& aName)
{
  nsStringKey key(aName);
  nsCOMPtr<inIBitmap> bitmap(aBitmap);
  nsCOMPtr<nsISupports> supports = do_QueryInterface(bitmap);
  mHash.Put(&key, supports);

  return NS_OK;
}

NS_IMETHODIMP
inBitmapDepot::Get(const nsAReadableString& aName, inIBitmap** _retval)
{
  nsStringKey key(aName);
  nsCOMPtr<nsISupports> supports = mHash.Get(&key);
  nsCOMPtr<inIBitmap> bitmap = do_QueryInterface(supports);
  *_retval = bitmap;
  NS_IF_ADDREF(*_retval);
  
  return NS_OK;
}

NS_IMETHODIMP
inBitmapDepot::Remove(const nsAReadableString& aName, inIBitmap** _retval)
{
  nsStringKey key(aName);
  nsCOMPtr<nsISupports> supports;
  mHash.Remove(&key, getter_AddRefs(supports));
  nsCOMPtr<inIBitmap> bitmap = do_QueryInterface(supports);
  *_retval = bitmap;
  NS_IF_ADDREF(*_retval);
    
  return NS_OK;
}

