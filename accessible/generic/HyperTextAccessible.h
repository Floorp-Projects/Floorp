/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HyperTextAccessible_h__
#define mozilla_a11y_HyperTextAccessible_h__

#include "AccessibleWrap.h"
#include "mozilla/a11y/HyperTextAccessibleBase.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "nsIFrame.h"  // only for nsSelectionAmount
#include "nsISelectionController.h"

class nsFrameSelection;
class nsIFrame;
class nsRange;
class nsIWidget;

namespace mozilla {
class EditorBase;
namespace dom {
class Selection;
}

namespace a11y {

class TextLeafPoint;
class TextRange;

struct DOMPoint {
  DOMPoint() : node(nullptr), idx(0) {}
  DOMPoint(nsINode* aNode, int32_t aIdx) : node(aNode), idx(aIdx) {}

  nsINode* node;
  int32_t idx;
};

/**
 * Special Accessible that knows how contain both text and embedded objects
 */
class HyperTextAccessible : public AccessibleWrap,
                            public HyperTextAccessibleBase {
 public:
  HyperTextAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HyperTextAccessible, AccessibleWrap)

  // LocalAccessible
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  virtual void Shutdown() override;
  virtual bool RemoveChild(LocalAccessible* aAccessible) override;
  virtual bool InsertChildAt(uint32_t aIndex, LocalAccessible* aChild) override;
  virtual Relation RelationByType(RelationType aType) const override;

  /**
   * Return whether the associated content is editable.
   */
  bool IsEditable() const;

  // HyperTextAccessible (static helper method)

  // Convert content offset to rendered text offset
  nsresult ContentToRenderedOffset(nsIFrame* aFrame, int32_t aContentOffset,
                                   uint32_t* aRenderedOffset) const;

  // Convert rendered text offset to content offset
  nsresult RenderedToContentOffset(nsIFrame* aFrame, uint32_t aRenderedOffset,
                                   int32_t* aContentOffset) const;

  //////////////////////////////////////////////////////////////////////////////
  // HyperLinkAccessible

  /**
   * Return link accessible at the given index.
   */
  LocalAccessible* LinkAt(uint32_t aIndex) {
    Accessible* child = EmbeddedChildAt(aIndex);
    return child ? child->AsLocal() : nullptr;
  }

  //////////////////////////////////////////////////////////////////////////////
  // HyperTextAccessible: DOM point to text offset conversions.

  /**
   * Turn a DOM point (node and offset) into a character offset of this
   * hypertext. Will look for closest match when the DOM node does not have
   * an accessible object associated with it. Will return an offset for the end
   * of the string if the node is not found.
   *
   * @param aNode         [in] the node to look for
   * @param aNodeOffset   [in] the offset to look for
   *                       if -1 just look directly for the node
   *                       if >=0 and aNode is text, this represents a char
   * offset if >=0 and aNode is not text, this represents a child node offset
   * @param aIsEndOffset  [in] if true, then this offset is not inclusive. The
   * character indicated by the offset returned is at [offset - 1]. This means
   * if the passed-in offset is really in a descendant, then the offset
   * returned will come just after the relevant embedded object characer. If
   * false, then the offset is inclusive. The character indicated by the offset
   * returned is at [offset]. If the passed-in offset in inside a descendant,
   * then the returned offset will be on the relevant embedded object char.
   */
  uint32_t DOMPointToOffset(nsINode* aNode, int32_t aNodeOffset,
                            bool aIsEndOffset = false) const;

  /**
   * Transform the given a11y point into the offset relative this hypertext.
   */
  uint32_t TransformOffset(LocalAccessible* aDescendant, uint32_t aOffset,
                           bool aIsEndOffset) const;

  /**
   * Convert the given offset into DOM point.
   *
   * If offset is at text leaf then DOM point is (text node, offsetInTextNode),
   * if before embedded object then (parent node, indexInParent), if after then
   * (parent node, indexInParent + 1).
   */
  DOMPoint OffsetToDOMPoint(int32_t aOffset) const;

  //////////////////////////////////////////////////////////////////////////////
  // TextAccessible

  virtual already_AddRefed<AccAttributes> DefaultTextAttributes() override;

  // HyperTextAccessibleBase provides an overload which takes an Accessible.
  using HyperTextAccessibleBase::GetChildOffset;

  virtual LocalAccessible* GetChildAtOffset(uint32_t aOffset) const override {
    return LocalChildAt(GetChildIndexAtOffset(aOffset));
  }

  /**
   * Return an offset at the given point.
   */
  int32_t OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType) override;

  /**
   * Get/set caret offset, if no caret then -1.
   */
  virtual int32_t CaretOffset() const override;
  virtual void SetCaretOffset(int32_t aOffset) override;

  virtual int32_t CaretLineNumber() override;

  /**
   * Return the caret rect and the widget containing the caret within this
   * text accessible.
   *
   * @param [out] the widget containing the caret
   * @return      the caret rect
   */
  mozilla::LayoutDeviceIntRect GetCaretRect(nsIWidget** aWidget);

  /**
   * Return true if caret is at end of line.
   */
  bool IsCaretAtEndOfLine() const;

  virtual int32_t SelectionCount() override;

  virtual bool SelectionBoundsAt(int32_t aSelectionNum, int32_t* aStartOffset,
                                 int32_t* aEndOffset) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual bool RemoveFromSelection(
      int32_t aSelectionNum) override;

  virtual void ScrollSubstringToPoint(int32_t aStartOffset, int32_t aEndOffset,
                                      uint32_t aCoordinateType, int32_t aX,
                                      int32_t aY) override;

  virtual void SelectionRanges(nsTArray<TextRange>* aRanges) const override;

  //////////////////////////////////////////////////////////////////////////////
  // EditableTextAccessible

  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void ReplaceText(
      const nsAString& aText) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void InsertText(
      const nsAString& aText, int32_t aPosition) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void CopyText(int32_t aStartPos,
                                                    int32_t aEndPos) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void CutText(int32_t aStartPos,
                                                   int32_t aEndPos) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void DeleteText(int32_t aStartPos,
                                                      int32_t aEndPos) override;
  MOZ_CAN_RUN_SCRIPT virtual void PasteText(int32_t aPosition) override;

  /**
   * Return the editor associated with the accessible.
   * The result may be either TextEditor or HTMLEditor.
   */
  virtual already_AddRefed<EditorBase> GetEditor() const;

  /**
   * Return DOM selection object for the accessible.
   */
  dom::Selection* DOMSelection() const;

 protected:
  virtual ~HyperTextAccessible() {}

  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  // HyperTextAccessible

  // Selection helpers

  /**
   * Return frame selection object for the accessible.
   */
  already_AddRefed<nsFrameSelection> FrameSelection() const;

  /**
   * Return selection ranges within the accessible subtree.
   */
  void GetSelectionDOMRanges(SelectionType aSelectionType,
                             nsTArray<nsRange*>* aRanges);

  // TODO: annotate this with `MOZ_CAN_RUN_SCRIPT` instead.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult SetSelectionRange(int32_t aStartPos,
                                                         int32_t aEndPos);

  // Helpers

  /**
   * Set xml-roles attributes for MathML elements.
   * @param aAttributes
   */
  void SetMathMLXMLRoles(AccAttributes* aAttributes);

  // HyperTextAccessibleBase
  virtual const Accessible* Acc() const override { return this; }

  virtual nsTArray<int32_t>& GetCachedHyperTextOffsets() override {
    return mOffsets;
  }

 private:
  /**
   * End text offsets array.
   */
  mutable nsTArray<int32_t> mOffsets;
};

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible downcasting method

inline HyperTextAccessible* LocalAccessible::AsHyperText() {
  return IsHyperText() ? static_cast<HyperTextAccessible*>(this) : nullptr;
}

inline HyperTextAccessibleBase* LocalAccessible::AsHyperTextBase() {
  return AsHyperText();
}

}  // namespace a11y
}  // namespace mozilla

#endif
