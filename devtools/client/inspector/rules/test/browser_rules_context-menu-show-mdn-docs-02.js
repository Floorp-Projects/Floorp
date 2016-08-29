/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the code that integrates the Style Inspector's rule view
 * with the MDN docs tooltip.
 *
 * If you display the context click on a property name in the rule view, you
 * should see a menu item "Show MDN Docs". If you click that item, the MDN
 * docs tooltip should be shown, containing docs from MDN for that property.
 *
 * This file tests that:
 * - clicking the context menu item shows the tooltip
 * - the tooltip content matches the property name for which the context menu was opened
 */

"use strict";

const {setBaseCssDocsUrl} =
  require("devtools/client/shared/widgets/MdnDocsWidget");

const PROPERTYNAME = "color";

const TEST_DOC = `
  <html>
    <body>
      <div style="color: red">
        Test "Show MDN Docs" context menu option
      </div>
    </body>
  </html>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf8," + encodeURIComponent(TEST_DOC));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  setBaseCssDocsUrl(URL_ROOT);

  info("Setting the popupNode for the MDN docs tooltip");

  let {nameSpan} = getRuleViewProperty(view, "element", PROPERTYNAME);

  let allMenuItems = openStyleContextMenuAndGetAllItems(view, nameSpan.firstChild);
  let menuitemShowMdnDocs = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.showMdnDocs"));

  let cssDocs = view.tooltips.cssDocs;

  info("Showing the MDN docs tooltip");
  let onShown = cssDocs.tooltip.once("shown");
  menuitemShowMdnDocs.click();
  yield onShown;
  ok(true, "The MDN docs tooltip was shown");

  info("Quick check that the tooltip contents are set");
  let h1 = cssDocs.tooltip.container.querySelector(".mdn-property-name");
  is(h1.textContent, PROPERTYNAME, "The MDN docs tooltip h1 is correct");
});
