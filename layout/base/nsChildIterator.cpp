/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * used by nsCSSFrameConstructor to determine and iterate the child list
 * used to construct frames (normal children or something from XBL)
 */

#include "nsChildIterator.h"
#include "nsIDocument.h"
#include "nsBindingManager.h"

nsresult
ChildIterator::Init(nsIContent*    aContent,
                    ChildIterator* aFirst,
                    ChildIterator* aLast)
{
  // Initialize out parameters to be equal, in case of failure.
  aFirst->mContent = aLast->mContent = nsnull;
  aFirst->mChild   = aLast->mChild   = nsnull;
  
  NS_PRECONDITION(aContent != nsnull, "no content");
  if (! aContent)
    return NS_ERROR_NULL_POINTER;

  nsIDocument* doc = aContent->OwnerDoc();

  // If this node has XBL children, then use them. Otherwise, just use
  // the vanilla content APIs.
  nsINodeList* nodes = doc->BindingManager()->GetXBLChildNodesFor(aContent);

  aFirst->mContent = aContent;
  aLast->mContent  = aContent;
  aFirst->mNodes   = nodes;
  aLast->mNodes    = nodes;

  if (nodes) {
    PRUint32 length;
    nodes->GetLength(&length);
    aFirst->mIndex = 0;
    aLast->mIndex = length;
  } else {
    aFirst->mChild = aContent->GetFirstChild();
    aLast->mChild = nsnull;
  }

  return NS_OK;
}
