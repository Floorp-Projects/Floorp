/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

namespace mozilla::dom {
class FileList;
class BlobImpl;
class DataTransfer;
}  // namespace mozilla::dom

class nsFileControlFrame final : public nsBlockFrame,
                                 public nsIFormControlFrame,
                                 public nsIAnonymousContentCreator {
  using Element = mozilla::dom::Element;

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsFileControlFrame)

  explicit nsFileControlFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext);

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  // nsIFormControlFrame
  nsresult SetFormProperty(nsAtom* aName, const nsAString& aValue) override;
  void SetFocus(bool aOn, bool aRepaint) override;

  void Destroy(DestroyContext&) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void ElementStateChanged(mozilla::dom::ElementState aStates) override;

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>&) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>&,
                                uint32_t aFilter) override;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

 protected:
  class MouseListener;
  friend class MouseListener;
  class MouseListener : public nsIDOMEventListener {
   public:
    NS_DECL_ISUPPORTS

    explicit MouseListener(nsFileControlFrame* aFrame) : mFrame(aFrame) {}

    void ForgetFrame() { mFrame = nullptr; }

   protected:
    virtual ~MouseListener() = default;

    nsFileControlFrame* mFrame;
  };

  class SyncDisabledStateEvent;
  friend class SyncDisabledStateEvent;
  class SyncDisabledStateEvent : public mozilla::Runnable {
   public:
    explicit SyncDisabledStateEvent(nsFileControlFrame* aFrame)
        : mozilla::Runnable("nsFileControlFrame::SyncDisabledStateEvent"),
          mFrame(aFrame) {}

    NS_IMETHOD Run() override {
      nsFileControlFrame* frame =
          static_cast<nsFileControlFrame*>(mFrame.GetFrame());
      NS_ENSURE_STATE(frame);

      frame->SyncDisabledState();
      return NS_OK;
    }

   private:
    WeakFrame mFrame;
  };

  class DnDListener : public MouseListener {
   public:
    explicit DnDListener(nsFileControlFrame* aFrame) : MouseListener(aFrame) {}

    // nsIDOMEventListener
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    NS_IMETHOD HandleEvent(mozilla::dom::Event* aEvent) override;

    nsresult GetBlobImplForWebkitDirectory(mozilla::dom::FileList* aFileList,
                                           mozilla::dom::BlobImpl** aBlobImpl);

    bool IsValidDropData(mozilla::dom::DataTransfer* aDataTransfer);
    bool CanDropTheseFiles(mozilla::dom::DataTransfer* aDataTransfer,
                           bool aSupportsMultiple);
  };

  /**
   * The text box input.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  RefPtr<Element> mTextContent;
  /**
   * The button to open a file or directory picker.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  RefPtr<Element> mBrowseFilesOrDirs;

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
};

#endif  // nsFileControlFrame_h___
