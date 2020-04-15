/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditUtils_h
#define HTMLEditUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "nsGkAtoms.h"

class nsAtom;

namespace mozilla {

class HTMLEditUtils final {
  using Element = dom::Element;
  using Selection = dom::Selection;

 public:
  /**
   * IsBlockElement() returns true if aContent is an element and it should
   * be treated as a block.  (This does not refer style information.)
   */
  static bool IsBlockElement(const nsIContent& aContent);
  /**
   * IsInlineElement() returns true if aElement is an element node but
   * shouldn't be treated as a block or aElement is not an element.
   * XXX This looks odd.  For example, how about a comment node?
   */
  static bool IsInlineElement(const nsIContent& aContent) {
    return !IsBlockElement(aContent);
  }

  static bool IsInlineStyle(nsINode* aNode);
  /**
   * IsRemovableInlineStyleElement() returns true if aElement is an inline
   * element and can be removed or split to in order to modifying inline
   * styles.
   */
  static bool IsRemovableInlineStyleElement(dom::Element& aElement);
  static bool IsFormatNode(nsINode* aNode);
  static bool IsNodeThatCanOutdent(nsINode* aNode);
  static bool IsHeader(nsINode& aNode);
  static bool IsListItem(nsINode* aNode);
  static bool IsTable(nsINode* aNode);
  static bool IsTableRow(nsINode* aNode);
  static bool IsTableElement(nsINode* aNode);
  static bool IsTableElementButNotTable(nsINode* aNode);
  static bool IsTableCell(nsINode* node);
  static bool IsTableCellOrCaption(nsINode& aNode);
  static bool IsList(nsINode* aNode);
  static bool IsPre(nsINode* aNode);
  static bool IsImage(nsINode* aNode);
  static bool IsLink(nsINode* aNode);
  static bool IsNamedAnchor(nsINode* aNode);
  static bool IsMozDiv(nsINode* aNode);
  static bool IsMailCite(nsINode* aNode);
  static bool IsFormWidget(nsINode* aNode);
  static bool SupportsAlignAttr(nsINode& aNode);
  static bool CanContain(int32_t aParent, int32_t aChild);
  static bool IsContainer(int32_t aTag);

  /**
   * See execCommand spec:
   * https://w3c.github.io/editing/execCommand.html#non-list-single-line-container
   * https://w3c.github.io/editing/execCommand.html#single-line-container
   */
  static bool IsNonListSingleLineContainer(nsINode& aNode);
  static bool IsSingleLineContainer(nsINode& aNode);

  static EditAction GetEditActionForInsert(const nsAtom& aTagName);
  static EditAction GetEditActionForRemoveList(const nsAtom& aTagName);
  static EditAction GetEditActionForInsert(const Element& aElement);
  static EditAction GetEditActionForFormatText(const nsAtom& aProperty,
                                               const nsAtom* aAttribute,
                                               bool aToSetStyle);
  static EditAction GetEditActionForAlignment(const nsAString& aAlignType);
};

/**
 * DefinitionListItemScanner() scans given `<dl>` element's children.
 * Then, you can check whether `<dt>` and/or `<dd>` elements are in it.
 */
class MOZ_STACK_CLASS DefinitionListItemScanner final {
 public:
  DefinitionListItemScanner() = delete;
  explicit DefinitionListItemScanner(dom::Element& aDLElement) {
    MOZ_ASSERT(aDLElement.IsHTMLElement(nsGkAtoms::dl));
    for (nsIContent* child = aDLElement.GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsHTMLElement(nsGkAtoms::dt)) {
        mDTFound = true;
        if (mDDFound) {
          break;
        }
        continue;
      }
      if (child->IsHTMLElement(nsGkAtoms::dd)) {
        mDDFound = true;
        if (mDTFound) {
          break;
        }
        continue;
      }
    }
  }

  bool DTElementFound() const { return mDTFound; }
  bool DDElementFound() const { return mDDFound; }

 private:
  bool mDTFound = false;
  bool mDDFound = false;
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditUtils_h
