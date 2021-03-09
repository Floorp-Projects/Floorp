/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

MARKUPMAP(
    a,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
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
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLButtonAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    caption,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
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

// XXX: Uncomment this once HTML-aam agrees to map to same as ARIA.
// MARKUPMAP(code, New_HyperText, roles::CODE)

MARKUPMAP(dd, New_HTMLDtOrDd<HyperTextAccessibleWrap>, roles::DEFINITION)

MARKUPMAP(del, New_HyperText, roles::CONTENT_DELETION)

MARKUPMAP(details, New_HyperText, roles::DETAILS)

MARKUPMAP(dialog, New_HyperText, roles::DIALOG)

MARKUPMAP(
    div,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      // Never create an accessible if we're part of an anonymous
      // subtree.
      if (aElement->IsInNativeAnonymousSubtree()) {
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
      if (displayValue != u"block"_ns && displayValue != u"inline-block"_ns) {
        return nullptr;
      }
      // Check for various conditions to determine if this is a block
      // break and needs to be rendered.
      // If its previous sibling is an inline element, we probably want
      // to break, so render.
      nsIContent* prevSibling = aElement->GetPreviousSibling();
      if (prevSibling) {
        nsIFrame* prevSiblingFrame = prevSibling->GetPrimaryFrame();
        if (prevSiblingFrame && prevSiblingFrame->IsInlineOutside()) {
          return new HyperTextAccessibleWrap(aElement, aContext->Document());
        }
      }
      // Now, check the children.
      nsIContent* firstChild = aElement->GetFirstChild();
      if (firstChild) {
        nsIFrame* firstChildFrame = firstChild->GetPrimaryFrame();
        if (!firstChildFrame) {
          // The first child is invisible, but this might be due to an
          // invisible text node. Try the next.
          firstChild = firstChild->GetNextSibling();
          if (!firstChild) {
            // If there's no next sibling, there's only one child, so there's
            // nothing more we can do.
            return nullptr;
          }
          firstChildFrame = firstChild->GetPrimaryFrame();
        }
        // Check to see if first child has an inline frame.
        if (firstChildFrame && firstChildFrame->IsInlineOutside()) {
          return new HyperTextAccessibleWrap(aElement, aContext->Document());
        }
        nsIContent* lastChild = aElement->GetLastChild();
        MOZ_ASSERT(lastChild);
        if (lastChild != firstChild) {
          nsIFrame* lastChildFrame = lastChild->GetPrimaryFrame();
          if (!lastChildFrame) {
            // The last child is invisible, but this might be due to an
            // invisible text node. Try the next.
            lastChild = lastChild->GetPreviousSibling();
            MOZ_ASSERT(lastChild);
            if (lastChild == firstChild) {
              return nullptr;
            }
            lastChildFrame = lastChild->GetPrimaryFrame();
          }
          // Check to see if last child has an inline frame.
          if (lastChildFrame && lastChildFrame->IsInlineOutside()) {
            return new HyperTextAccessibleWrap(aElement, aContext->Document());
          }
        }
      }
      return nullptr;
    },
    roles::SECTION)

MARKUPMAP(
    dl,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::DEFINITION_LIST)

MARKUPMAP(dt, New_HTMLDtOrDd<HTMLLIAccessible>, roles::TERM)

MARKUPMAP(
    figcaption,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLFigcaptionAccessible(aElement, aContext->Document());
    },
    roles::CAPTION)

MARKUPMAP(
    figure,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLFigureAccessible(aElement, aContext->Document());
    },
    roles::FIGURE, Attr(xmlroles, figure))

MARKUPMAP(
    fieldset,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLGroupboxAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    form,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLFormAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    footer,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLHeaderOrFooterAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    header,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
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
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLHRAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    input,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
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
        return new HTMLDateTimeAccessible<roles::TIME_EDITOR>(
            aElement, aContext->Document());
      }
      if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                nsGkAtoms::date, eIgnoreCase)) {
        return new HTMLDateTimeAccessible<roles::DATE_EDITOR>(
            aElement, aContext->Document());
      }
      return nullptr;
    },
    0)

MARKUPMAP(ins, New_HyperText, roles::CONTENT_INSERTION)

MARKUPMAP(
    label,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLLabelAccessible(aElement, aContext->Document());
    },
    roles::LABEL)

MARKUPMAP(
    legend,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLLegendAccessible(aElement, aContext->Document());
    },
    roles::LABEL)

MARKUPMAP(
    li,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
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

MARKUPMAP(mark, New_HyperText, roles::MARK, Attr(xmlroles, mark))

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
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableAccessible(aElement, aContext->Document());
    },
    roles::MATHML_TABLE, AttrFromDOM(align, align),
    AttrFromDOM(columnlines_, columnlines_), AttrFromDOM(rowlines_, rowlines_))

MARKUPMAP(
    mlabeledtr_,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableRowAccessible(aElement, aContext->Document());
    },
    roles::MATHML_LABELED_ROW)

MARKUPMAP(
    mtr_,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableRowAccessible(aElement, aContext->Document());
    },
    roles::MATHML_TABLE_ROW)

MARKUPMAP(
    mtd_,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableCellAccessible(aElement, aContext->Document());
    },
    roles::MATHML_CELL)

MARKUPMAP(maction_, New_HyperText, roles::MATHML_ACTION,
          AttrFromDOM(actiontype_, actiontype_),
          AttrFromDOM(selection_, selection_))

MARKUPMAP(
    menu,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::LIST)

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
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::LIST)

MARKUPMAP(
    option,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLSelectOptionAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    optgroup,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLSelectOptGroupAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    output,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLOutputAccessible(aElement, aContext->Document());
    },
    roles::STATUSBAR, Attr(live, polite))

MARKUPMAP(p, nullptr, roles::PARAGRAPH)

MARKUPMAP(
    progress,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLProgressAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(q, New_HyperText, 0)

MARKUPMAP(
    section,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLSectionAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(
    summary,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLSummaryAccessible(aElement, aContext->Document());
    },
    roles::SUMMARY)

MARKUPMAP(
    table,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      if (!aElement->GetPrimaryFrame() ||
          aElement->GetPrimaryFrame()->AccessibleType() != eHTMLTableType) {
        return new ARIAGridAccessibleWrap(aElement, aContext->Document());
      }

      // Make sure that our children are proper layout table parts
      for (nsIContent* child = aElement->GetFirstChild(); child;
           child = child->GetNextSibling()) {
        if (child->IsAnyOfHTMLElements(nsGkAtoms::thead, nsGkAtoms::tfoot,
                                       nsGkAtoms::tbody, nsGkAtoms::tr)) {
          // These children elements need to participate in the layout table
          // and need table row(group) frames.
          nsIFrame* childFrame = child->GetPrimaryFrame();
          if (childFrame && (!childFrame->IsTableRowGroupFrame() &&
                             !childFrame->IsTableRowFrame())) {
            return new ARIAGridAccessibleWrap(aElement, aContext->Document());
          }
        }
      }
      return nullptr;
    },
    0)

MARKUPMAP(time, New_HyperText, 0, Attr(xmlroles, time),
          AttrFromDOM(datetime, datetime))

MARKUPMAP(tbody, nullptr, roles::GROUPING)

MARKUPMAP(
    td,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      if (aContext->IsTableRow() &&
          aContext->GetContent() == aElement->GetParent()) {
        // If HTML:td element is part of its HTML:table, which has CSS
        // display style other than 'table', then create a generic table
        // cell accessible, because there's no underlying table layout and
        // thus native HTML table cell class doesn't work. The same is
        // true if the cell itself has CSS display:block;.
        if (!aContext->IsHTMLTableRow() || !aElement->GetPrimaryFrame() ||
            aElement->GetPrimaryFrame()->AccessibleType() !=
                eHTMLTableCellType) {
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

MARKUPMAP(tfoot, nullptr, roles::GROUPING)

MARKUPMAP(
    th,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
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

MARKUPMAP(thead, nullptr, roles::GROUPING)

MARKUPMAP(
    tr,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      // If HTML:tr element is part of its HTML:table, which has CSS
      // display style other than 'table', then create a generic table row
      // accessible, because there's no underlying table layout and thus
      // native HTML table row class doesn't work. Refer to
      // CreateAccessibleByFrameType dual logic.
      LocalAccessible* table = aContext->IsTable() ? aContext : nullptr;
      if (!table && aContext->LocalParent() &&
          aContext->LocalParent()->IsTable()) {
        table = aContext->LocalParent();
      }
      if (table) {
        nsIContent* parentContent = aElement->GetParent();
        nsIFrame* parentFrame = parentContent->GetPrimaryFrame();
        if (!parentFrame || !parentFrame->IsTableWrapperFrame()) {
          parentContent = parentContent->GetParent();
          parentFrame = parentContent->GetPrimaryFrame();
          if (table->GetContent() == parentContent &&
              ((!parentFrame || !parentFrame->IsTableWrapperFrame()) ||
               !aElement->GetPrimaryFrame() ||
               aElement->GetPrimaryFrame()->AccessibleType() !=
                   eHTMLTableRowType)) {
            return new ARIARowAccessible(aElement, aContext->Document());
          }
        }
      }
      return nullptr;
    },
    0)

MARKUPMAP(
    ul,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::LIST)

MARKUPMAP(
    meter,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLMeterAccessible(aElement, aContext->Document());
    },
    roles::METER)
