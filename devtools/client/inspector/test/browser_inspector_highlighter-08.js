/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that performing multiple requests to highlight nodes or to hide the highlighter,
// without waiting for the former ones to complete, still works well.
add_task(async function () {
  info("Loading the test document and opening the inspector");
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    "data:text/html,"
  );
  const html = await getNodeFront("html", inspector);
  const body = await getNodeFront("body", inspector);
  const type = inspector.highlighters.TYPES.BOXMODEL;
  const getActiveHighlighter = () => {
    return inspector.highlighters.getActiveHighlighter(type);
  };
  is(getActiveHighlighter(), null, "The highlighter is hidden by default");

  info("Highlight <body>, and hide the highlighter immediately after");
  await Promise.all([
    inspector.highlighters.showHighlighterTypeForNode(type, body),
    inspector.highlighters.hideHighlighterType(type),
  ]);
  is(getActiveHighlighter(), null, "The highlighter is hidden");

  info("Highlight <body>, then <html>, then <body> again, synchronously");
  await Promise.all([
    inspector.highlighters.showHighlighterTypeForNode(type, body),
    inspector.highlighters.showHighlighterTypeForNode(type, html),
    inspector.highlighters.showHighlighterTypeForNode(type, body),
  ]);
  ok(
    await highlighterTestFront.assertHighlightedNode("body"),
    "The highlighter highlights <body>"
  );

  info("Highlight <html>, then <body>, then <html> again, synchronously");
  await Promise.all([
    inspector.highlighters.showHighlighterTypeForNode(type, html),
    inspector.highlighters.showHighlighterTypeForNode(type, body),
    inspector.highlighters.showHighlighterTypeForNode(type, html),
  ]);
  ok(
    await highlighterTestFront.assertHighlightedNode("html"),
    "The highlighter highlights <html>"
  );

  info("Hide the highlighter, and highlight <html> immediately after");
  await Promise.all([
    inspector.highlighters.hideHighlighterType(type),
    inspector.highlighters.showHighlighterTypeForNode(type, body),
  ]);
  ok(
    await highlighterTestFront.assertHighlightedNode("body"),
    "The highlighter highlights <body>"
  );

  info("Highlight <html>, and hide the highlighter immediately after");
  await Promise.all([
    inspector.highlighters.showHighlighterTypeForNode(type, html),
    inspector.highlighters.hideHighlighterType(type),
  ]);
  is(getActiveHighlighter(), null, "The highlighter is hidden");
});
