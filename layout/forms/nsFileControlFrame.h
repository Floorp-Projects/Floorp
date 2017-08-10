/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFileControlFrame_h___
#define nsFileControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsBlockFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMEventListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

class nsIDOMDataTransfer;
class nsIDOMFileList;
namespace mozilla {
namespace dom {
class BlobImpl;
} // namespace dom
} // namespace mozilla

class nsFileControlFrame : public nsBlockFrame,
                           public nsIFormControlFrame,
                           public nsIAnonymousContentCreator
{
public:
  explicit nsFileControlFrame(nsStyleContext* aContext);

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsFileControlFrame)

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) override;
  virtual void SetFocus(bool aOn, bool aRepaint) override;

  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;
  virtual void ContentStatesChanged(mozilla::EventStates aStates) override;

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  typedef bool (*AcceptAttrCallback)(const nsAString&, void*);

protected:

  class MouseListener;
  friend class MouseListener;
  class MouseListener : public nsIDOMEventListener {
  public:
    NS_DECL_ISUPPORTS

    explicit MouseListener(nsFileControlFrame* aFrame)
     : mFrame(aFrame)
    {}

    void ForgetFrame() {
      mFrame = nullptr;
    }

  protected:
    virtual ~MouseListener() {}

    nsFileControlFrame* mFrame;
  };

  class SyncDisabledStateEvent;
  friend class SyncDisabledStateEvent;
  class SyncDisabledStateEvent : public mozilla::Runnable
  {
  public:
    explicit SyncDisabledStateEvent(nsFileControlFrame* aFrame)
      : mozilla::Runnable("nsFileControlFrame::SyncDisabledStateEvent")
      , mFrame(aFrame)
    {}

    NS_IMETHOD Run() override {
      nsFileControlFrame* frame = static_cast<nsFileControlFrame*>(mFrame.GetFrame());
      NS_ENSURE_STATE(frame);

      frame->SyncDisabledState();
      return NS_OK;
    }

  private:
    WeakFrame mFrame;
  };

  class DnDListener: public MouseListener {
  public:
    explicit DnDListener(nsFileControlFrame* aFrame)
      : MouseListener(aFrame)
    {}

    NS_DECL_NSIDOMEVENTLISTENER

    nsresult GetBlobImplForWebkitDirectory(nsIDOMFileList* aFileList,
                                           mozilla::dom::BlobImpl** aBlobImpl);

    bool IsValidDropData(nsIDOMDataTransfer* aDOMDataTransfer);
    bool CanDropTheseFiles(nsIDOMDataTransfer* aDOMDataTransfer, bool aSupportsMultiple);
  };

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsBlockFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  /**
   * The text box input.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mTextContent;
  /**
   * The button to open a file or directory picker.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mBrowseFilesOrDirs;

  /**
   * Drag and drop mouse listener.
   * This makes sure we don't get used after destruction.
   */
  RefPtr<DnDListener> mMouseListener;

protected:
  /**
   * Sync the disabled state of the content with anonymous children.
   */
  void SyncDisabledState();

  /**
   * Updates the displayed value by using aValue.
   */
  void UpdateDisplayedValue(const nsAString& aValue, bool aNotify);
};

#endif // nsFileControlFrame_h___
