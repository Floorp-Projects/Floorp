/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsObjectFrame_h___
#define nsObjectFrame_h___

#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"

class nsPluginInstanceOwner;

#define nsObjectFrameSuper nsHTMLContainerFrame

class nsObjectFrame : public nsObjectFrameSuper {
public:
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD DidReflow(nsIPresContext* aPresContext,
                       nsDidReflowStatus aStatus);
  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD  HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent*     aEvent,
                          nsEventStatus*  aEventStatus);

  NS_IMETHOD Scrolled(nsIView *aView);
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);
  //local methods
  nsresult CreateWidget(nsIPresContext* aPresContext,
                        nscoord aWidth,
                        nscoord aHeight,
                        PRBool aViewOnly);
  nsresult GetFullURL(nsIURI*& aFullURL);

  nsresult GetPluginInstance(nsIPluginInstance*& aPluginInstance);
  
  void IsSupportedImage(nsIContent* aContent, PRBool* aImage);

protected:
  virtual ~nsObjectFrame();

  virtual PRIntn GetSkipSides() const;

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);


  nsresult SetFullURL(nsIURI* aURL);

  nsresult InstantiateWidget(nsIPresContext*          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsCID aWidgetCID);

  nsresult InstantiatePlugin(nsIPresContext*          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsIPluginHost* aPluginHost, 
							const char* aMimetype,
							nsIURI* aURL);
  //~~~
  nsresult ReinstantiatePlugin(nsIPresContext* aPresContext, 
                               nsHTMLReflowMetrics& aMetrics, 
                               const nsHTMLReflowState& aReflowState);

  nsresult HandleImage(nsIPresContext*          aPresContext,
					   nsHTMLReflowMetrics&     aMetrics,
					   const nsHTMLReflowState& aReflowState,
					   nsReflowStatus&          aStatus,
					   nsIFrame* child);
 
  nsresult GetBaseURL(nsIURI* &aURL);

private:
  nsPluginInstanceOwner *mInstanceOwner;
  nsIURI                *mFullURL;
  nsIFrame              *mFirstChild;
  nsIWidget				      *mWidget;
};


#endif /* nsObjectFrame_h___ */
