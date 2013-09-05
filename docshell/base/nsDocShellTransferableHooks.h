/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellTransferableHooks_h__
#define nsDocShellTransferableHooks_h__

#include "nsIClipboardDragDropHookList.h"
#include "nsCOMArray.h"

class nsIClipboardDragDropHooks;

class nsTransferableHookData : public nsIClipboardDragDropHookList
{
public:
    nsTransferableHookData();
    virtual ~nsTransferableHookData();
    NS_DECL_ISUPPORTS
    NS_DECL_NSICLIPBOARDDRAGDROPHOOKLIST

protected:
    nsCOMArray<nsIClipboardDragDropHooks> mHookList;
};

#endif // nsDocShellTransferableHooks_h__
