/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the ObjectInspector is rendered correctly in the sidebar.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<script>
    console.log({
      a:1,
      b:2,
      c:[3]
    });
  </script>`;

add_task(async function() {
  // Should be removed when sidebar work is complete
  await pushPref("devtools.webconsole.sidebarToggle", true);
  const isMacOS = Services.appinfo.OS === "Darwin";

  const hud = await openNewTabAndConsole(TEST_URI);

  const message = findMessage(hud, "Object");
  const object = message.querySelector(".object-inspector .objectBox-object");

  info("Ctrl+click on an object to put it in the sidebar");
  const onSidebarShown = waitFor(() =>
    hud.ui.document.querySelector(".sidebar")
  );
  EventUtils.sendMouseEvent(
    {
      type: "click",
      [isMacOS ? "metaKey" : "ctrlKey"]: true,
    },
    object,
    hud.ui.window
  );
  await onSidebarShown;
  ok(true, "sidebar is displayed after user Ctrl+clicked on it");

  const sidebarContents = hud.ui.document.querySelector(".sidebar-contents");
  let objectInspectors = [...sidebarContents.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    1,
    "There is the expected number of object inspectors"
  );
  let [objectInspector] = objectInspectors;

  // The object in the sidebar now should look like:
  // ▼ { … }
  // |   a: 1
  // |   b: 2
  // | ▶︎ c: Array [3]
  // | ▶︎ <prototype>: Object { … }
  await waitFor(() => objectInspector.querySelectorAll(".node").length === 5);

  let propertiesNodes = [
    ...objectInspector.querySelectorAll(".object-label"),
  ].map(el => el.textContent);
  let arrayPropertiesNames = ["a", "b", "c", "<prototype>"];
  is(
    JSON.stringify(propertiesNodes),
    JSON.stringify(arrayPropertiesNames),
    "The expected nodes are displayed"
  );

  info("Expand the object in the console output");
  object.click();
  await waitFor(() => message.querySelectorAll(".node").length === 5);
  const cNode = message.querySelectorAll(".node")[3];
  info("Ctrl+click on the `c` property node to put it in the sidebar");
  EventUtils.sendMouseEvent(
    {
      type: "click",
      [isMacOS ? "metaKey" : "ctrlKey"]: true,
    },
    cNode,
    hud.ui.window
  );

  objectInspectors = [...sidebarContents.querySelectorAll(".tree")];
  is(objectInspectors.length, 1, "There is still only one object inspector");
  [objectInspector] = objectInspectors;

  // The object in the sidebar now should look like:
  // ▼ (1) […]
  // |   0: 3
  // |   length: 1
  // | ▶︎ <prototype>: Array []
  await waitFor(() => objectInspector.querySelectorAll(".node").length === 4);

  propertiesNodes = [...objectInspector.querySelectorAll(".object-label")].map(
    el => el.textContent
  );
  arrayPropertiesNames = ["0", "length", "<prototype>"];
  is(
    JSON.stringify(propertiesNodes),
    JSON.stringify(arrayPropertiesNames),
    "The expected nodes are displayed"
  );
});
