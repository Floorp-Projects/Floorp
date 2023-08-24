/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that properties can be selected and copied from the computed view.

const TEST_URI = `
  <style type="text/css">
    span {
      font-variant-caps: small-caps;
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

  await testCopySome(view);
  await testCopyAll(view);
});

async function testCopySome(view) {
  const expectedPattern =
    "font-family: helvetica, sans-serif;[\\r\\n]+" +
    "font-size: 16px;[\\r\\n]+" +
    "font-variant-caps: small-caps;[\\r\\n]*";

  await copySomeTextAndCheckClipboard(
    view,
    {
      start: { prop: 1, offset: 0 },
      end: { prop: 3, offset: 3 },
    },
    expectedPattern
  );
}

async function testCopyAll(view) {
  const expectedPattern =
    "color: rgb\\(255, 255, 0\\);[\\r\\n]+" +
    "font-family: helvetica, sans-serif;[\\r\\n]+" +
    "font-size: 16px;[\\r\\n]+" +
    "font-variant-caps: small-caps;[\\r\\n]*";

  await copyAllAndCheckClipboard(view, expectedPattern);
}
