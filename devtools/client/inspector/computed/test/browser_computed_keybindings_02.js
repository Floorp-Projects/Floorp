/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the computed-view keyboard navigation.

const TEST_URI = `
  <style type="text/css">
    span {
      font-variant: small-caps;
      color: #000000;
    }
    .nomatches {
      color: #ff0000;
    }
  </style>
  <div id="first" style="margin: 10em;
    font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">
    <h1>Some header text</h1>
    <p id="salutation" style="font-size: 12pt">hi.</p>
    <p id="body" style="font-size: 12pt">I am a test-case. This text exists
    solely to provide some things to <span style="color: yellow">
    highlight</span> and <span style="font-weight: bold">count</span>
    style list-items in the box at right. If you are reading this,
    you should go do something else instead. Maybe read a book. Or better
    yet, write some test-cases for another bit of code.
    <span style="font-style: italic">some text</span></p>
    <p id="closing">more text</p>
    <p>even more text</p>
  </div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openComputedView();
  await selectNode("span", inspector);

  info("Selecting the first computed style in the list");
  const firstStyle = view.styleDocument.querySelector(
    "#computed-container .computed-property-view"
  );
  ok(firstStyle, "First computed style found in panel");
  firstStyle.focus();

  info("Tab to select the 2nd style and press return");
  let onExpanded = inspector.once("computed-view-property-expanded");
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onExpanded;

  info("Verify the 2nd style has been expanded");
  const secondStyleSelectors = view.styleDocument.querySelectorAll(
    ".computed-property-view .matchedselectors"
  )[1];
  ok(!!secondStyleSelectors.childNodes.length, "Matched selectors expanded");

  info("Tab back up and test the same thing, with space");
  onExpanded = inspector.once("computed-view-property-expanded");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  EventUtils.synthesizeKey(" ");
  await onExpanded;

  info("Verify the 1st style has been expanded too");
  const firstStyleSelectors = view.styleDocument.querySelectorAll(
    ".computed-property-view .matchedselectors"
  )[0];
  ok(!!firstStyleSelectors.childNodes.length, "Matched selectors expanded");
});
