/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that console API calls in the content page appear in the browser console.

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8>console API calls<script>
  console.log({ contentObject: "YAY!", deep: ["yes!"] });
</script>`;

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  // Enable Fission browser console to see the logged content object
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");

  await addTab(TEST_URI);

  info("Open the Browser Console");
  let hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Wait until the content object is displayed");
  let objectMessage = await waitFor(() =>
    findConsoleAPIMessage(
      hud,
      `Object { contentObject: "YAY!", deep: (1) […] }`
    )
  );
  ok(true, "Content object is displayed in the Browser Console");

  await testExpandObject(objectMessage);

  info("Restart the Browser Console");
  await safeCloseBrowserConsole();
  hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Wait until the content object is displayed");
  objectMessage = await waitFor(() =>
    findConsoleAPIMessage(
      hud,
      `Object { contentObject: "YAY!", deep: (1) […] }`
    )
  );
  ok(true, "Content object is displayed in the Browser Console after restart");

  await testExpandObject(objectMessage);
});

async function testExpandObject(objectMessage) {
  info("Check that the logged content object can be expanded");
  const oi = objectMessage.querySelector(".tree");

  ok(oi, "There's an object inspector component for the content object");

  oi.querySelector(".arrow").click();
  // The object inspector now looks like:
  // ▼ Object { contentObject: "YAY!", deep: (1) […] }
  // |  contentObject: "YAY!"
  // |  ▶︎ deep: Array [ "yes!" ]
  // |  ▶︎ <prototype>
  await waitFor(() => oi.querySelectorAll(".node").length === 4);
  ok(true, "The ObjectInspector was expanded");
  const [root, contentObjectProp, deepProp, prototypeProp] = [
    ...oi.querySelectorAll(".node"),
  ];

  ok(
    root.textContent.includes('Object { contentObject: "YAY!", deep: (1) […] }')
  );
  ok(contentObjectProp.textContent.includes(`contentObject: "YAY!"`));
  ok(deepProp.textContent.includes(`deep: Array [ "yes!" ]`));
  ok(prototypeProp.textContent.includes(`<prototype>`));

  // The object inspector now looks like:
  // ▼ Object { contentObject: "YAY!", deep: (1) […] }
  // |  contentObject: "YAY!"
  // |  ▼︎ deep: (1) […]
  // |  |  0: "yes!"
  // |  |  length: 1
  // |  |  ▶︎ <prototype>
  // |  ▶︎ <prototype>
  deepProp.querySelector(".arrow").click();
  await waitFor(() => oi.querySelectorAll(".node").length === 7);
  ok(true, "The nested array was expanded");
}
