/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

MARKUPMAP(
    a,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // Only some roles truly enjoy life as HTMLLinkAccessibles, for
      // details see closed bug 494807.
      const nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(aElement);
      if (roleMapEntry && roleMapEntry->role != roles::NOTHING &&
          roleMapEntry->role != roles::LINK) {
        return new HyperTextAccessibleWrap(aElement, aContext->Document());
      }

      return new HTMLLinkAccessible(aElement, aContext->Document());
    },
    roles::LINK)

MARKUPMAP(abbr, New_HyperText, 0)

MARKUPMAP(acronym, New_HyperText, 0)

MARKUPMAP(article, New_HyperText, roles::ARTICLE, Attr(xmlroles, article))

MARKUPMAP(aside, New_HyperText, roles::LANDMARK)

MARKUPMAP(blockquote, New_HyperText, roles::BLOCKQUOTE)

MARKUPMAP(
    button,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLButtonAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    caption,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      if (aContext->IsTable()) {
        dom::HTMLTableElement* tableEl =
            dom::HTMLTableElement::FromNode(aContext->GetContent());
        if (tableEl && tableEl == aElement->GetParent() &&
            tableEl->GetCaption() == aElement) {
          return new HTMLCaptionAccessible(aElement, aContext->Document());
        }
      }
      return nullptr;
    },
    0)

MARKUPMAP(dd, New_HTMLDtOrDd<HyperTextAccessibleWrap>, roles::DEFINITION)

MARKUPMAP(del, New_HyperText, roles::CONTENT_DELETION)

MARKUPMAP(details, New_HyperText, roles::DETAILS)

MARKUPMAP(
    div,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // Never create an accessible if we're part of an anonymous
      // subtree.
      if (aElement->IsInAnonymousSubtree()) {
        return nullptr;
      }
      // Always create an accessible if the div has an id.
      if (aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::id)) {
        return new HyperTextAccessibleWrap(aElement, aContext->Document());
      }
      // Never create an accessible if the div is not display:block; or
      // display:inline-block;
      nsAutoString displayValue;
      StyleInfo styleInfo(aElement);
      styleInfo.Display(displayValue);
      if (displayValue != NS_LITERAL_STRING("block") &&
          displayValue != NS_LITERAL_STRING("inline-block")) {
        return nullptr;
      }
      // Check for various conditions to determine if this is a block
      // break and needs to be rendered.
      // If its previous sibling is an inline element, we probably want
      // to break, so render.
      nsIContent* prevSibling = aElement->GetPreviousSibling();
      if (prevSibling) {
        nsIFrame* prevSiblingFrame = prevSibling->GetPrimaryFrame();
        if (prevSiblingFrame && prevSiblingFrame->IsInlineFrame()) {
          return new HyperTextAccessibleWrap(aElement, aContext->Document());
        }
      }
      // Now, check the children.
      nsIContent* firstChild = aElement->GetFirstChild();
      if (firstChild) {
        // Render it if it is a text node.
        if (firstChild->IsText()) {
          return new HyperTextAccessibleWrap(aElement, aContext->Document());
        }
        // Check to see if first child has an inline frame.
        nsIFrame* firstChildFrame = firstChild->GetPrimaryFrame();
        if (firstChildFrame && (firstChildFrame->IsInlineFrame() ||
                                firstChildFrame->IsBrFrame())) {
          return new HyperTextAccessibleWrap(aElement, aContext->Document());
        }
        nsIContent* lastChild = aElement->GetLastChild();
        if (lastChild && lastChild != firstChild) {
          // Render it if it is a text node.
          if (lastChild->IsText()) {
            return new HyperTextAccessibleWrap(aElement, aContext->Document());
          }
          // Check to see if last child has an inline frame.
          nsIFrame* lastChildFrame = lastChild->GetPrimaryFrame();
          if (lastChildFrame && (lastChildFrame->IsInlineFrame() ||
                                 lastChildFrame->IsBrFrame())) {
            return new HyperTextAccessibleWrap(aElement, aContext->Document());
          }
        }
      }
      return nullptr;
    },
    roles::SECTION)

MARKUPMAP(
    dl,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::DEFINITION_LIST)

MARKUPMAP(dt, New_HTMLDtOrDd<HTMLLIAccessible>, roles::TERM)

MARKUPMAP(
    figcaption,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLFigcaptionAccessible(aElement, aContext->Document());
    },
    roles::CAPTION)

MARKUPMAP(
    figure,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLFigureAccessible(aElement, aContext->Document());
    },
    roles::FIGURE, Attr(xmlroles, figure))

MARKUPMAP(
    fieldset,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLGroupboxAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    form,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLFormAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    footer,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLHeaderOrFooterAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    header,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLHeaderOrFooterAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(h1, New_HyperText, roles::HEADING)

MARKUPMAP(h2, New_HyperText, roles::HEADING)

MARKUPMAP(h3, New_HyperText, roles::HEADING)

MARKUPMAP(h4, New_HyperText, roles::HEADING)

MARKUPMAP(h5, New_HyperText, roles::HEADING)

MARKUPMAP(h6, New_HyperText, roles::HEADING)

MARKUPMAP(
    hr,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLHRAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    input,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // TODO(emilio): This would be faster if it used
      // HTMLInputElement's already-parsed representation.
      if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                nsGkAtoms::checkbox, eIgnoreCase)) {
        return new CheckboxAccessible(aElement, aContext->Document());
      }
      if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                nsGkAtoms::image, eIgnoreCase)) {
        return new HTMLButtonAccessible(aElement, aContext->Document());
      }
      if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                nsGkAtoms::radio, eIgnoreCase)) {
        return new HTMLRadioButtonAccessible(aElement, aContext->Document());
      }
      if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                nsGkAtoms::time, eIgnoreCase)) {
        return new EnumRoleAccessible<roles::GROUPING>(aElement,
                                                       aContext->Document());
      }
      if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                nsGkAtoms::date, eIgnoreCase)) {
        return new EnumRoleAccessible<roles::DATE_EDITOR>(aElement,
                                                          aContext->Document());
      }
      return nullptr;
    },
    0)

MARKUPMAP(ins, New_HyperText, roles::CONTENT_INSERTION)

MARKUPMAP(
    label,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLLabelAccessible(aElement, aContext->Document());
    },
    roles::LABEL)

MARKUPMAP(
    legend,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLLegendAccessible(aElement, aContext->Document());
    },
    roles::LABEL)

MARKUPMAP(
    li,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // If list item is a child of accessible list then create an
      // accessible for it unconditionally by tag name. nsBlockFrame
      // creates the list item accessible for other elements styled as
      // list items.
      if (aContext->IsList() &&
          aContext->GetContent() == aElement->GetParent()) {
        return new HTMLLIAccessible(aElement, aContext->Document());
      }

      return nullptr;
    },
    0)

MARKUPMAP(main, New_HyperText, roles::LANDMARK)

MARKUPMAP(map, nullptr, roles::TEXT_CONTAINER)

MARKUPMAP(math, New_HyperText, roles::MATHML_MATH)

MARKUPMAP(mi_, New_HyperText, roles::MATHML_IDENTIFIER)

MARKUPMAP(mn_, New_HyperText, roles::MATHML_NUMBER)

MARKUPMAP(mo_, New_HyperText, roles::MATHML_OPERATOR,
          AttrFromDOM(accent_, accent_), AttrFromDOM(fence_, fence_),
          AttrFromDOM(separator_, separator_), AttrFromDOM(largeop_, largeop_))

MARKUPMAP(mtext_, New_HyperText, roles::MATHML_TEXT)

MARKUPMAP(ms_, New_HyperText, roles::MATHML_STRING_LITERAL)

MARKUPMAP(mglyph_, New_HyperText, roles::MATHML_GLYPH)

MARKUPMAP(mrow_, New_HyperText, roles::MATHML_ROW)

MARKUPMAP(mfrac_, New_HyperText, roles::MATHML_FRACTION,
          AttrFromDOM(bevelled_, bevelled_),
          AttrFromDOM(linethickness_, linethickness_))

MARKUPMAP(msqrt_, New_HyperText, roles::MATHML_SQUARE_ROOT)

MARKUPMAP(mroot_, New_HyperText, roles::MATHML_ROOT)

MARKUPMAP(mfenced_, New_HyperText, roles::MATHML_FENCED,
          AttrFromDOM(close, close), AttrFromDOM(open, open),
          AttrFromDOM(separators_, separators_))

MARKUPMAP(menclose_, New_HyperText, roles::MATHML_ENCLOSED,
          AttrFromDOM(notation_, notation_))

MARKUPMAP(mstyle_, New_HyperText, roles::MATHML_STYLE)

MARKUPMAP(msub_, New_HyperText, roles::MATHML_SUB)

MARKUPMAP(msup_, New_HyperText, roles::MATHML_SUP)

MARKUPMAP(msubsup_, New_HyperText, roles::MATHML_SUB_SUP)

MARKUPMAP(munder_, New_HyperText, roles::MATHML_UNDER,
          AttrFromDOM(accentunder_, accentunder_), AttrFromDOM(align, align))

MARKUPMAP(mover_, New_HyperText, roles::MATHML_OVER,
          AttrFromDOM(accent_, accent_), AttrFromDOM(align, align))

MARKUPMAP(munderover_, New_HyperText, roles::MATHML_UNDER_OVER,
          AttrFromDOM(accent_, accent_),
          AttrFromDOM(accentunder_, accentunder_), AttrFromDOM(align, align))

MARKUPMAP(mmultiscripts_, New_HyperText, roles::MATHML_MULTISCRIPTS)

MARKUPMAP(
    mtable_,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLTableAccessible(aElement, aContext->Document());
    },
    roles::MATHML_TABLE, AttrFromDOM(align, align),
    AttrFromDOM(columnlines_, columnlines_), AttrFromDOM(rowlines_, rowlines_))

MARKUPMAP(
    mlabeledtr_,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLTableRowAccessible(aElement, aContext->Document());
    },
    roles::MATHML_LABELED_ROW)

MARKUPMAP(
    mtr_,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLTableRowAccessible(aElement, aContext->Document());
    },
    roles::MATHML_TABLE_ROW)

MARKUPMAP(
    mtd_,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLTableCellAccessible(aElement, aContext->Document());
    },
    roles::MATHML_CELL)

MARKUPMAP(maction_, New_HyperText, roles::MATHML_ACTION,
          AttrFromDOM(actiontype_, actiontype_),
          AttrFromDOM(selection_, selection_))

MARKUPMAP(merror_, New_HyperText, roles::MATHML_ERROR)

MARKUPMAP(mstack_, New_HyperText, roles::MATHML_STACK,
          AttrFromDOM(align, align), AttrFromDOM(position, position))

MARKUPMAP(mlongdiv_, New_HyperText, roles::MATHML_LONG_DIVISION,
          AttrFromDOM(longdivstyle_, longdivstyle_))

MARKUPMAP(msgroup_, New_HyperText, roles::MATHML_STACK_GROUP,
          AttrFromDOM(position, position), AttrFromDOM(shift_, shift_))

MARKUPMAP(msrow_, New_HyperText, roles::MATHML_STACK_ROW,
          AttrFromDOM(position, position))

MARKUPMAP(mscarries_, New_HyperText, roles::MATHML_STACK_CARRIES,
          AttrFromDOM(location_, location_), AttrFromDOM(position, position))

MARKUPMAP(mscarry_, New_HyperText, roles::MATHML_STACK_CARRY,
          AttrFromDOM(crossout_, crossout_))

MARKUPMAP(msline_, New_HyperText, roles::MATHML_STACK_LINE,
          AttrFromDOM(position, position))

MARKUPMAP(nav, New_HyperText, roles::LANDMARK)

MARKUPMAP(
    ol,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::LIST)

MARKUPMAP(
    option,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLSelectOptionAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    optgroup,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLSelectOptGroupAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    output,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLOutputAccessible(aElement, aContext->Document());
    },
    roles::SECTION, Attr(live, polite))

MARKUPMAP(p, nullptr, roles::PARAGRAPH)

MARKUPMAP(
    progress,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLProgressAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(q, New_HyperText, 0)

MARKUPMAP(
    section,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLSectionAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    summary,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLSummaryAccessible(aElement, aContext->Document());
    },
    roles::SUMMARY)

MARKUPMAP(
    table,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      if (aElement->GetPrimaryFrame() &&
          aElement->GetPrimaryFrame()->AccessibleType() != eHTMLTableType) {
        return new ARIAGridAccessibleWrap(aElement, aContext->Document());
      }
      return nullptr;
    },
    0)

MARKUPMAP(time, New_HyperText, 0, Attr(xmlroles, time),
          AttrFromDOM(datetime, datetime))

MARKUPMAP(
    tbody,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // Expose this as a grouping if its frame type is non-standard.
      if (aElement->GetPrimaryFrame() &&
          aElement->GetPrimaryFrame()->IsTableRowGroupFrame()) {
        return nullptr;
      }
      return new HyperTextAccessibleWrap(aElement, aContext->Document());
    },
    roles::GROUPING)

MARKUPMAP(
    td,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      if (aContext->IsTableRow() &&
          aContext->GetContent() == aElement->GetParent()) {
        // If HTML:td element is part of its HTML:table, which has CSS
        // display style other than 'table', then create a generic table
        // cell accessible, because there's no underlying table layout and
        // thus native HTML table cell class doesn't work. The same is
        // true if the cell itself has CSS display:block;.
        if (!aContext->IsHTMLTableRow() ||
            (aElement->GetPrimaryFrame() &&
             aElement->GetPrimaryFrame()->AccessibleType() !=
                 eHTMLTableCellType)) {
          return new ARIAGridCellAccessibleWrap(aElement, aContext->Document());
        }
        if (aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::scope)) {
          return new HTMLTableHeaderCellAccessibleWrap(aElement,
                                                       aContext->Document());
        }
      }
      return nullptr;
    },
    0)

MARKUPMAP(
    tfoot,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // Expose this as a grouping if its frame type is non-standard.
      if (aElement->GetPrimaryFrame() &&
          aElement->GetPrimaryFrame()->IsTableRowGroupFrame()) {
        return nullptr;
      }
      return new HyperTextAccessibleWrap(aElement, aContext->Document());
    },
    roles::GROUPING)

MARKUPMAP(
    th,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      if (aContext->IsTableRow() &&
          aContext->GetContent() == aElement->GetParent()) {
        if (!aContext->IsHTMLTableRow()) {
          return new ARIAGridCellAccessibleWrap(aElement, aContext->Document());
        }
        return new HTMLTableHeaderCellAccessibleWrap(aElement,
                                                     aContext->Document());
      }
      return nullptr;
    },
    0)

MARKUPMAP(
    tfoot,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // Expose this as a grouping if its frame type is non-standard.
      if (aElement->GetPrimaryFrame() &&
          aElement->GetPrimaryFrame()->IsTableRowGroupFrame()) {
        return nullptr;
      }
      return new HyperTextAccessibleWrap(aElement, aContext->Document());
    },
    roles::GROUPING)

MARKUPMAP(
    tr,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      // If HTML:tr element is part of its HTML:table, which has CSS
      // display style other than 'table', then create a generic table row
      // accessible, because there's no underlying table layout and thus
      // native HTML table row class doesn't work. Refer to
      // CreateAccessibleByFrameType dual logic.
      Accessible* table = aContext->IsTable() ? aContext : nullptr;
      if (!table && aContext->Parent() && aContext->Parent()->IsTable()) {
        table = aContext->Parent();
      }
      if (table) {
        nsIContent* parentContent = aElement->GetParent();
        nsIFrame* parentFrame = parentContent->GetPrimaryFrame();
        if (parentFrame && !parentFrame->IsTableWrapperFrame()) {
          parentContent = parentContent->GetParent();
          parentFrame = parentContent->GetPrimaryFrame();
          if (table->GetContent() == parentContent &&
              ((parentFrame && !parentFrame->IsTableWrapperFrame()) ||
               (aElement->GetPrimaryFrame() &&
                aElement->GetPrimaryFrame()->AccessibleType() !=
                    eHTMLTableRowType))) {
            return new ARIARowAccessible(aElement, aContext->Document());
          }
        }
      }
      return nullptr;
    },
    0)

MARKUPMAP(
    ul,
    [](Element* aElement, Accessible* aContext) -> Accessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::LIST)
