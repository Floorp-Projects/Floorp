/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  virtual already_AddRefed<Accessible> CreateAccessible();
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
    
    MouseListener(nsFileControlFrame* aFrame)
     : mFrame(aFrame) 
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
    CaptureMouseListener(nsFileControlFrame* aFrame) 
      : MouseListener(aFrame)
      , mMode(0) 
    {}

    NS_DECL_NSIDOMEVENTLISTENER
    PRUint32 mMode;
  };
  
  class BrowseMouseListener: public MouseListener {
  public:
    BrowseMouseListener(nsFileControlFrame* aFrame) 
      : MouseListener(aFrame) 
    {}

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


