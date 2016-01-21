/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextRange_h__
#define mozilla_a11y_TextRange_h__

#include "mozilla/Move.h"
#include "nsAutoPtr.h"
#include "nsCaseTreatment.h"
#include "nsRect.h"
#include "nsTArray.h"

 class nsIVariant;

namespace mozilla {
namespace a11y {

class Accessible;
class HyperTextAccessible;

/**
 * A text point (hyper text + offset), represents a boundary of text range.
 */
struct TextPoint final
{
  TextPoint(HyperTextAccessible* aContainer, int32_t aOffset) :
    mContainer(aContainer), mOffset(aOffset) { }
  TextPoint(const TextPoint& aPoint) :
    mContainer(aPoint.mContainer), mOffset(aPoint.mOffset) { }

  HyperTextAccessible* mContainer;
  int32_t mOffset;

  bool operator ==(const TextPoint& aPoint) const
    { return mContainer == aPoint.mContainer && mOffset == aPoint.mOffset; }
  bool operator <(const TextPoint& aPoint) const;
};

/**
 * Represents a text range within the text control or document.
 */
class TextRange final
{
public:
  TextRange(HyperTextAccessible* aRoot,
            HyperTextAccessible* aStartContainer, int32_t aStartOffset,
            HyperTextAccessible* aEndContainer, int32_t aEndOffset);
  TextRange() {}
  TextRange(TextRange&& aRange) :
    mRoot(mozilla::Move(aRange.mRoot)),
    mStartContainer(mozilla::Move(aRange.mStartContainer)),
    mEndContainer(mozilla::Move(aRange.mEndContainer)),
    mStartOffset(aRange.mStartOffset), mEndOffset(aRange.mEndOffset) {}

  TextRange& operator= (TextRange&& aRange)
  {
    mRoot = mozilla::Move(aRange.mRoot);
    mStartContainer = mozilla::Move(aRange.mStartContainer);
    mEndContainer = mozilla::Move(aRange.mEndContainer);
    mStartOffset = aRange.mStartOffset;
    mEndOffset = aRange.mEndOffset;
    return *this;
  }

  HyperTextAccessible* StartContainer() const { return mStartContainer; }
  int32_t StartOffset() const { return mStartOffset; }
  HyperTextAccessible* EndContainer() const { return mEndContainer; }
  int32_t EndOffset() const { return mEndOffset; }

  bool operator ==(const TextRange& aRange) const
  {
    return mStartContainer == aRange.mStartContainer &&
      mStartOffset == aRange.mStartOffset &&
      mEndContainer == aRange.mEndContainer && mEndOffset == aRange.mEndOffset;
  }

  TextPoint StartPoint() const { return TextPoint(mStartContainer, mStartOffset); }
  TextPoint EndPoint() const { return TextPoint(mEndContainer, mEndOffset); }

  /**
   * Return a container containing both start and end points.
   */
  Accessible* Container() const;

  /**
   * Return a list of embedded objects enclosed by the text range (includes
   * partially overlapped objects).
   */
  void EmbeddedChildren(nsTArray<Accessible*>* aChildren) const;

  /**
   * Return text enclosed by the range.
   */
  void Text(nsAString& aText) const;

  /**
   * Return list of bounding rects of the text range by lines.
   */
  void Bounds(nsTArray<nsIntRect> aRects) const;

  enum ETextUnit {
    eFormat,
    eWord,
    eLine,
    eParagraph,
    ePage,
    eDocument
  };

  /**
   * Move the range or its points on specified amount of given units.
   */
  void Move(ETextUnit aUnit, int32_t aCount)
  {
    MoveEnd(aUnit, aCount);
    MoveStart(aUnit, aCount);
  }
  void MoveStart(ETextUnit aUnit, int32_t aCount)
  {
    MoveInternal(aUnit, aCount, *mStartContainer, mStartOffset,
                 mEndContainer, mEndOffset);
  }
  void MoveEnd(ETextUnit aUnit, int32_t aCount)
    { MoveInternal(aUnit, aCount, *mEndContainer, mEndOffset); }

  /**
   * Move the range points to the closest unit boundaries.
   */
  void Normalize(ETextUnit aUnit);

  /**
   * Crops the range if it overlaps the given accessible element boundaries,
   * returns true if the range was cropped successfully.
   */
  bool Crop(Accessible* aContainer);

  enum EDirection {
    eBackward,
    eForward
  };

  /**
   * Return range enclosing the found text.
   */
  void FindText(const nsAString& aText, EDirection aDirection,
                nsCaseTreatment aCaseSensitive, TextRange* aFoundRange) const;

  enum EAttr {
    eAnimationStyleAttr,
    eAnnotationObjectsAttr,
    eAnnotationTypesAttr,
    eBackgroundColorAttr,
    eBulletStyleAttr,
    eCapStyleAttr,
    eCaretBidiModeAttr,
    eCaretPositionAttr,
    eCultureAttr,
    eFontNameAttr,
    eFontSizeAttr,
    eFontWeightAttr,
    eForegroundColorAttr,
    eHorizontalTextAlignmentAttr,
    eIndentationFirstLineAttr,
    eIndentationLeadingAttr,
    eIndentationTrailingAttr,
    eIsActiveAttr,
    eIsHiddenAttr,
    eIsItalicAttr,
    eIsReadOnlyAttr,
    eIsSubscriptAttr,
    eIsSuperscriptAttr,
    eLinkAttr,
    eMarginBottomAttr,
    eMarginLeadingAttr,
    eMarginTopAttr,
    eMarginTrailingAttr,
    eOutlineStylesAttr,
    eOverlineColorAttr,
    eOverlineStyleAttr,
    eSelectionActiveEndAttr,
    eStrikethroughColorAttr,
    eStrikethroughStyleAttr,
    eStyleIdAttr,
    eStyleNameAttr,
    eTabsAttr,
    eTextFlowDirectionsAttr,
    eUnderlineColorAttr,
    eUnderlineStyleAttr
  };

  /**
   * Return range enclosing text having requested attribute.
   */
  void FindAttr(EAttr aAttr, nsIVariant* aValue, EDirection aDirection,
                TextRange* aFoundRange) const;

  /**
   * Add/remove the text range from selection.
   */
  void AddToSelection() const;
  void RemoveFromSelection() const;
  void Select() const;

  /**
   * Scroll the text range into view.
   */
  enum EHowToAlign {
    eAlignToTop,
    eAlignToBottom
  };
  void ScrollIntoView(EHowToAlign aHow) const;

  /**
   * Return true if this TextRange object represents an actual range of text.
   */
  bool IsValid() const { return mRoot; }

  void SetStartPoint(HyperTextAccessible* aContainer, int32_t aOffset)
    { mStartContainer = aContainer; mStartOffset = aOffset; }
  void SetEndPoint(HyperTextAccessible* aContainer, int32_t aOffset)
    { mStartContainer = aContainer; mStartOffset = aOffset; }

private:
  TextRange(const TextRange& aRange) = delete;
  TextRange& operator=(const TextRange& aRange) = delete;

  friend class HyperTextAccessible;
  friend class xpcAccessibleTextRange;

  void Set(HyperTextAccessible* aRoot,
           HyperTextAccessible* aStartContainer, int32_t aStartOffset,
           HyperTextAccessible* aEndContainer, int32_t aEndOffset);

  /**
   * Text() method helper.
   * @param  aText            [in,out] calculated text
   * @param  aCurrent         [in] currently traversed node
   * @param  aStartIntlOffset [in] start offset if current node is a text node
   * @return                   true if calculation is not finished yet
   */
  bool TextInternal(nsAString& aText, Accessible* aCurrent,
                    uint32_t aStartIntlOffset) const;

  void MoveInternal(ETextUnit aUnit, int32_t aCount,
                    HyperTextAccessible& aContainer, int32_t aOffset,
                    HyperTextAccessible* aStopContainer = nullptr,
                    int32_t aStopOffset = 0);

  /**
   * A helper method returning a common parent for two given accessible
   * elements.
   */
  Accessible* CommonParent(Accessible* aAcc1, Accessible* aAcc2,
                           nsTArray<Accessible*>* aParents1, uint32_t* aPos1,
                           nsTArray<Accessible*>* aParents2, uint32_t* aPos2) const;

  RefPtr<HyperTextAccessible> mRoot;
  RefPtr<HyperTextAccessible> mStartContainer;
  RefPtr<HyperTextAccessible> mEndContainer;
  int32_t mStartOffset;
  int32_t mEndOffset;
};


} // namespace a11y
} // namespace mozilla

#endif
