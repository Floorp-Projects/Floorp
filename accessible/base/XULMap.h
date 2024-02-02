/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XULMAP_TYPE(browser, OuterDocAccessible)
XULMAP_TYPE(button, XULButtonAccessible)
XULMAP_TYPE(checkbox, CheckboxAccessible)
XULMAP_TYPE(dropMarker, XULDropmarkerAccessible)
XULMAP_TYPE(editor, OuterDocAccessible)
XULMAP_TYPE(findbar, XULToolbarAccessible)
XULMAP_TYPE(groupbox, XULGroupboxAccessible)
XULMAP_TYPE(iframe, OuterDocAccessible)
XULMAP_TYPE(listheader, XULColumAccessible)
XULMAP_TYPE(menu, XULMenuitemAccessible)
XULMAP_TYPE(menubar, XULMenubarAccessible)
XULMAP_TYPE(menucaption, XULMenuitemAccessible)
XULMAP_TYPE(menuitem, XULMenuitemAccessible)
XULMAP_TYPE(menulist, XULComboboxAccessible)
XULMAP_TYPE(menuseparator, XULMenuSeparatorAccessible)
XULMAP_TYPE(notification, XULAlertAccessible)
XULMAP_TYPE(radio, XULRadioButtonAccessible)
XULMAP_TYPE(radiogroup, XULRadioGroupAccessible)
XULMAP_TYPE(richlistbox, XULListboxAccessible)
XULMAP_TYPE(richlistitem, XULListitemAccessible)
XULMAP_TYPE(statusbar, XULStatusBarAccessible)
XULMAP_TYPE(tab, XULTabAccessible)
XULMAP_TYPE(tabpanels, XULTabpanelsAccessible)
XULMAP_TYPE(tabs, XULTabsAccessible)
XULMAP_TYPE(toolbarseparator, XULToolbarSeparatorAccessible)
XULMAP_TYPE(toolbarspacer, XULToolbarSeparatorAccessible)
XULMAP_TYPE(toolbarspring, XULToolbarSeparatorAccessible)
XULMAP_TYPE(treecol, XULColumnItemAccessible)
XULMAP_TYPE(treecols, XULTreeColumAccessible)
XULMAP_TYPE(toolbar, XULToolbarAccessible)
XULMAP_TYPE(toolbarbutton, XULToolbarButtonAccessible)

XULMAP(description,
       [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
         if (aElement->ClassList()->Contains(u"tooltip-label"_ns)) {
           // FIXME(emilio): Why this special case?
           return nullptr;
         }

         return new XULLabelAccessible(aElement, aContext->Document());
       })

XULMAP(tooltip,
       [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
         return new XULTooltipAccessible(aElement, aContext->Document());
       })

XULMAP(label,
       [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
         if (aElement->ClassList()->Contains(u"text-link"_ns)) {
           return new XULLinkAccessible(aElement, aContext->Document());
         }
         return new XULLabelAccessible(aElement, aContext->Document());
       })

XULMAP(image,
       [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
         // Don't include nameless images in accessible tree.
         if (!aElement->HasAttr(nsGkAtoms::tooltiptext)) {
           return nullptr;
         }

         return new ImageAccessible(aElement, aContext->Document());
       })

XULMAP(menupopup, [](Element* aElement, LocalAccessible* aContext) {
  return CreateMenupopupAccessible(aElement, aContext);
})

XULMAP(panel,
       [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
         static const Element::AttrValuesArray sIgnoreTypeVals[] = {
             nsGkAtoms::autocomplete_richlistbox, nsGkAtoms::autocomplete,
             nullptr};

         if (aElement->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                       sIgnoreTypeVals, eIgnoreCase) >= 0) {
           return nullptr;
         }

         if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautofocus,
                                   nsGkAtoms::_true, eCaseMatters)) {
           return new XULAlertAccessible(aElement, aContext->Document());
         }

         return new EnumRoleAccessible<roles::PANE>(aElement,
                                                    aContext->Document());
       })

XULMAP(tree,
       [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
         nsIContent* child =
             nsTreeUtils::GetDescendantChild(aElement, nsGkAtoms::treechildren);
         if (!child) return nullptr;

         nsTreeBodyFrame* treeFrame = do_QueryFrame(child->GetPrimaryFrame());
         if (!treeFrame) return nullptr;

         RefPtr<nsTreeColumns> treeCols = treeFrame->Columns();
         uint32_t count = treeCols->Count();

         // Outline of list accessible.
         if (count == 1) {
           return new XULTreeAccessible(aElement, aContext->Document(),
                                        treeFrame);
         }

         // Table or tree table accessible.
         return new XULTreeGridAccessible(aElement, aContext->Document(),
                                          treeFrame);
       })
