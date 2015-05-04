/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaret_h__
#define AccessibleCaret_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/dom/Element.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsRect.h"
#include "nsRefPtr.h"
#include "nsString.h"

class nsIDocument;
class nsIFrame;
class nsIPresShell;
struct nsPoint;

namespace mozilla {

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
class AccessibleCaret final
{
public:
  explicit AccessibleCaret(nsIPresShell* aPresShell);
  ~AccessibleCaret();

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

  Appearance GetAppearance() const
  {
    return mAppearance;
  }

  void SetAppearance(Appearance aAppearance);

  // Return true if current appearance is either Normal, NormalNotShown, Left,
  // or Right.
  bool IsLogicallyVisible() const
  {
      return mAppearance != Appearance::None;
  }

  // Return true if current appearance is either Normal, Left, or Right.
  bool IsVisuallyVisible() const
  {
    return (mAppearance != Appearance::None) &&
           (mAppearance != Appearance::NormalNotShown);
  }

  // Set true to enable the "Text Selection Bar" described in "Text Selection
  // Visual Spec" in bug 921965.
  void SetSelectionBarEnabled(bool aEnabled);

  // This enumeration representing the result returned by SetPosition().
  enum class PositionChangedResult : uint8_t {
    // Position is not changed.
    NotChanged,

    // Position is changed.
    Changed,

    // Position is out of scroll port.
    Invisible
  };
  PositionChangedResult SetPosition(nsIFrame* aFrame, int32_t aOffset);

  // Does two AccessibleCarets overlap?
  bool Intersects(const AccessibleCaret& aCaret) const;

  // Is the point within the caret's rect? The point should be relative to root
  // frame.
  bool Contains(const nsPoint& aPoint) const;

  // The geometry center of the imaginary caret (nsCaret) to which this
  // AccessibleCaret is attached. It is needed when dragging the caret.
  nsPoint LogicalPosition() const
  {
    return mImaginaryCaretRect.Center();
  }

  // Element for 'Intersects' test. Container of image and bar elements.
  dom::Element* CaretElement() const
  {
    return mCaretElementHolder->GetContentNode();
  }

private:
  // Argument aRect should be relative to CustomContentContainerFrame().
  void SetCaretElementPosition(const nsRect& aRect);
  void SetSelectionBarElementPosition(const nsRect& aRect);

  // Element which contains the caret image for 'Contains' test.
  dom::Element* CaretImageElement() const
  {
    return CaretElement()->GetFirstElementChild();
  }

  // Element which represents the text selection bar.
  dom::Element* SelectionBarElement() const
  {
    return CaretElement()->GetLastElementChild();
  }

  nsIFrame* RootFrame() const
  {
    return mPresShell->GetRootFrame();
  }

  nsIFrame* CustomContentContainerFrame() const;

  // Transform Appearance to CSS class name in ua.css.
  static nsString AppearanceString(Appearance aAppearance);

  already_AddRefed<dom::Element> CreateCaretElement(nsIDocument* aDocument) const;

  // Inject caret element into custom content container.
  void InjectCaretElement(nsIDocument* aDocument);

  // Remove caret element from custom content container.
  void RemoveCaretElement(nsIDocument* aDocument);

  // The bottom-center of the imaginary caret to which this AccessibleCaret is
  // attached.
  static nsPoint CaretElementPosition(const nsRect& aRect)
  {
    return aRect.TopLeft() + nsPoint(aRect.width / 2, aRect.height);
  }

  class DummyTouchListener final : public nsIDOMEventListener
  {
  public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override
    {
      return NS_OK;
    }

  private:
    virtual ~DummyTouchListener() {};
  };

  // Member variables
  Appearance mAppearance = Appearance::None;

  bool mSelectionBarEnabled = false;

  // AccessibleCaretManager owns us. When it's destroyed by
  // AccessibleCaretEventHub::Terminate() which is called in
  // PresShell::Destroy(), it frees us automatically. No need to worry we
  // outlive mPresShell.
  nsIPresShell* MOZ_NON_OWNING_REF const mPresShell = nullptr;

  nsRefPtr<dom::AnonymousContent> mCaretElementHolder;

  // mImaginaryCaretRect is relative to root frame.
  nsRect mImaginaryCaretRect;

  // A no-op touch-start listener which prevents APZ from panning when dragging
  // the caret.
  nsRefPtr<DummyTouchListener> mDummyTouchListener{new DummyTouchListener()};

}; // class AccessibleCaret

} // namespace mozilla

#endif // AccessibleCaret_h__
