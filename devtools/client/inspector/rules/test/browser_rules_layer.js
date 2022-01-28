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

    h1 {
      color: pink;
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
    getRuleViewAncestorRulesDataElementByIndex(view, 1),
    null,
    "first rule displayed is the one without @layer"
  );

  is(
    getRuleViewAncestorRulesDataTextByIndex(view, 2),
    "@layer",
    "text at index 1 contains anonymous layer."
  );

  is(
    getRuleViewAncestorRulesDataTextByIndex(view, 3),
    "@layer myLayer",
    "text at index 2 contains named layer."
  );

  is(
    getRuleViewAncestorRulesDataTextByIndex(view, 4),
    ["@layer importedLayer", "@media screen"].join("\n"),
    "text at index 3 contains imported stylesheet named layer and media query information."
  );

  is(
    getRuleViewAncestorRulesDataTextByIndex(view, 5),
    [
      "@layer importedLayer",
      "@media screen",
      "@layer in-imported-stylesheet",
    ].join("\n"),
    "text at index 4 contains all the layers and media queries it's nested in"
  );

  is(
    getRuleViewAncestorRulesDataTextByIndex(view, 6),
    "@layer",
    "text at index 5 contains imported stylesheet anonymous layer."
  );
});
