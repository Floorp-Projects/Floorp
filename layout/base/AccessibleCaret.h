/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaret_h__
#define AccessibleCaret_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/AnonymousContent.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIFrame.h"  // for WeakFrame only
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"
#include "nsRect.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"

class nsIFrame;
struct nsPoint;

namespace mozilla {
class PresShell;
namespace dom {
class Element;
class Event;
}  // namespace dom

// -----------------------------------------------------------------------------
// Upon the creation of AccessibleCaret, it will insert DOM Element as an
// anonymous content containing the caret image. The caret appearance and
// position can be controlled by SetAppearance() and SetPosition().
//
// All the rect or point are relative to root frame except being specified
// explicitly.
//
// None of the methods in AccessibleCaret will flush layout or style. To ensure
// that SetPosition() works correctly, the caller must make sure the layout is
// up to date.
//
// Please see the wiki page for more information.
// https://wiki.mozilla.org/AccessibleCaret
//
class AccessibleCaret {
 public:
  explicit AccessibleCaret(PresShell* aPresShell);
  virtual ~AccessibleCaret();

  // This enumeration representing the visibility and visual style of an
  // AccessibleCaret.
  //
  // Use SetAppearance() to change the appearance, and use GetAppearance() to
  // get the current appearance.
  enum class Appearance : uint8_t {
    // Do not display the caret at all.
    None,

    // Display the caret in default style.
    Normal,

    // The caret should be displayed logically but it is kept invisible to the
    // user. This enum is the only difference between "logically visible" and
    // "visually visible". It can be used for reasons such as:
    // 1. Out of scroll port.
    // 2. For UX requirement such as hide a caret in an empty text area.
    NormalNotShown,

    // Display the caret which is tilted to the left.
    Left,

    // Display the caret which is tilted to the right.
    Right
  };

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const Appearance& aAppearance);

  Appearance GetAppearance() const { return mAppearance; }

  virtual void SetAppearance(Appearance aAppearance);

  // Return true if current appearance is either Normal, NormalNotShown, Left,
  // or Right.
  bool IsLogicallyVisible() const { return mAppearance != Appearance::None; }

  // Return true if current appearance is either Normal, Left, or Right.
  bool IsVisuallyVisible() const {
    return (mAppearance != Appearance::None) &&
           (mAppearance != Appearance::NormalNotShown);
  }

  // This enum represents the result returned by SetPosition().
  enum class PositionChangedResult : uint8_t {
    // Both position and the zoom level are not changed.
    NotChanged,

    // The position is changed. (The zoom level may or may not be changed.)
    Position,

    // Only the zoom level is changed. The position is *not* changed.
    Zoom,

    // The position is out of scroll port.
    Invisible
  };

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const PositionChangedResult& aResult);

  virtual PositionChangedResult SetPosition(nsIFrame* aFrame, int32_t aOffset);

  // Does two AccessibleCarets overlap?
  bool Intersects(const AccessibleCaret& aCaret) const;

  // Is the point within the caret's rect? The point should be relative to root
  // frame.
  enum class TouchArea {
    Full,  // Contains both text overlay and caret image.
    CaretImage
  };
  bool Contains(const nsPoint& aPoint, TouchArea aTouchArea) const;

  // The geometry center of the imaginary caret (nsCaret) to which this
  // AccessibleCaret is attached. It is needed when dragging the caret.
  nsPoint LogicalPosition() const { return mImaginaryCaretRect.Center(); }

  // Element for 'Intersects' test. This is the container of the caret image
  // and text-overlay elements. See CreateCaretElement() for the content
  // structure.
  dom::Element& CaretElement() const { return *mCaretElementHolder->Host(); }

  // Ensures that the caret element is made "APZ aware" so that the APZ code
  // doesn't scroll the page when the user is trying to drag the caret.
  void EnsureApzAware();

  bool IsInPositionFixedSubtree() const;

 protected:
  // Argument aRect should be relative to CustomContentContainerFrame().
  void SetCaretElementStyle(const nsRect& aRect, float aZoomLevel);
  void SetTextOverlayElementStyle(const nsRect& aRect, float aZoomLevel);
  void SetCaretImageElementStyle(const nsRect& aRect, float aZoomLevel);

  // Get current zoom level.
  float GetZoomLevel();

  // Element which contains the text overly for the 'Contains' test.
  dom::Element* TextOverlayElement() const;

  // Element which contains the caret image for 'Contains' test.
  dom::Element* CaretImageElement() const;

  nsIFrame* RootFrame() const;

  nsIFrame* CustomContentContainerFrame() const;

  // Transform Appearance to CSS id used in ua.css.
  static nsAutoString AppearanceString(Appearance aAppearance);

  void CreateCaretElement() const;

  // Inject caret element into custom content container.
  void InjectCaretElement(dom::Document*);

  // Remove caret element from custom content container.
  void RemoveCaretElement(dom::Document*);

  // Clear the cached rects and zoom level.
  void ClearCachedData();

  // The top-center of the imaginary caret to which this AccessibleCaret is
  // attached.
  static nsPoint CaretElementPosition(const nsRect& aRect) {
    return aRect.TopLeft() + nsPoint(aRect.width / 2, 0);
  }

  class DummyTouchListener final : public nsIDOMEventListener {
   public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD HandleEvent(mozilla::dom::Event* aEvent) override {
      return NS_OK;
    }

   private:
    virtual ~DummyTouchListener() = default;
  };

  // Member variables
  Appearance mAppearance = Appearance::None;

  // AccessibleCaretManager owns us by a UniquePtr. When it's terminated by
  // AccessibleCaretEventHub::Terminate() which is called in
  // PresShell::Destroy(), it frees us automatically. No need to worry if we
  // outlive mPresShell.
  PresShell* const MOZ_NON_OWNING_REF mPresShell = nullptr;

  RefPtr<dom::AnonymousContent> mCaretElementHolder;

  // This cached rect is relative to the root frame, and is used in
  // LogicalPosition() when dragging a caret.
  nsRect mImaginaryCaretRect;

  // This cached rect is relative to the custom content container, and is used
  // in SetPosition() to check whether the caret position has changed.
  nsRect mImaginaryCaretRectInContainerFrame;

  // The reference frame we used to calculate mImaginaryCaretRect and
  // mImaginaryCaretRectInContainerFrame.
  WeakFrame mImaginaryCaretReferenceFrame;

  // Cache current zoom level to determine whether position is changed.
  float mZoomLevel = 0.0f;

  // A no-op touch-start listener which prevents APZ from panning when dragging
  // the caret.
  RefPtr<DummyTouchListener> mDummyTouchListener{new DummyTouchListener()};
};  // class AccessibleCaret

std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaret::Appearance& aAppearance);

std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaret::PositionChangedResult& aResult);

}  // namespace mozilla

#endif  // AccessibleCaret_h__
