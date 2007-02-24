/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/* rendering objects for replaced elements implemented by a plugin */

#ifndef nsObjectFrame_h___
#define nsObjectFrame_h___

#ifdef XP_WIN
#include <windows.h>
#endif

#include "nsIObjectFrame.h"
#include "nsFrame.h"

#ifdef ACCESSIBILITY
class nsIAccessible;
#endif

class nsPluginInstanceOwner;
class nsIPluginHost;
class nsIPluginInstance;
class nsPresContext;

#define nsObjectFrameSuper nsFrame

class nsObjectFrame : public nsObjectFrameSuper, public nsIObjectFrame {
public:
  friend nsIFrame* NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  // nsISupports 
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame* aParent,
                  nsIFrame* aPrevInFlow);
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD DidReflow(nsPresContext* aPresContext,
                       const nsHTMLReflowState* aReflowState,
                       nsDidReflowStatus aStatus);
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  void PrintPlugin(nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  void PaintPlugin(nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD  HandleEvent(nsPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus);

  virtual nsIAtom* GetType() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsObjectFrameSuper::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

  virtual PRBool SupportsVisibilityHidden() { return PR_FALSE; }
  virtual PRBool NeedsView() { return PR_TRUE; }
  virtual nsresult CreateWidgetForView(nsIView* aView);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  virtual void Destroy();

  NS_IMETHOD GetPluginInstance(nsIPluginInstance*& aPluginInstance);
  virtual nsresult Instantiate(nsIChannel* aChannel, nsIStreamListener** aStreamListener);
  virtual nsresult Instantiate(const char* aMimeType, nsIURI* aURI);
  virtual void StopPlugin();


  /* fail on any requests to get a cursor from us because plugins set their own! see bug 118877 */
  NS_IMETHOD GetCursor(const nsPoint& aPoint, nsIFrame::Cursor& aCursor) 
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

  // accessibility support
#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#ifdef XP_WIN
  NS_IMETHOD GetPluginPort(HWND *aPort);
#endif
#endif

  //local methods
  nsresult CreateWidget(nscoord aWidth, nscoord aHeight, PRBool aViewOnly);

  // for a given aRoot, this walks the frame tree looking for the next outFrame
  static nsIObjectFrame* GetNextObjectFrame(nsPresContext* aPresContext,
                                            nsIFrame* aRoot);

protected:
  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  nsObjectFrame(nsStyleContext* aContext) : nsObjectFrameSuper(aContext) {}
  virtual ~nsObjectFrame();

  // NOTE:  This frame class does not inherit from |nsLeafFrame|, so
  // this is not a virtual method implementation.
  void GetDesiredSize(nsPresContext* aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsHTMLReflowMetrics& aDesiredSize);

  nsresult InstantiatePlugin(nsIPluginHost* aPluginHost, 
                             const char* aMimetype,
                             nsIURI* aURL);

  /**
   * Adjust the plugin's idea of its size, using aSize as its new size.
   * (aSize must be in twips)
   */
  void FixupWindow(const nsSize& aSize);

  PRBool IsFocusable(PRInt32 *aTabIndex = nsnull, PRBool aWithMouse = PR_FALSE);

  // check attributes and optionally CSS to see if we should display anything
  PRBool IsHidden(PRBool aCheckVisibilityStyle = PR_TRUE) const;

  void NotifyContentObjectWrapper();

  nsPoint GetWindowOriginInPixels(PRBool aWindowless);

  /**
   * Makes sure that mInstanceOwner is valid and without a current plugin
   * instance. Essentially, this prepares the frame to receive a new plugin.
   */
  NS_HIDDEN_(nsresult) PrepareInstanceOwner();

  friend class nsPluginInstanceOwner;
private:
  nsPluginInstanceOwner *mInstanceOwner;
  nsRect                mWindowlessRect;

#ifdef DEBUG
  // For assertions that make it easier to determine if a crash is due
  // to the underlying problem described in bug 136927.
  PRBool mInstantiating;
#endif
};


#endif /* nsObjectFrame_h___ */
