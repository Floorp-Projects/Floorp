/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is 
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsPresContext.h"
#include "nsISupportsArray.h"
#include "nsSVGGenericContainerFrame.h"
#include "nsIXTFVisualWrapperPrivate.h"
#include "nsIAnonymousContentCreator.h"

typedef nsSVGGenericContainerFrame nsXTFSVGDisplayFrameBase;

class nsXTFSVGDisplayFrame : public nsXTFSVGDisplayFrameBase,
                             public nsIAnonymousContentCreator
{
public:
  friend nsresult
  NS_NewXTFSVGDisplayFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

protected:
  // nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsPresContext* aPresContext,
                                    nsISupportsArray& aAnonymousItems);
  
  // If the creator doesn't want to create special frame for frame hierarchy
  // then it should null out the style content arg and return NS_ERROR_FAILURE
  NS_IMETHOD CreateFrameFor(nsPresContext*   aPresContext,
                            nsIContent *      aContent,
                            nsIFrame**        aFrame) {
    if (aFrame) *aFrame = nsnull; return NS_ERROR_FAILURE; }  
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewXTFSVGDisplayFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  nsXTFSVGDisplayFrame* it = new (aPresShell) nsXTFSVGDisplayFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsXTFSVGDisplayFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsXTFSVGDisplayFrameBase)


//----------------------------------------------------------------------
// nsIAnonymousContentCreator methods:

NS_IMETHODIMP
nsXTFSVGDisplayFrame::CreateAnonymousContent(nsPresContext* aPresContext,
                                             nsISupportsArray& aAnonymousItems)
{
  nsCOMPtr<nsIXTFVisualWrapperPrivate> visual = do_QueryInterface(mContent);
  NS_ASSERTION(visual, "huh? associated content not implementing nsIXTFVisualWrapperPrivate");
  return visual->CreateAnonymousContent(aPresContext, aAnonymousItems);
}
