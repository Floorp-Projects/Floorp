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

#ifndef nsFileControlFrame_h___
#define nsFileControlFrame_h___

#include "nsBlockFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMEventListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsICapturePicker.h"
#include "nsCOMPtr.h"

class nsTextControlFrame;
class nsIDOMDragEvent;

class nsFileControlFrame : public nsBlockFrame,
                           public nsIFormControlFrame,
                           public nsIAnonymousContentCreator
{
public:
  nsFileControlFrame(nsStyleContext* aContext);

  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame*   aParent,
                  nsIFrame*   aPrevInFlow);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const;
  virtual void SetFocus(bool aOn, bool aRepaint);

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);
  virtual void ContentStatesChanged(nsEventStates aStates);
  virtual bool IsLeaf() const;



  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilter);

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif  

  typedef bool (*AcceptAttrCallback)(const nsAString&, void*);

protected:
  
  struct CaptureCallbackData {
    nsICapturePicker* picker;
    PRUint32 mode;
  };
  
  PRUint32 GetCaptureMode(const CaptureCallbackData& aData);
  
  class MouseListener;
  friend class MouseListener;
  class MouseListener : public nsIDOMEventListener {
  public:
    NS_DECL_ISUPPORTS
    
    MouseListener(nsFileControlFrame* aFrame) :
      mFrame(aFrame)
    {}

    void ForgetFrame() {
      mFrame = nsnull;
    }

  protected:
    nsFileControlFrame* mFrame;
  };

  class SyncDisabledStateEvent;
  friend class SyncDisabledStateEvent;
  class SyncDisabledStateEvent : public nsRunnable
  {
  public:
    SyncDisabledStateEvent(nsFileControlFrame* aFrame)
      : mFrame(aFrame)
    {}

    NS_IMETHOD Run() {
      nsFileControlFrame* frame = static_cast<nsFileControlFrame*>(mFrame.GetFrame());
      NS_ENSURE_STATE(frame);

      frame->SyncDisabledState();
      return NS_OK;
    }

  private:
    nsWeakFrame mFrame;
  };

  class CaptureMouseListener: public MouseListener {
  public:
    CaptureMouseListener(nsFileControlFrame* aFrame) : MouseListener(aFrame),
                                                       mMode(0) {};
    NS_DECL_NSIDOMEVENTLISTENER
    PRUint32 mMode;
  };
  
  class BrowseMouseListener: public MouseListener {
  public:
    BrowseMouseListener(nsFileControlFrame* aFrame) : MouseListener(aFrame) {};
    NS_DECL_NSIDOMEVENTLISTENER

    static bool IsValidDropData(nsIDOMDragEvent* aEvent);
  };

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsBlockFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  virtual PRIntn GetSkipSides() const;

  /**
   * The text box input.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mTextContent;
  /**
   * The browse button input.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mBrowse;

  /**
   * The capture button input.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mCapture;

  /**
   * Our mouse listener.  This makes sure we don't get used after destruction.
   */
  nsRefPtr<BrowseMouseListener> mMouseListener;
  nsRefPtr<CaptureMouseListener> mCaptureMouseListener;

protected:
  /**
   * @return the text control frame, or null if not found
   */
  nsTextControlFrame* GetTextControlFrame();

  /**
   * Copy an attribute from file content to text and button content.
   * @param aNameSpaceID namespace of attr
   * @param aAttribute attribute atom
   * @param aWhichControls which controls to apply to (SYNC_TEXT or SYNC_FILE)
   */
  void SyncAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                PRInt32 aWhichControls);

  /**
   * Sync the disabled state of the content with anonymous children.
   */
  void SyncDisabledState();
};

#endif


