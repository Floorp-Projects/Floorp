/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XULMAP_TYPE(browser, OuterDocAccessible)
XULMAP_TYPE(button, XULButtonAccessible)
XULMAP_TYPE(checkbox, XULCheckboxAccessible)
XULMAP_TYPE(description, XULLabelAccessible)
XULMAP_TYPE(dropMarker, XULDropmarkerAccessible)
XULMAP_TYPE(editor, OuterDocAccessible)
XULMAP_TYPE(findbar, XULToolbarAccessible)
XULMAP_TYPE(groupbox, XULGroupboxAccessible)
XULMAP_TYPE(iframe, OuterDocAccessible)
XULMAP_TYPE(listbox, XULListboxAccessibleWrap)
XULMAP_TYPE(listhead, XULColumAccessible)
XULMAP_TYPE(listheader, XULColumnItemAccessible)
XULMAP_TYPE(listitem, XULListitemAccessible)
XULMAP_TYPE(menu, XULMenuitemAccessibleWrap)
XULMAP_TYPE(menubar, XULMenubarAccessible)
XULMAP_TYPE(menucaption, XULMenuitemAccessibleWrap)
XULMAP_TYPE(menuitem, XULMenuitemAccessibleWrap)
XULMAP_TYPE(menulist, XULComboboxAccessible)
XULMAP_TYPE(menuseparator, XULMenuSeparatorAccessible)
XULMAP_TYPE(notification, XULAlertAccessible)
XULMAP_TYPE(progressmeter, XULProgressMeterAccessible)
XULMAP_TYPE(radio, XULRadioButtonAccessible)
XULMAP_TYPE(radiogroup, XULRadioGroupAccessible)
XULMAP_TYPE(richlistbox, XULListboxAccessibleWrap)
XULMAP_TYPE(richlistitem, XULListitemAccessible)
XULMAP_TYPE(scale, XULSliderAccessible)
XULMAP_TYPE(statusbar, XULStatusBarAccessible)
XULMAP_TYPE(tab, XULTabAccessible)
XULMAP_TYPE(tabpanels, XULTabpanelsAccessible)
XULMAP_TYPE(tabs, XULTabsAccessible)
XULMAP_TYPE(toolbarseparator, XULToolbarSeparatorAccessible)
XULMAP_TYPE(toolbarspacer, XULToolbarSeparatorAccessible)
XULMAP_TYPE(toolbarspring, XULToolbarSeparatorAccessible)
XULMAP_TYPE(treecol, XULColumnItemAccessible)
XULMAP_TYPE(treecolpicker, XULButtonAccessible)
XULMAP_TYPE(treecols, XULTreeColumAccessible)
XULMAP_TYPE(toolbar, XULToolbarAccessible)
XULMAP_TYPE(toolbarbutton, XULToolbarButtonAccessible)
XULMAP_TYPE(tooltip, XULTooltipAccessible)

XULMAP(
  colorpicker,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                              nsGkAtoms::button, eIgnoreCase)) {
      return new XULColorPickerAccessible(aElement, aContext->Document());
    }
    return nullptr;
  }
)

XULMAP(
  label,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    if (aElement->ClassList()->Contains(NS_LITERAL_STRING("text-link"))) {
      return new XULLinkAccessible(aElement, aContext->Document());
    }
    return new XULLabelAccessible(aElement, aContext->Document());
  }
)

XULMAP(
  image,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    if (aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::onclick)) {
      return new XULToolbarButtonAccessible(aElement, aContext->Document());
    }

    if (aElement->ClassList()->Contains(NS_LITERAL_STRING("colorpickertile"))) {
      return new XULColorPickerTileAccessible(aElement, aContext->Document());
    }

    // Don't include nameless images in accessible tree.
    if (!aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext)) {
      return nullptr;
    }

    return new ImageAccessibleWrap(aElement, aContext->Document());
  }
)

XULMAP(
  listcell,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    // Only create cells if there's more than one per row.
    nsIContent* listItem = aElement->GetParent();
    if (!listItem) {
      return nullptr;
    }

    for (nsIContent* child = listItem->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsXULElement(nsGkAtoms::listcell) && child != aElement) {
        return new XULListCellAccessibleWrap(aElement, aContext->Document());
      }
    }

    return nullptr;
  }
)

XULMAP(
  menupopup,
  [](Element* aElement, Accessible* aContext) {
    return CreateMenupopupAccessible(aElement, aContext);
  }
)

XULMAP(
  panel,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    static const Element::AttrValuesArray sIgnoreTypeVals[] =
      { &nsGkAtoms::autocomplete_richlistbox, &nsGkAtoms::autocomplete, nullptr };

    if (aElement->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                  sIgnoreTypeVals, eIgnoreCase) >= 0) {
      return nullptr;
    }

    if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautofocus,
                              nsGkAtoms::_true, eCaseMatters)) {
      return new XULAlertAccessible(aElement, aContext->Document());
    }

    return new EnumRoleAccessible<roles::PANE>(aElement, aContext->Document());
  }
)

XULMAP(
  popup,
  [](Element* aElement, Accessible* aContext) {
    return CreateMenupopupAccessible(aElement, aContext);
  }
)

XULMAP(
  textbox,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                              nsGkAtoms::autocomplete, eIgnoreCase)) {
      return new XULComboboxAccessible(aElement, aContext->Document());
    }

    return new EnumRoleAccessible<roles::SECTION>(aElement, aContext->Document());
  }
)

XULMAP(
  thumb,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    if (aElement->ClassList()->Contains(NS_LITERAL_STRING("scale-thumb"))) {
      return new XULThumbAccessible(aElement, aContext->Document());
    }
    return nullptr;
  }
)

XULMAP(
  tree,
  [](Element* aElement, Accessible* aContext) -> Accessible* {
    nsIContent* child = nsTreeUtils::GetDescendantChild(aElement,
                                                        nsGkAtoms::treechildren);
    if (!child)
      return nullptr;

    nsTreeBodyFrame* treeFrame = do_QueryFrame(child->GetPrimaryFrame());
    if (!treeFrame)
      return nullptr;

    RefPtr<nsTreeColumns> treeCols = treeFrame->Columns();
    int32_t count = 0;
    treeCols->GetCount(&count);

    // Outline of list accessible.
    if (count == 1) {
      return new XULTreeAccessible(aElement, aContext->Document(), treeFrame);
    }

    // Table or tree table accessible.
    return new XULTreeGridAccessibleWrap(aElement, aContext->Document(), treeFrame);
  }
)
