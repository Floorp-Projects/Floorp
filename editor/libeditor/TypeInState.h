/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TypeInState_h
#define mozilla_TypeInState_h

#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorForwards.h"
#include "mozilla/EventForwards.h"
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

struct PropItem {
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

  PropItem() = delete;
  PropItem(nsStaticAtom* aTag, nsAtom* aAttribute, const nsAString& aValue,
           SpecifiedStyle aSpecifiedStyle = SpecifiedStyle::Preserve)
      : mTag(aTag),
        mAttribute(aAttribute != nsGkAtoms::_empty ? aAttribute : nullptr),
        mAttributeValueOrCSSValue(aValue),
        mSpecifiedStyle(aSpecifiedStyle) {
    MOZ_COUNT_CTOR(PropItem);
  }
  MOZ_COUNTED_DTOR(PropItem)
};

class StyleCache final {
 public:
  StyleCache() = delete;
  StyleCache(nsStaticAtom& aTag, nsStaticAtom* aAttribute,
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

class MOZ_STACK_CLASS AutoStyleCacheArray final
    : public AutoTArray<StyleCache, 21> {
 public:
  index_type IndexOf(const nsStaticAtom& aTag,
                     const nsStaticAtom* aAttribute) const {
    for (index_type index = 0; index < Length(); ++index) {
      const StyleCache& styleCache = ElementAt(index);
      if (&styleCache.TagRef() == &aTag &&
          styleCache.GetAttribute() == aAttribute) {
        return index;
      }
    }
    return NoIndex;
  }
};

class TypeInState final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TypeInState)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(TypeInState)

  TypeInState();
  void Reset();

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
  void PreserveStyle(const StyleCache& aStyleToPreserve) {
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
  UniquePtr<PropItem> TakeClearingStyle() {
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
  UniquePtr<PropItem> TakePreservedStyle() {
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

  void GetTypingState(bool& isSet, bool& theSetting, nsStaticAtom& aProp,
                      nsAtom* aAttr = nullptr, nsString* aOutValue = nullptr);

 protected:
  virtual ~TypeInState();

  void ClearStyleInternal(
      nsStaticAtom* aHTMLProperty, nsAtom* aAttribute,
      SpecifiedStyle aSpecifiedStyle = SpecifiedStyle::Preserve);

  void RemovePropFromSetList(nsStaticAtom* aProp, nsAtom* aAttr);
  void RemovePropFromClearedList(nsStaticAtom* aProp, nsAtom* aAttr);
  bool IsPropSet(nsStaticAtom& aProp, nsAtom* aAttr, nsAString* outValue);
  bool IsPropSet(nsStaticAtom& aProp, nsAtom* aAttr, nsAString* outValue,
                 int32_t& outIndex);
  bool IsPropCleared(nsStaticAtom* aProp, nsAtom* aAttr);
  bool IsPropCleared(nsStaticAtom* aProp, nsAtom* aAttr, int32_t& outIndex);

  bool IsLinkStyleSet() const {
    int32_t unusedIndex = -1;
    return FindPropInList(nsGkAtoms::a, nullptr, nullptr, mPreservingStyles,
                          unusedIndex);
  }
  bool IsExplicitlyLinkStyleCleared() const {
    int32_t unusedIndex = -1;
    return FindPropInList(nsGkAtoms::a, nullptr, nullptr, mClearingStyles,
                          unusedIndex);
  }
  bool IsOnlyLinkStyleCleared() const {
    return mClearingStyles.Length() == 1 && IsExplicitlyLinkStyleCleared();
  }
  bool AreAllStylesCleared() const {
    int32_t unusedIndex = -1;
    return FindPropInList(nullptr, nullptr, nullptr, mClearingStyles,
                          unusedIndex);
  }
  bool AreSomeStylesSet() const { return !mPreservingStyles.IsEmpty(); }
  bool AreSomeStylesCleared() const { return !mClearingStyles.IsEmpty(); }

  static bool FindPropInList(nsStaticAtom* aProp, nsAtom* aAttr,
                             nsAString* outValue,
                             const nsTArray<UniquePtr<PropItem>>& aList,
                             int32_t& outIndex);

  nsTArray<UniquePtr<PropItem>> mPreservingStyles;
  nsTArray<UniquePtr<PropItem>> mClearingStyles;
  EditorDOMPoint mLastSelectionPoint;
  int32_t mRelativeFontSize;
  Command mLastSelectionCommand;
  bool mMouseDownFiredInLinkElement;
  bool mMouseUpFiredInLinkElement;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_TypeInState_h
