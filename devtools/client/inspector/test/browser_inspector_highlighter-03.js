/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that iframes are correctly highlighted.

const IFRAME_SRC = `<style>
    body {
      margin:0;
      height:100%;
      background-color:tomato;
    }
  </style>
  <body class=remote>hello from iframe</body>`;

const DOCUMENT_SRC = `<style>
    iframe {
      height:200px;
      border: 11px solid black;
      padding: 13px;
    }
    body,iframe,h1 {
      margin:0;
      padding: 0;
    }
  </style>
  <body>
    <iframe src='https://example.com/document-builder.sjs?html=${encodeURIComponent(
      IFRAME_SRC
    )}'></iframe>
  </body>`;

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

add_task(async function () {
  const { inspector, toolbox, highlighterTestFront } =
    await openInspectorForURL(TEST_URI);

  info("Waiting for box mode to show.");
  const topLevelBodyNodeFront = await getNodeFront("body", inspector);
  await inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    topLevelBodyNodeFront
  );

  info("Waiting for element picker to become active.");
  await startPicker(toolbox);

  info("Check that hovering iframe padding does highlight the iframe element");
  // the iframe has 13px of padding, so hovering at [1,1] should be enough.
  await hoverElement(inspector, "iframe", 1, 1);

  await isNodeCorrectlyHighlighted(highlighterTestFront, "iframe");

  info("Scrolling the document");
  await setContentPageElementProperty(
    "iframe",
    "style",
    "margin-bottom: 2000px"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.scrollBy(0, 40)
  );

  // target the body within the iframe
  const iframeBodySelector = ["iframe", "body"];
  let iframeHighlighterTestFront = highlighterTestFront;
  let bodySelectorWithinHighlighterEnv = iframeBodySelector;

  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    const target = toolbox.commands.targetCommand
      .getAllTargets([toolbox.commands.targetCommand.TYPES.FRAME])
      .find(t => t.url.startsWith("https://example.com"));

    // We need to retrieve the highlighterTestFront for the frame target.
    iframeHighlighterTestFront = await getHighlighterTestFront(toolbox, {
      target,
    });

    bodySelectorWithinHighlighterEnv = ["body"];
  }

  info("Check that hovering the iframe <body> highlights the expected element");
  await hoverElement(inspector, iframeBodySelector, 40, 40);

  ok(
    await iframeHighlighterTestFront.assertHighlightedNode(
      bodySelectorWithinHighlighterEnv
    ),
    "highlighter is shown on the iframe body"
  );
  await isNodeCorrectlyHighlighted(
    iframeHighlighterTestFront,
    iframeBodySelector
  );

  info("Scrolling the document up");
  // scroll up so we can inspect the top level document again
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.scrollTo(0, 0)
  );

  info("Check that hovering iframe padding again does work");
  // the iframe has 13px of padding, so hovering at [1,1] should be enough.
  await hoverElement(inspector, "iframe", 1, 1);
  await isNodeCorrectlyHighlighted(highlighterTestFront, "iframe");

  info("And finally check that hovering the iframe <body> again does work");
  info("Check that hovering the iframe <body> highlights the expected element");
  await hoverElement(inspector, iframeBodySelector, 40, 40);

  ok(
    await iframeHighlighterTestFront.assertHighlightedNode(
      bodySelectorWithinHighlighterEnv
    ),
    "highlighter is shown on the iframe body"
  );
  await isNodeCorrectlyHighlighted(
    iframeHighlighterTestFront,
    iframeBodySelector
  );

  info("Stop the element picker.");
  await toolbox.nodePicker.stop({ canceled: true });
});
