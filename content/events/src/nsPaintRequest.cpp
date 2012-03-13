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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "nsPaintRequest.h"

#include "nsDOMClassInfoID.h"
#include "nsClientRect.h"
#include "nsIFrame.h"
#include "nsContentUtils.h"

DOMCI_DATA(PaintRequest, nsPaintRequest)

NS_INTERFACE_TABLE_HEAD(nsPaintRequest)
  NS_INTERFACE_TABLE1(nsPaintRequest, nsIDOMPaintRequest)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PaintRequest)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPaintRequest)
NS_IMPL_RELEASE(nsPaintRequest)

NS_IMETHODIMP
nsPaintRequest::GetClientRect(nsIDOMClientRect** aResult)
{
  nsRefPtr<nsClientRect> clientRect = new nsClientRect();
  if (!clientRect)
    return NS_ERROR_OUT_OF_MEMORY;
  clientRect->SetLayoutRect(mRequest.mRect);
  clientRect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPaintRequest::GetReason(nsAString& aResult)
{
  switch (mRequest.mFlags & nsIFrame::INVALIDATE_REASON_MASK) {
  case nsIFrame::INVALIDATE_REASON_SCROLL_BLIT:
    aResult.AssignLiteral("scroll copy");
    break;
  case nsIFrame::INVALIDATE_REASON_SCROLL_REPAINT:
    aResult.AssignLiteral("scroll repaint");
    break;
  default:
    aResult.Truncate();
    break;
  }
  return NS_OK;
}

DOMCI_DATA(PaintRequestList, nsPaintRequestList)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsPaintRequestList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsPaintRequestList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsPaintRequestList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsPaintRequestList)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_TABLE_HEAD(nsPaintRequestList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(nsPaintRequestList, nsIDOMPaintRequestList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsPaintRequestList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PaintRequestList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPaintRequestList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPaintRequestList)

NS_IMETHODIMP    
nsPaintRequestList::GetLength(PRUint32* aLength)
{
  *aLength = mArray.Count();
  return NS_OK;
}

NS_IMETHODIMP    
nsPaintRequestList::Item(PRUint32 aIndex, nsIDOMPaintRequest** aReturn)
{
  NS_IF_ADDREF(*aReturn = nsPaintRequestList::GetItemAt(aIndex));
  return NS_OK;
}

nsIDOMPaintRequest*
nsPaintRequestList::GetItemAt(PRUint32 aIndex)
{
  return mArray.SafeObjectAt(aIndex);
}
