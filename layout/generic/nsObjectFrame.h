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
#ifndef nsObjectFrame_h___
#define nsObjectFrame_h___

#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
#include "nsIObjectFrame.h"

class nsPluginInstanceOwner;

#define nsObjectFrameSuper nsHTMLContainerFrame

class nsObjectFrame : public nsObjectFrameSuper, public nsIObjectFrame {
public:
  // nsISupports 
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

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
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

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
  NS_IMETHOD GetPluginInstance(nsIPluginInstance*& aPluginInstance);

  //local methods
  nsresult CreateWidget(nsIPresContext* aPresContext,
                        nscoord aWidth,
                        nscoord aHeight,
                        PRBool aViewOnly);
  nsresult GetFullURL(nsIURI*& aFullURL);
  
  void IsSupportedImage(nsIContent* aContent, PRBool* aImage);
  void IsSupportedDocument(nsIContent* aContent, PRBool* aDoc);

  // for a given aRoot, this walks the frame tree looking for the next outFrame
  nsresult GetNextObjectFrame(nsIPresContext* aPresContext,
                              nsIFrame* aRoot,
                              nsIObjectFrame** outFrame);

  nsIPresContext *mPresContext;  // weak ref
protected:
  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

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

  nsresult ReinstantiatePlugin(nsIPresContext* aPresContext, 
                               nsHTMLReflowMetrics& aMetrics, 
                               const nsHTMLReflowState& aReflowState);

  nsresult HandleChild(nsIPresContext*          aPresContext,
                       nsHTMLReflowMetrics&     aMetrics,
                       const nsHTMLReflowState& aReflowState,
                       nsReflowStatus&          aStatus,
                       nsIFrame*                child);
 
  nsresult GetBaseURL(nsIURI* &aURL);

  PRBool IsHidden() const;

  nsresult NotifyContentObjectWrapper();

private:
  nsPluginInstanceOwner *mInstanceOwner;
  nsIURI                *mFullURL;
  nsIFrame              *mFirstChild;
  nsIWidget             *mWidget;
};


#endif /* nsObjectFrame_h___ */
