/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PendingStyles_h
#define mozilla_PendingStyles_h

#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorForwards.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nscore.h"

class nsINode;

namespace mozilla {
namespace dom {
class MouseEvent;
class Selection;
}  // namespace dom

enum class SpecifiedStyle : uint8_t { Preserve, Discard };

class PendingStyle final {
 public:
  PendingStyle() = delete;
  PendingStyle(nsStaticAtom* aTag, nsAtom* aAttribute, const nsAString& aValue,
               SpecifiedStyle aSpecifiedStyle = SpecifiedStyle::Preserve)
      : mTag(aTag),
        mAttribute(aAttribute != nsGkAtoms::_empty ? aAttribute : nullptr),
        mAttributeValueOrCSSValue(aValue),
        mSpecifiedStyle(aSpecifiedStyle) {
    MOZ_COUNT_CTOR(PendingStyle);
  }
  MOZ_COUNTED_DTOR(PendingStyle)

  MOZ_KNOWN_LIVE nsStaticAtom* GetTag() const { return mTag; }
  MOZ_KNOWN_LIVE nsAtom* GetAttribute() const { return mAttribute; }
  const nsString& AttributeValueOrCSSValueRef() const {
    return mAttributeValueOrCSSValue;
  }
  void UpdateAttributeValueOrCSSValue(const nsAString& aNewValue) {
    mAttributeValueOrCSSValue = aNewValue;
  }
  SpecifiedStyle GetSpecifiedStyle() const { return mSpecifiedStyle; }

 private:
  MOZ_KNOWN_LIVE nsStaticAtom* const mTag = nullptr;
  // TODO: Once we stop using `HTMLEditor::SetInlinePropertiesAsSubAction` to
  //       add any attributes of <a href>, we can make this `nsStaticAtom*`.
  MOZ_KNOWN_LIVE const RefPtr<nsAtom> mAttribute;
  // If the editor is in CSS mode, this value is the property value.
  // If the editor is in HTML mode, this value is not empty only when
  // - mAttribute is not nullptr
  // - mTag is CSS invertible style and "-moz-editor-invert-value"
  nsString mAttributeValueOrCSSValue;
  // Whether the class and style attribute should be preserved or discarded.
  const SpecifiedStyle mSpecifiedStyle = SpecifiedStyle::Preserve;
};

class PendingStyleCache final {
 public:
  PendingStyleCache() = delete;
  PendingStyleCache(nsStaticAtom& aTag, nsStaticAtom* aAttribute,
                    const nsAString& aValue)
      : mTag(aTag), mAttribute(aAttribute), mAttributeValueOrCSSValue(aValue) {}

  MOZ_KNOWN_LIVE nsStaticAtom& TagRef() const { return mTag; }
  MOZ_KNOWN_LIVE nsStaticAtom* GetAttribute() const { return mAttribute; }
  const nsString& AttributeValueOrCSSValueRef() const {
    return mAttributeValueOrCSSValue;
  }

 private:
  MOZ_KNOWN_LIVE nsStaticAtom& mTag;
  MOZ_KNOWN_LIVE nsStaticAtom* const mAttribute;
  const nsString mAttributeValueOrCSSValue;
};

class MOZ_STACK_CLASS AutoPendingStyleCacheArray final
    : public AutoTArray<PendingStyleCache, 21> {
 public:
  index_type IndexOf(const nsStaticAtom& aTag,
                     const nsStaticAtom* aAttribute) const {
    for (index_type index = 0; index < Length(); ++index) {
      const PendingStyleCache& styleCache = ElementAt(index);
      if (&styleCache.TagRef() == &aTag &&
          styleCache.GetAttribute() == aAttribute) {
        return index;
      }
    }
    return NoIndex;
  }
};

enum class PendingStyleState {
  // Will be not changed in new content
  NotUpdated,
  // Will be applied to new content
  BeingPreserved,
  // Will be cleared from new content
  BeingCleared,
};

/******************************************************************************
 * PendingStyles stores pending inline styles which WILL be applied to new
 * content when it'll be inserted.  I.e., updating styles of this class means
 * that it does NOT update the DOM tree immediately.
 ******************************************************************************/
class PendingStyles final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PendingStyles)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(PendingStyles)

  PendingStyles() = default;

  void Reset() {
    mClearingStyles.Clear();
    mPreservingStyles.Clear();
  }

  nsresult UpdateSelState(const HTMLEditor& aHTMLEditor);

  /**
   * PreHandleMouseEvent() is called when `HTMLEditorEventListener` receives
   * "mousedown" and "mouseup" events.  Note that `aMouseDownOrUpEvent` may not
   * be acceptable event for the `HTMLEditor`, but this is called even in
   * the case because the event may cause a following `OnSelectionChange()`
   * call.
   */
  void PreHandleMouseEvent(const dom::MouseEvent& aMouseDownOrUpEvent);

  void PreHandleSelectionChangeCommand(Command aCommand);
  void PostHandleSelectionChangeCommand(const HTMLEditor& aHTMLEditor,
                                        Command aCommand);

  void OnSelectionChange(const HTMLEditor& aHTMLEditor, int16_t aReason);

  /**
   * Preserve the style for next inserting content.  E.g., when user types next
   * character, an inline element which provides the style will be inserted
   * and the typing character will appear in it.
   *
   * @param aHTMLProperty       The HTML tag name which represents the style.
   *                            For example, nsGkAtoms::b for bold text.
   * @param aAttribute          nullptr or attribute name which represents the
   *                            style with aHTMLProperty.  E.g., nsGkAtoms::size
   *                            for nsGkAtoms::font.
   * @param aAttributeValueOrCSSValue
   *                            New value of aAttribute or new CSS value if the
   *                            editor is in the CSS mode.
   */
  void PreserveStyle(nsStaticAtom& aHTMLProperty, nsAtom* aAttribute,
                     const nsAString& aAttributeValueOrCSSValue);

  /**
   * Preserve the styles with new values in aStylesToPreserve.
   * See above for the detail.
   */
  void PreserveStyles(
      const nsTArray<EditorInlineStyleAndValue>& aStylesToPreserve);

  /**
   * Preserve the style with new value specified with aStyleToPreserve.
   * See above for the detail.
   */
  void PreserveStyle(const PendingStyleCache& aStyleToPreserve) {
    PreserveStyle(aStyleToPreserve.TagRef(), aStyleToPreserve.GetAttribute(),
                  aStyleToPreserve.AttributeValueOrCSSValueRef());
  }

  /**
   * Clear the style when next content is inserted.  E.g., when user types next
   * character, it'll will be inserted after parent element which provides the
   * style and clones element which represents other styles in the parent
   * element.
   *
   * @param aHTMLProperty       The HTML tag name which represents the style.
   *                            For example, nsGkAtoms::b for bold text.
   * @param aAttribute          nullptr or attribute name which represents the
   *                            style with aHTMLProperty.  E.g., nsGkAtoms::size
   *                            for nsGkAtoms::font.
   */
  void ClearStyle(nsStaticAtom& aHTMLProperty, nsAtom* aAttribute) {
    ClearStyleInternal(&aHTMLProperty, aAttribute);
  }

  /**
   * Clear all styles specified by aStylesToClear when next content is inserted.
   * See above for the detail.
   */
  void ClearStyles(const nsTArray<EditorInlineStyle>& aStylesToClear);

  /**
   * Clear all styles when next inserting content.  E.g., when user types next
   * character, it will be inserted outside any inline parents which provides
   * current text style.
   */
  void ClearAllStyles() {
    // XXX Why don't we clear mClearingStyles first?
    ClearStyleInternal(nullptr, nullptr);
  }

  /**
   * Clear <a> element and discard styles which is applied by it.
   */
  void ClearLinkAndItsSpecifiedStyle() {
    ClearStyleInternal(nsGkAtoms::a, nullptr, SpecifiedStyle::Discard);
  }

  /**
   * TakeClearingStyle() hands back next property item on the clearing styles.
   * This must be used only for handling to clear the styles from inserting
   * content.
   */
  UniquePtr<PendingStyle> TakeClearingStyle() {
    if (mClearingStyles.IsEmpty()) {
      return nullptr;
    }
    return mClearingStyles.PopLastElement();
  }

  /**
   * TakePreservedStyle() hands back next property item on the preserving
   * styles. This should be used only for handling setting new style for
   * inserted content.
   */
  UniquePtr<PendingStyle> TakePreservedStyle() {
    if (mPreservingStyles.IsEmpty()) {
      return nullptr;
    }
    return mPreservingStyles.PopLastElement();
  }

  /**
   * TakeRelativeFontSize() hands back relative font value, which is then
   * cleared out.
   */
  int32_t TakeRelativeFontSize();

  /**
   * GetStyleState() returns whether the style will be applied to new content,
   * removed from new content or not changed.
   *
   * @param aHTMLProperty       The HTML tag name which represents the style.
   *                            For example, nsGkAtoms::b for bold text.
   * @param aAttribute          nullptr or attribute name which represents the
   *                            style with aHTMLProperty.  E.g., nsGkAtoms::size
   *                            for nsGkAtoms::font.
   * @param aOutNewAttributeValueOrCSSValue
   *                            [out, optional] If applied to new content, this
   *                            is set to the new value.
   */
  PendingStyleState GetStyleState(
      nsStaticAtom& aHTMLProperty, nsAtom* aAttribute = nullptr,
      nsString* aOutNewAttributeValueOrCSSValue = nullptr) const;

 protected:
  virtual ~PendingStyles() { Reset(); };

  void ClearStyleInternal(
      nsStaticAtom* aHTMLProperty, nsAtom* aAttribute,
      SpecifiedStyle aSpecifiedStyle = SpecifiedStyle::Preserve);

  void CancelPreservingStyle(nsStaticAtom* aHTMLProperty, nsAtom* aAttribute);
  void CancelClearingStyle(nsStaticAtom& aHTMLProperty, nsAtom* aAttribute);

  Maybe<size_t> IndexOfPreservingStyle(nsStaticAtom& aHTMLProperty,
                                       nsAtom* aAttribute,
                                       nsAString* aOutValue = nullptr) const {
    return IndexOfStyleInArray(&aHTMLProperty, aAttribute, aOutValue,
                               mPreservingStyles);
  }
  Maybe<size_t> IndexOfClearingStyle(nsStaticAtom* aHTMLProperty,
                                     nsAtom* aAttribute) const {
    return IndexOfStyleInArray(aHTMLProperty, aAttribute, nullptr,
                               mClearingStyles);
  }

  bool IsLinkStyleSet() const {
    return IndexOfPreservingStyle(*nsGkAtoms::a, nullptr).isSome();
  }
  bool IsExplicitlyLinkStyleCleared() const {
    return IndexOfClearingStyle(nsGkAtoms::a, nullptr).isSome();
  }
  bool IsOnlyLinkStyleCleared() const {
    return mClearingStyles.Length() == 1 && IsExplicitlyLinkStyleCleared();
  }
  bool IsStyleCleared(nsStaticAtom* aHTMLProperty, nsAtom* aAttribute) const {
    return IndexOfClearingStyle(aHTMLProperty, aAttribute).isSome() ||
           AreAllStylesCleared();
  }
  bool AreAllStylesCleared() const {
    return IndexOfClearingStyle(nullptr, nullptr).isSome();
  }
  bool AreSomeStylesSet() const { return !mPreservingStyles.IsEmpty(); }
  bool AreSomeStylesCleared() const { return !mClearingStyles.IsEmpty(); }

  static Maybe<size_t> IndexOfStyleInArray(
      nsStaticAtom* aHTMLProperty, nsAtom* aAttribute, nsAString* aOutValue,
      const nsTArray<UniquePtr<PendingStyle>>& aArray);

  nsTArray<UniquePtr<PendingStyle>> mPreservingStyles;
  nsTArray<UniquePtr<PendingStyle>> mClearingStyles;
  EditorDOMPoint mLastSelectionPoint;
  int32_t mRelativeFontSize = 0;
  Command mLastSelectionCommand = Command::DoNothing;
  bool mMouseDownFiredInLinkElement = false;
  bool mMouseUpFiredInLinkElement = false;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_PendingStyles_h
