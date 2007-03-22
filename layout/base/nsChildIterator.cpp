/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
  aFirst->mIndex   = aLast->mIndex   = 0;
  
  NS_PRECONDITION(aContent != nsnull, "no content");
  if (! aContent)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDocument> doc = aContent->GetDocument();
  NS_ASSERTION(doc, "element not in the document");
  if (! doc)
    return NS_ERROR_FAILURE;

  // If this node has XBL children, then use them. Otherwise, just use
  // the vanilla content APIs.
  nsCOMPtr<nsIDOMNodeList> nodes;
  doc->BindingManager()->GetXBLChildNodesFor(aContent, getter_AddRefs(nodes));

  PRUint32 length;
  if (nodes)
    nodes->GetLength(&length);
  else
    length = aContent->GetChildCount();

  aFirst->mContent = aContent;
  aLast->mContent  = aContent;
  aFirst->mIndex   = 0;
  aLast->mIndex    = length;
  aFirst->mNodes   = nodes;
  aLast->mNodes    = nodes;

  return NS_OK;
}
