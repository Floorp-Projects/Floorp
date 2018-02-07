/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XULMAP_TYPE(checkbox, XULCheckboxAccessible)
XULMAP_TYPE(dropMarker, XULDropmarkerAccessible)
XULMAP_TYPE(findbar, XULToolbarAccessible)
XULMAP_TYPE(groupbox, XULGroupboxAccessible)
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
XULMAP_TYPE(tooltip, XULTooltipAccessible)

XULMAP(
  image,
  [](nsIContent* aContent, Accessible* aContext) -> Accessible* {
    if (aContent->IsElement() &&
        aContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::onclick)) {
      return new XULToolbarButtonAccessible(aContent, aContext->Document());
    }

    // Don't include nameless images in accessible tree.
    if (!aContent->IsElement() ||
        !aContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext)) {
      return nullptr;
    }

    return new ImageAccessibleWrap(aContent, aContext->Document());
  }
)

XULMAP(
  listcell,
  [](nsIContent* aContent, Accessible* aContext) -> Accessible* {
    // Only create cells if there's more than one per row.
    nsIContent* listItem = aContent->GetParent();
    if (!listItem) {
      return nullptr;
    }

    for (nsIContent* child = listItem->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsXULElement(nsGkAtoms::listcell) && child != aContent) {
        return new XULListCellAccessibleWrap(aContent, aContext->Document());
      }
    }

    return nullptr;
  }
)

XULMAP(
  tree,
  [](nsIContent* aContent, Accessible* aContext) -> Accessible* {
    nsIContent* child = nsTreeUtils::GetDescendantChild(aContent,
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
      return new XULTreeAccessible(aContent, aContext->Document(), treeFrame);
    }

    // Table or tree table accessible.
    return new XULTreeGridAccessibleWrap(aContent, aContext->Document(), treeFrame);
  }
)
