/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is
 * Novell Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsTextRectangle.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"

NS_INTERFACE_TABLE_HEAD(nsTextRectangle)
  NS_INTERFACE_TABLE1(nsTextRectangle, nsIDOMTextRectangle)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(TextRectangle)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTextRectangle)
NS_IMPL_RELEASE(nsTextRectangle)

nsTextRectangle::nsTextRectangle()
  : mX(0.0), mY(0.0), mWidth(0.0), mHeight(0.0)
{
}

NS_IMETHODIMP
nsTextRectangle::GetLeft(float* aResult)
{
  *aResult = mX;
  return NS_OK;
}

NS_IMETHODIMP
nsTextRectangle::GetTop(float* aResult)
{
  *aResult = mY;
  return NS_OK;
}

NS_IMETHODIMP
nsTextRectangle::GetRight(float* aResult)
{
  *aResult = mX + mWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsTextRectangle::GetBottom(float* aResult)
{
  *aResult = mY + mHeight;
  return NS_OK;
}

NS_INTERFACE_TABLE_HEAD(nsTextRectangleList)
  NS_INTERFACE_TABLE1(nsTextRectangleList, nsIDOMTextRectangleList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(TextRectangleList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTextRectangleList)
NS_IMPL_RELEASE(nsTextRectangleList)


NS_IMETHODIMP    
nsTextRectangleList::GetLength(PRUint32* aLength)
{
  *aLength = mArray.Count();
  return NS_OK;
}

NS_IMETHODIMP    
nsTextRectangleList::Item(PRUint32 aIndex, nsIDOMTextRectangle** aReturn)
{
  if (aIndex >= PRUint32(mArray.Count())) {
    *aReturn = nsnull;
    return NS_OK;
  } 
  
  NS_IF_ADDREF(*aReturn = mArray.ObjectAt(aIndex));
  return NS_OK;
}
