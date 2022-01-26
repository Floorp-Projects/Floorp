/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct when the page defines layers.

const TEST_URI = `
  <style type="text/css">
    @import url(${URL_ROOT_COM_SSL}doc_imported_anonymous_layer.css) layer;
    @import url(${URL_ROOT_COM_SSL}doc_imported_named_layer.css) layer(importedLayer);

    @layer myLayer {
      h1 {
        background-color: tomato;
        color: lightgreen;
      }
    }

    @layer {
      h1 {
        color: green;
        font-variant: small-caps
      }
    }
  </style>
  <h1>Hello @layer!</h1>
`;

add_task(async function() {
  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();

  await selectNode("h1", inspector);

  is(
    getRuleViewParentDataTextByIndex(view, 1),
    "@layer",
    "text at index 1 contains anonymous layer."
  );

  is(
    getRuleViewParentDataTextByIndex(view, 2),
    "@layer myLayer",
    "text at index 2 contains named layer."
  );

  is(
    getRuleViewParentDataTextByIndex(view, 3),
    "@layer importedLayer @media screen",
    "text at index 3 contains imported stylesheet named layer."
  );

  // XXX: This is highlighting an issue with nested layers/media queries, where only the
  // last item is displayed. This should be fixed in Bug 1751417, and this test case should
  // then have `@layer importedLayer @media screen @layer in-imported-stylesheet`
  is(
    getRuleViewParentDataTextByIndex(view, 4),
    "@layer in-imported-stylesheet",
    "text at index 4 only contains the last layer it's in."
  );

  is(
    getRuleViewParentDataTextByIndex(view, 5),
    "@layer",
    "text at index 5 contains imported stylesheet anonymous layer."
  );
});

function getRuleViewParentDataElementByIndex(view, ruleIndex) {
  return view.styleDocument.querySelector(
    `.ruleview-rule:nth-of-type(${ruleIndex + 1}) .ruleview-rule-parent-data`
  );
}

function getRuleViewParentDataTextByIndex(view, ruleIndex) {
  return getRuleViewParentDataElementByIndex(view, ruleIndex)?.textContent;
}
