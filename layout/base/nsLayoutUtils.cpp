/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsFrameList.h"

/**
 * A namespace class for static layout utilities.
 */

/**
 * GetFirstChildFrame returns the first "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :before generated frame).   
 * @param aPresContext the prescontext
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetFirstChildFrame(nsIPresContext* aPresContext,
                   nsIFrame*       aFrame,
                   nsIContent*     aContent)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  nsIFrame* childFrame;

  // Get the first child frame
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);

  // If the child frame is a pseudo-frame, then return its first child.
  // Note that the frame we create for the generated content is also a
  // pseudo-frame and so don't drill down in that case
  if (childFrame &&
      childFrame->IsPseudoFrame(aContent) &&
      !childFrame->IsGeneratedContentFrame()) {
    return GetFirstChildFrame(aPresContext, childFrame, aContent);
  }

  return childFrame;
}

/**
 * GetLastChildFrame returns the last "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :after generated frame).   
 * @param aPresContext the prescontext
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetLastChildFrame(nsIPresContext* aPresContext,
                  nsIFrame*       aFrame,
                  nsIContent*     aContent)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  // Get the last in flow frame
  nsIFrame* lastInFlow;
  do {
    lastInFlow = aFrame;
    lastInFlow->GetNextInFlow(&aFrame);
  } while (aFrame);

  // Get the last child frame
  nsIFrame* firstChildFrame;
  lastInFlow->FirstChild(aPresContext, nsnull, &firstChildFrame);
  if (firstChildFrame) {
    nsFrameList frameList(firstChildFrame);
    nsIFrame*   lastChildFrame = frameList.LastChild();

    NS_ASSERTION(lastChildFrame, "unexpected error");

    // Get the frame's first-in-flow. This matters in case the frame has
    // been continuted across multiple lines
    while (PR_TRUE) {
      nsIFrame* prevInFlow;
      lastChildFrame->GetPrevInFlow(&prevInFlow);
      if (prevInFlow) {
        lastChildFrame = prevInFlow;
      } else {
        break;
      }
    }

    // If the last child frame is a pseudo-frame, then return its last child.
    // Note that the frame we create for the generated content is also a
    // pseudo-frame and so don't drill down in that case
    if (lastChildFrame &&
        lastChildFrame->IsPseudoFrame(aContent) &&
        !lastChildFrame->IsGeneratedContentFrame()) {
      return GetLastChildFrame(aPresContext, lastChildFrame, aContent);
    }

    return lastChildFrame;
  }

  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetBeforeFrame(nsIFrame* aFrame, nsIPresContext* aPresContext)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");
#ifdef DEBUG
  nsIFrame* prevInFlow = nsnull;
  aFrame->GetPrevInFlow(&prevInFlow);
  NS_ASSERTION(!prevInFlow, "aFrame must be first-in-flow");
#endif
  
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  nsIFrame* firstFrame = GetFirstChildFrame(aPresContext, aFrame, content);

  if (firstFrame && firstFrame->IsGeneratedContentFrame()) {
    return firstFrame;
  }

  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetAfterFrame(nsIFrame* aFrame, nsIPresContext* aPresContext)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  nsIFrame* lastFrame = GetLastChildFrame(aPresContext, aFrame, content);

  if (lastFrame && lastFrame->IsGeneratedContentFrame()) {
    return lastFrame;
  }

  return nsnull;
}
