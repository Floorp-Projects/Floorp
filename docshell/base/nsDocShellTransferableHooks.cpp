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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Kathleen Brade <brade@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsDocShellTransferableHooks.h"
#include "nsIClipboardDragDropHooks.h"
#include "nsIClipboardDragDropHookList.h"
#include "nsISimpleEnumerator.h"
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

NS_IMPL_ISUPPORTS1(nsTransferableHookData, nsIClipboardDragDropHookList)

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
