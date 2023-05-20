/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests keyboard navigation of the DevTools notification box.

// The test page attempts to load a stylesheet at an invalid URL which will
// trigger a devtools notification to show up on top of the window.
const TEST_PAGE = `<link rel="stylesheet" type="text/css" href="http://mochi.test:1234/invalid.port">`;
const TEST_URL = `data:text/html;charset=utf8,${TEST_PAGE}`;

add_task(async function () {
  info("Create a test tab and open the toolbox");
  const toolbox = await openNewTabAndToolbox(TEST_URL, "styleeditor");
  const doc = toolbox.doc;

  info("Wait until the notification box displays the stylesheet warning");
  const notificationBox = await waitFor(() =>
    doc.querySelector(".notificationbox")
  );

  ok(
    notificationBox.querySelector(".notification"),
    "A notification is rendered"
  );

  const toolbar = doc.querySelector(".devtools-tabbar");
  const tabButtons = toolbar.querySelectorAll(".devtools-tab, button");

  // Put the keyboard focus onto the first tab button.
  tabButtons[0].focus();
  is(doc.activeElement.id, tabButtons[0].id, "First tab button is focused.");

  // Move the focus to the notification box.
  info("Send a shift+tab key event to focus the previous focusable element");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  is(
    doc.activeElement,
    notificationBox.querySelector(".messageCloseButton"),
    "The focus is on the close button of the notification"
  );

  info("Send a vk_space key event to click on the close button");
  EventUtils.synthesizeKey("VK_SPACE");

  info("Wait until the notification is removed");
  await waitUntil(() => !notificationBox.querySelector(".notificationbox"));
});
