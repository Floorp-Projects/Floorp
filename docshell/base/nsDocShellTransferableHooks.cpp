/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellTransferableHooks.h"
#include "nsIClipboardDragDropHooks.h"
#include "nsIClipboardDragDropHookList.h"
#include "nsArrayEnumerator.h"

nsTransferableHookData::nsTransferableHookData()
{
}


nsTransferableHookData::~nsTransferableHookData()
{
}

//*****************************************************************************
// nsIClipboardDragDropHookList
//*****************************************************************************

NS_IMPL_ISUPPORTS(nsTransferableHookData, nsIClipboardDragDropHookList)

NS_IMETHODIMP
nsTransferableHookData::AddClipboardDragDropHooks(
                                        nsIClipboardDragDropHooks *aOverrides)
{
    NS_ENSURE_ARG(aOverrides);

    // don't let a hook be added more than once
    if (mHookList.IndexOfObject(aOverrides) == -1)
    {
        if (!mHookList.AppendObject(aOverrides))
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsTransferableHookData::RemoveClipboardDragDropHooks(
                                         nsIClipboardDragDropHooks *aOverrides)
{
    NS_ENSURE_ARG(aOverrides);
    if (!mHookList.RemoveObject(aOverrides))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsTransferableHookData::GetHookEnumerator(nsISimpleEnumerator **aResult)
{
    return NS_NewArrayEnumerator(aResult, mHookList);
}
