/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the CssDocs tooltip of the ruleview can be closed when pressing the Escape
 * key.
 */

"use strict";

const {setBaseCssDocsUrl} =
  require("devtools/client/shared/widgets/MdnDocsWidget");

const PROPERTYNAME = "color";

const TEST_URI = `
  <html>
    <body>
      <div style="color: red">
        Test "Show MDN Docs" closes on escape
      </div>
    </body>
  </html>
`;

/**
 * Test that the tooltip is hidden when we press Escape
 */
add_task(function* () {
  yield addTab("data:text/html;charset=utf8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  setBaseCssDocsUrl(URL_ROOT);

  info("Retrieve a valid anchor for the CssDocs tooltip");
  let {nameSpan} = getRuleViewProperty(view, "element", PROPERTYNAME);

  info("Showing the MDN docs tooltip");
  let onShown = view.tooltips.cssDocs.tooltip.once("shown");
  view.tooltips.cssDocs.show(nameSpan, PROPERTYNAME);
  yield onShown;
  ok(true, "The MDN docs tooltip was shown");

  info("Simulate pressing the 'Escape' key");
  let onHidden = view.tooltips.cssDocs.tooltip.once("hidden");
  EventUtils.sendKey("escape");
  yield onHidden;
  ok(true, "The MDN docs tooltip was hidden on pressing 'escape'");
});
