/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsCOMPtr.h"
#include "nsTreeOuterFrame.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"
#include "nsIDOMXULTreeElement.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"

//
// NS_NewTreeOuterFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeOuterFrame (nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeOuterFrame* it = new nsTreeOuterFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeOuterFrame


// Constructor
nsTreeOuterFrame::nsTreeOuterFrame()
:nsTableOuterFrame() { }

// Destructor
nsTreeOuterFrame::~nsTreeOuterFrame()
{
}

NS_IMETHODIMP 
nsTreeOuterFrame::HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eConsumeDoDefault;
  if (aEvent->message == NS_KEY_DOWN) {
    // Retrieve the tree frame.
    nsIFrame* curr = mFrames.FirstChild();
    while (curr) {
      nsCOMPtr<nsIContent> content;
      curr->GetContent(getter_AddRefs(content));
      if (content) {
        nsCOMPtr<nsIAtom> tag;
        content->GetTag(*getter_AddRefs(tag));
        if (tag && tag.get() == nsXULAtoms::tree) {
          // This is our actual tree frame.
          return curr->HandleEvent(aPresContext, aEvent, aEventStatus);
        }
      }

      curr->GetNextSibling(&curr);
    }
  }
  return NS_OK;
}

