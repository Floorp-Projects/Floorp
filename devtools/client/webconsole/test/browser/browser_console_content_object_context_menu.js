/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that "Copy Object" on a the content message works in the browser console.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>console API calls<script>
  console.log({
    contentObject: "YAY!",
    deep: ["hello", "world"]
  });
</script>`;

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  // Enable Fission browser console to see the logged content object
  await pushPref("devtools.browsertoolbox.fission", true);

  await addTab(TEST_URI);

  info("Open the Browser Console");
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Wait until the content object is displayed");
  const objectMessage = await waitFor(() =>
    findMessage(hud, `Object { contentObject: "YAY!", deep: (2) […] }`)
  );
  ok(true, "Content object is displayed in the Browser Console");

  info("Expand the object");
  const oi = objectMessage.querySelector(".tree");
  oi.querySelector(".arrow").click();
  // The object inspector now looks like:
  // ▼ Object { contentObject: "YAY!", deep: (1) […] }
  // |  contentObject: "YAY!"
  // |  ▶︎ deep: Array [ "hello", "world" ]
  // |  ▶︎ <prototype>

  await waitFor(() => oi.querySelectorAll(".node").length === 4);
  ok(true, "The ObjectInspector was expanded");
  oi.scrollIntoView();

  info("Check that the object can be copied to clipboard");
  await testCopyObject(
    hud,
    oi.querySelector(".objectBox-object"),
    JSON.stringify({ contentObject: "YAY!", deep: ["hello", "world"] }, null, 2)
  );

  info("Check that inner object can be copied to clipboard");
  await testCopyObject(
    hud,
    oi.querySelectorAll(".node")[2].querySelector(".objectBox-array"),
    JSON.stringify(["hello", "world"], null, 2)
  );
});

async function testCopyObject(hud, element, expected) {
  info("Check `Copy object` is enabled");
  const menuPopup = await openContextMenu(hud, element);
  const copyObjectMenuItem = menuPopup.querySelector(
    "#console-menu-copy-object"
  );
  ok(!copyObjectMenuItem.disabled, "`Copy object` is enabled");

  info("Click on `Copy object`");
  await waitForClipboardPromise(() => copyObjectMenuItem.click(), expected);
  await hideContextMenu(hud);
}
