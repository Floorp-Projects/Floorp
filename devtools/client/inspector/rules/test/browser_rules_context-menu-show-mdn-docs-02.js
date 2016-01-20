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
 * - pressing "Escape" while the tooltip is showing hides the tooltip
 */

"use strict";

const {setBaseCssDocsUrl} = require("devtools/client/shared/widgets/MdnDocsWidget");

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
  yield testShowAndHideMdnTooltip(view);
});

function* testShowMdnTooltip(view) {
  setBaseCssDocsUrl(URL_ROOT);

  info("Setting the popupNode for the MDN docs tooltip");

  let {nameSpan} = getRuleViewProperty(view, "element", PROPERTYNAME);

  view.styleDocument.popupNode = nameSpan.firstChild;
  view._contextmenu._updateMenuItems();

  let cssDocs = view.tooltips.cssDocs;

  info("Showing the MDN docs tooltip");
  let onShown = cssDocs.tooltip.once("shown");
  view._contextmenu.menuitemShowMdnDocs.click();
  yield onShown;
  ok(true, "The MDN docs tooltip was shown");
}

/**
 * Test that:
 *  - the MDN tooltip is shown when we click the context menu item
 *  - the tooltip's contents have been initialized (we don't fully
 *  test this here, as it's fully tested with the tooltip test code)
 *  - the tooltip is hidden when we press Escape
 */
function* testShowAndHideMdnTooltip(view) {
  yield testShowMdnTooltip(view);

  info("Quick check that the tooltip contents are set");
  let cssDocs = view.tooltips.cssDocs;

  let tooltipDocument = cssDocs.tooltip.content.contentDocument;
  let h1 = tooltipDocument.getElementById("property-name");
  is(h1.textContent, PROPERTYNAME, "The MDN docs tooltip h1 is correct");

  info("Simulate pressing the 'Escape' key");
  let onHidden = cssDocs.tooltip.once("hidden");
  EventUtils.sendKey("escape");
  yield onHidden;
  ok(true, "The MDN docs tooltip was hidden on pressing 'escape'");
}

/**
 * Returns the root element for the rule view.
 */
var rootElement = view => (view.element) ? view.element : view.styleDocument;
