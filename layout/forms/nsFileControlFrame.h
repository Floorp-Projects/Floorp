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

namespace mozilla {
namespace dom {
class FileList;
class BlobImpl;
class DataTransfer;
}  // namespace dom
}  // namespace mozilla

class nsFileControlFrame final : public nsBlockFrame,
                                 public nsIFormControlFrame,
                                 public nsIAnonymousContentCreator {
  using Element = mozilla::dom::Element;

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsFileControlFrame)

  explicit nsFileControlFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext);

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsAtom* aName,
                                   const nsAString& aValue) override;
  virtual void SetFocus(bool aOn, bool aRepaint) override;

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(
      nsTArray<ContentInfo>& aElements) override;
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

    explicit MouseListener(nsFileControlFrame* aFrame) : mFrame(aFrame) {}

    void ForgetFrame() { mFrame = nullptr; }

   protected:
    virtual ~MouseListener() = default;

    nsFileControlFrame* mFrame;
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

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsBlockFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

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
   * Crop aText to fit inside aWidth using the styles of aFrame.
   * @return true if aText was modified
   */
  static bool CropTextToWidth(gfxContext& aRenderingContext,
                              const nsIFrame* aFrame, nscoord aWidth,
                              nsString& aText);

  /**
   * Updates the displayed value by using aValue.
   */
  void UpdateDisplayedValue(const nsAString& aValue, bool aNotify);
};

#endif  // nsFileControlFrame_h___
