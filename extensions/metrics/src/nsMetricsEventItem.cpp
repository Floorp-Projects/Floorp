/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsMetricsEventItem.h"
#include "nsIPropertyBag.h"

nsMetricsEventItem::nsMetricsEventItem(const nsAString &itemNamespace,
                                       const nsAString &itemName)
    : mNamespace(itemNamespace), mName(itemName)
{
}

nsMetricsEventItem::~nsMetricsEventItem()
{
}

NS_IMPL_ISUPPORTS1(nsMetricsEventItem, nsIMetricsEventItem)

NS_IMETHODIMP
nsMetricsEventItem::GetItemNamespace(nsAString &result)
{
  result = mNamespace;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::GetItemName(nsAString &result)
{
  result = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::GetProperties(nsIPropertyBag **aProperties)
{
  NS_IF_ADDREF(*aProperties = mProperties);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::SetProperties(nsIPropertyBag *aProperties)
{
  // This do_QueryInterface() shouldn't be necessary, but is needed to avoid
  // assertions from nsCOMPtr when an nsIWritablePropertyBag2 is passed in.
  mProperties = do_QueryInterface(aProperties);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::ChildAt(PRInt32 index, nsIMetricsEventItem **result)
{
  NS_ENSURE_ARG_RANGE(index, 0, PRInt32(mChildren.Length()) - 1);

  NS_ADDREF(*result = mChildren[index]);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::IndexOf(nsIMetricsEventItem *item, PRInt32 *result)
{
  *result = PRInt32(mChildren.IndexOf(item));  // NoIndex mapped to -1
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::AppendChild(nsIMetricsEventItem *item)
{
  NS_ENSURE_ARG_POINTER(item);

  NS_ENSURE_TRUE(mChildren.AppendElement(item), NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::InsertChildAt(nsIMetricsEventItem *item, PRInt32 index)
{
  NS_ENSURE_ARG_POINTER(item);

  // allow appending
  NS_ENSURE_ARG_RANGE(index, 0, PRInt32(mChildren.Length()));

  NS_ENSURE_TRUE(mChildren.InsertElementAt(index, item),
                 NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::RemoveChildAt(PRInt32 index)
{
  NS_ENSURE_ARG_RANGE(index, 0, PRInt32(mChildren.Length()) - 1);

  mChildren.RemoveElementAt(index);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::ReplaceChildAt(nsIMetricsEventItem *newItem, PRInt32 index)
{
  NS_ENSURE_ARG_POINTER(newItem);
  NS_ENSURE_ARG_RANGE(index, 0, PRInt32(mChildren.Length()) - 1);

  mChildren.ReplaceElementsAt(index, 1, newItem);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::ClearChildren()
{
  mChildren.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsEventItem::GetChildCount(PRInt32 *childCount)
{
  *childCount = PRInt32(mChildren.Length());
  return NS_OK;
}
