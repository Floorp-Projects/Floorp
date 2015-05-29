/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentSupportMap.h"
#include "nsXULElement.h"

nsresult
nsContentSupportMap::Remove(nsIContent* aElement)
{
    if (!mMap.IsInitialized())
        return NS_ERROR_NOT_INITIALIZED;

    nsIContent* child = aElement;
    do {
        PL_DHashTableRemove(&mMap, child);
        child = child->GetNextNode(aElement);
    } while(child);

    return NS_OK;
}


