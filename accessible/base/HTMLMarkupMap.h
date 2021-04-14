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

MARKUPMAP(
    menu,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLListAccessible(aElement, aContext->Document());
    },
    roles::LIST)

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
          // parentContent can be null if this tr is at the top level of a
          // shadow root (with the table outside the shadow root).
          parentFrame =
              parentContent ? parentContent->GetPrimaryFrame() : nullptr;
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
