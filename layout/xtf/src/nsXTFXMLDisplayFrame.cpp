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
#include "nsStyleSet.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsBlockFrame.h"
#include "nsInlineFrame.h"
#include "nsIXTFXMLVisual.h"
#include "nsIDOMElement.h"
#include "nsIXTFVisualWrapperPrivate.h"
#include "nsIAnonymousContentCreator.h"

////////////////////////////////////////////////////////////////////////
// nsXTFXMLBlockDisplayFrame

nsresult NS_NewXTFXMLDisplayFrame(nsIPresShell* aPresShell,
                                  PRBool isBlock, nsIFrame** aNewFrame);
  

typedef nsBlockFrame nsXTFXMLBlockDisplayFrameBase;

class nsXTFXMLBlockDisplayFrame : public nsXTFXMLBlockDisplayFrameBase,
                                  public nsIAnonymousContentCreator
{
public:
  nsXTFXMLBlockDisplayFrame();

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
  
public:
  // nsIFrame
  virtual nsIFrame* GetContentInsertionFrame();

  NS_IMETHOD  DidReflow(nsPresContext*           aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus         aStatus);

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

nsXTFXMLBlockDisplayFrame::nsXTFXMLBlockDisplayFrame()
    : nsXTFXMLBlockDisplayFrameBase()
{}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsXTFXMLBlockDisplayFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsXTFXMLBlockDisplayFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

nsIFrame*
nsXTFXMLBlockDisplayFrame::GetContentInsertionFrame()
{
  nsCOMPtr<nsIXTFVisualWrapperPrivate> visual = do_QueryInterface(mContent);
  NS_ASSERTION(visual, "huh? associated content not implementing nsIXTFVisualWrapperPrivate");
  
  nsCOMPtr<nsIDOMElement> childInsertionPoint;
  visual->GetInsertionPoint(getter_AddRefs(childInsertionPoint));
  if (!childInsertionPoint) return nsnull; // we don't take visual child content
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(childInsertionPoint);
  NS_ASSERTION(content, "element not implementing nsIContent!?");

  nsIFrame* insertionFrame = nsnull;
  GetPresContext()->PresShell()->GetPrimaryFrameFor(content, &insertionFrame);
  return insertionFrame;
}



NS_IMETHODIMP
nsXTFXMLBlockDisplayFrame::DidReflow(nsPresContext*           aPresContext,
                                const nsHTMLReflowState*  aReflowState,
                                nsDidReflowStatus         aStatus)
{
  nsresult rv = nsXTFXMLBlockDisplayFrameBase::DidReflow(aPresContext, aReflowState, aStatus);
  nsCOMPtr<nsIXTFVisualWrapperPrivate> visual = do_QueryInterface(mContent);
  NS_ASSERTION(visual, "huh? associated content not implementing nsIXTFVisualWrapperPrivate");
  visual->DidLayout();
  
  return rv;
}

//----------------------------------------------------------------------
// nsIAnonymousContentCreator methods:

NS_IMETHODIMP
nsXTFXMLBlockDisplayFrame::CreateAnonymousContent(nsPresContext* aPresContext,
                                             nsISupportsArray& aAnonymousItems)
{
  nsCOMPtr<nsIXTFVisualWrapperPrivate> visual = do_QueryInterface(mContent);
  NS_ASSERTION(visual, "huh? associated content not implementing nsIXTFVisualWrapperPrivate");
  return visual->CreateAnonymousContent(aPresContext, aAnonymousItems);
}

////////////////////////////////////////////////////////////////////////
// nsXTFXMLInlineDisplayFrame

nsresult NS_NewXTFXMLDisplayFrame(nsIPresShell* aPresShell,
                                  PRBool isInline, nsIFrame** aNewFrame);
  

typedef nsInlineFrame nsXTFXMLInlineDisplayFrameBase;

class nsXTFXMLInlineDisplayFrame : public nsXTFXMLInlineDisplayFrameBase,
                                   public nsIAnonymousContentCreator
{
public:
  nsXTFXMLInlineDisplayFrame();

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
  
public:
  // nsIFrame
  virtual nsIFrame* GetContentInsertionFrame();

  NS_IMETHOD  DidReflow(nsPresContext*           aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus         aStatus);

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

nsXTFXMLInlineDisplayFrame::nsXTFXMLInlineDisplayFrame()
    : nsXTFXMLInlineDisplayFrameBase()
{}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsXTFXMLInlineDisplayFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsXTFXMLInlineDisplayFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

nsIFrame*
nsXTFXMLInlineDisplayFrame::GetContentInsertionFrame()
{
  nsCOMPtr<nsIXTFVisualWrapperPrivate> visual = do_QueryInterface(mContent);
  NS_ASSERTION(visual, "huh? associated content not implementing nsIXTFVisualWrapperPrivate");
  
  nsCOMPtr<nsIDOMElement> childInsertionPoint;
  visual->GetInsertionPoint(getter_AddRefs(childInsertionPoint));
  if (!childInsertionPoint) return nsnull; // we don't take visual child content
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(childInsertionPoint);
  NS_ASSERTION(content, "element not implementing nsIContent!?");

  nsIFrame* insertionFrame = nsnull;
  GetPresContext()->PresShell()->GetPrimaryFrameFor(content, &insertionFrame);
  return insertionFrame;
}



NS_IMETHODIMP
nsXTFXMLInlineDisplayFrame::DidReflow(nsPresContext*           aPresContext,
                                      const nsHTMLReflowState*  aReflowState,
                                      nsDidReflowStatus         aStatus)
{
  nsresult rv = nsXTFXMLInlineDisplayFrameBase::DidReflow(aPresContext, aReflowState, aStatus);
  nsCOMPtr<nsIXTFVisualWrapperPrivate> visual = do_QueryInterface(mContent);
  NS_ASSERTION(visual, "huh? associated content not implementing nsIXTFVisualWrapperPrivate");
  visual->DidLayout();
  
  return rv;
}

//----------------------------------------------------------------------
// nsIAnonymousContentCreator methods:

NS_IMETHODIMP
nsXTFXMLInlineDisplayFrame::CreateAnonymousContent(nsPresContext* aPresContext,
                                                   nsISupportsArray& aAnonymousItems)
{
  nsCOMPtr<nsIXTFVisualWrapperPrivate> visual = do_QueryInterface(mContent);
  NS_ASSERTION(visual, "huh? associated content not implementing nsIXTFVisualWrapperPrivate");
  return visual->CreateAnonymousContent(aPresContext, aAnonymousItems);
}


////////////////////////////////////////////////////////////////////////
// Construction API

nsresult
NS_NewXTFXMLDisplayFrame(nsIPresShell* aPresShell, PRBool isBlock, nsIFrame** aNewFrame)
{
  if (isBlock)
    *aNewFrame = new (aPresShell) nsXTFXMLBlockDisplayFrame();
  else
    *aNewFrame = new (aPresShell) nsXTFXMLInlineDisplayFrame();
  
  if (nsnull == *aNewFrame)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}
