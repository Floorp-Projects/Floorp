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
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
#ifndef NSIFRAMETRAVERSAL_H
#define NSIFRAMETRAVERSAL_H

#include "nsISupports.h"
#include "nsIEnumerator.h"
#include "nsIFrame.h"

/* Brief explanation of frame traversal types:
 *
 * LEAF:
 *  Iterate over only the leaf frames in the tree, in depth-first order.
 *
 * EXTENSIVE:
 *  Iterate over all frames in the tree, including non-leaf frames.
 *  Child frames are traversed before their parents going both forward
 *  and backward.
 *
 * FOCUS:
 *  Traverse frames in "focus" order, which is like extensive but
 *  does a strict preorder traversal in both directions.  This type of
 *  traversal also handles placeholder frames transparently, meaning that
 *  it will never stop on one - going down will get the real frame, going
 *  back up will go on past the placeholder, so the placeholders are logically
 *  part of the frame tree.
 *
 * FASTEST:
 *  XXX not implemented
 *
 * VISUAL:
 *  Traverse frames in "visual" order (left-to-right, top-to-bottom).
 */

enum nsTraversalType{
  LEAF,
  EXTENSIVE,
  FOCUS,
  FASTEST
#ifdef IBMBIDI // Simon
   , VISUAL
#endif
}; 

// {1691E1F3-EE41-11d4-9885-00C04FA0CF4B}
#define NS_IFRAMETRAVERSAL_IID \
{ 0x1691e1f3, 0xee41, 0x11d4, { 0x98, 0x85, 0x0, 0xc0, 0x4f, 0xa0, 0xcf, 0x4b } }

class nsIFrameTraversal : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFRAMETRAVERSAL_IID)

  NS_IMETHOD NewFrameTraversal(nsIBidirectionalEnumerator **aEnumerator,
                              PRUint32 aType,
                              nsIPresContext* aPresContext,
                              nsIFrame *aStart) = 0;
};


#endif //NSIFRAMETRAVERSAL_H
