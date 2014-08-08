/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentSupportMap.h"
#include "nsXULElement.h"

void
nsContentSupportMap::Init()
{
    PL_DHashTableInit(&mMap, PL_DHashGetStubOps(), nullptr, sizeof(Entry));
}

void
nsContentSupportMap::Finish()
{
    if (mMap.ops)
        PL_DHashTableFinish(&mMap);
}

nsresult
nsContentSupportMap::Remove(nsIContent* aElement)
{
    if (!mMap.ops)
        return NS_ERROR_NOT_INITIALIZED;

    nsIContent* child = aElement;
    do {
        PL_DHashTableOperate(&mMap, child, PL_DHASH_REMOVE);
        child = child->GetNextNode(aElement);
    } while(child);

    return NS_OK;
}


