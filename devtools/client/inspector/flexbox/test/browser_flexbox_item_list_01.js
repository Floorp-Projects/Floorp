/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  getStr,
} = require("resource://devtools/client/inspector/layout/utils/l10n.js");

// Test the flex item list is empty when there are no flex items for the selected flex
// container.

const TEST_URI = `
  <div id="container" style="display:flex">
  </div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const { getActiveHighlighter } = getHighlighterTestHelpers(inspector);

  const onFlexHeaderRendered = waitForDOM(doc, ".flex-header");
  await selectNode("#container", inspector);
  const [flexHeader] = await onFlexHeaderRendered;
  const flexHighlighterToggle = flexHeader.querySelector(
    "#flexbox-checkbox-toggle"
  );
  const flexItemListHeader = doc.querySelector(".flex-item-list-header");

  info("Checking the state of the Flexbox Inspector.");
  ok(flexHeader, "The flex container header is rendered.");
  ok(flexHighlighterToggle, "The flexbox highlighter toggle is rendered.");
  is(
    flexItemListHeader.textContent,
    getStr("flexbox.noFlexItems"),
    "The flex item list header shows 'No flex items' when there are no items."
  );
  ok(
    !flexHighlighterToggle.checked,
    "The flexbox highlighter toggle is unchecked."
  );
  ok(
    !getActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter is shown."
  );
});
