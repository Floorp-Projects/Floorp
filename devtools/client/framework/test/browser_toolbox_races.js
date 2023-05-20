/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Toggling the toolbox three time can take more than 45s on slow test machine
requestLongerTimeout(2);

// Test toggling the toolbox quickly and see if there is any race breaking it.

const URL = "data:text/html;charset=utf-8,Toggling devtools quickly";
const {
  gDevToolsBrowser,
} = require("resource://devtools/client/framework/devtools-browser.js");

add_task(async function () {
  // Make sure this test starts with the selectedTool pref cleared. Previous
  // tests select various tools, and that sets this pref.
  Services.prefs.clearUserPref("devtools.toolbox.selectedTool");

  await addTab(URL);

  let ready = 0,
    destroy = 0,
    destroyed = 0;
  const onReady = () => {
    ready++;
  };
  const onDestroy = () => {
    destroy++;
  };
  const onDestroyed = () => {
    destroyed++;
  };
  gDevTools.on("toolbox-ready", onReady);
  gDevTools.on("toolbox-destroy", onDestroy);
  gDevTools.on("toolbox-destroyed", onDestroyed);

  // The current implementation won't toggle the toolbox many times,
  // instead it will ignore toggles that happens while the toolbox is still
  // creating or still destroying.

  info("Toggle the toolbox many times in a row");
  toggle();
  toggle();
  toggle();
  toggle();
  toggle();
  await wait(500);

  await waitFor(() => ready == 1);
  is(
    ready,
    1,
    "No matter how many times we called toggle, it will only open the toolbox once"
  );
  is(
    destroy,
    0,
    "All subsequent, synchronous call to toggle will be ignored and the toolbox won't be destroyed"
  );
  is(destroyed, 0);

  info("Retoggle the toolbox many times in a row");
  toggle();
  toggle();
  toggle();
  toggle();
  toggle();
  await wait(500);

  await waitFor(() => destroyed == 1);
  is(destroyed, 1, "Similarly, the toolbox will be closed");
  is(destroy, 1);
  is(
    ready,
    1,
    "and no other toolbox will be opened. The subsequent toggle will be ignored."
  );

  gDevTools.off("toolbox-ready", onReady);
  gDevTools.off("toolbox-destroy", onDestroy);
  gDevTools.off("toolbox-destroyed", onDestroyed);
  await wait(1000);

  gBrowser.removeCurrentTab();
});

function toggle() {
  // When enabling the input event prioritization, we'll reserve some time to
  // process input events in each frame. In that case, the synthesized input
  // events may delay the normal events. Replace synthesized key events by
  // toggleToolboxCommand to prevent the synthesized input events jam the
  // content process and cause the test timeout.
  gDevToolsBrowser.toggleToolboxCommand(window.gBrowser);
}
