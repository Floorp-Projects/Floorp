/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we correctly display appropriate media query titles in the
// property view.

const TEST_URI = URL_ROOT + "doc_media_queries.html";

var {
  PropertyView,
} = require("resource://devtools/client/inspector/computed/computed.js");

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openComputedView();
  await selectNode("div", inspector);
  await checkPropertyView(view);
});

function checkPropertyView(view) {
  const propertyView = new PropertyView(view, "width");
  propertyView.createListItemElement();
  propertyView.matchedExpanded = true;

  return propertyView.refreshMatchedSelectors().then(() => {
    const numMatchedSelectors = propertyView.matchedSelectors.length;

    is(
      numMatchedSelectors,
      2,
      "Property view has the correct number of matched selectors for div"
    );

    is(
      propertyView.hasMatchedSelectors,
      true,
      "hasMatchedSelectors returns true"
    );
  });
}
