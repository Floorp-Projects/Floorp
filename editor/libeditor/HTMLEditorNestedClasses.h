/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditorNestedClasses_h
#define HTMLEditorNestedClasses_h

#include "EditorForwards.h"
#include "HTMLEditor.h"       // for HTMLEditor
#include "HTMLEditHelpers.h"  // for EditorInlineStyleAndValue

#include "mozilla/Attributes.h"
#include "mozilla/Result.h"

namespace mozilla {

/*****************************************************************************
 * AutoInlineStyleSetter is a temporary class to set an inline style to
 * specific nodes.
 ****************************************************************************/

class MOZ_STACK_CLASS HTMLEditor::AutoInlineStyleSetter final
    : private EditorInlineStyleAndValue {
  using Element = dom::Element;
  using Text = dom::Text;

 public:
  explicit AutoInlineStyleSetter(
      const EditorInlineStyleAndValue& aStyleAndValue)
      : EditorInlineStyleAndValue(aStyleAndValue) {}

  /**
   * Split aText at aStartOffset and aEndOffset (except when they are start or
   * end of its data) and wrap the middle text node in an element to apply the
   * style.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<SplitRangeOffFromNodeResult, nsresult>
  SplitTextNodeAndApplyStyleToMiddleNode(HTMLEditor& aHTMLEditor, Text& aText,
                                         uint32_t aStartOffset,
                                         uint32_t aEndOffset) const;

  /**
   * Remove same style from children and apply the style entire (except
   * non-editable nodes) aContent.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  ApplyStyleToNodeOrChildrenAndRemoveNestedSameStyle(
      HTMLEditor& aHTMLEditor, nsIContent& aContent) const;

 private:
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult> ApplyStyle(
      HTMLEditor& aHTMLEditor, nsIContent& aContent) const;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  ApplyCSSTextDecoration(HTMLEditor& aHTMLEditor, nsIContent& aContent) const;

  /**
   * ElementIsGoodContainerForTheStyle() returns true if aElement is a
   * good container for applying the style to a node.  I.e., if this returns
   * true, moving nodes into aElement is enough to apply the style to them.
   * Otherwise, you need to create new element for the style.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<bool, nsresult>
  ElementIsGoodContainerForTheStyle(HTMLEditor& aHTMLEditor,
                                    Element& aElement) const;
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditorNestedClasses_h
