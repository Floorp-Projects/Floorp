/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Toggling the toolbox three time can take more than 45s on slow test machine
requestLongerTimeout(2);

// Test toggling the toolbox quickly and see if there is any race breaking it.

const URL = "data:text/html;charset=utf-8,Toggling devtools quickly";
const {
  gDevToolsBrowser,
} = require("devtools/client/framework/devtools-browser");

add_task(async function() {
  // Make sure this test starts with the selectedTool pref cleared. Previous
  // tests select various tools, and that sets this pref.
  Services.prefs.clearUserPref("devtools.toolbox.selectedTool");

  await addTab(URL);

  let created = 0,
    ready = 0,
    destroy = 0,
    destroyed = 0;
  const onCreated = () => {
    created++;
  };
  const onReady = () => {
    ready++;
  };
  const onDestroy = () => {
    destroy++;
  };
  const onDestroyed = () => {
    destroyed++;
  };
  gDevTools.on("toolbox-created", onCreated);
  gDevTools.on("toolbox-ready", onReady);
  gDevTools.on("toolbox-destroy", onDestroy);
  gDevTools.on("toolbox-destroyed", onDestroyed);

  // The current implementation won't toggle the toolbox many times,
  // instead it will ignore toggles that happens while the toolbox is still
  // creating or still destroying.

  // Toggle the toolbox at least 3 times.
  info("Trying to toggle the toolbox 3 times");
  while (created < 3) {
    // Sent multiple event to try to race the code during toolbox creation and destruction
    toggle();
    toggle();
    toggle();

    // Release the event loop to let a chance to actually create or destroy the toolbox!
    await wait(50);
  }
  info("Toggled the toolbox 3 times");

  // Now wait for the 3rd toolbox to be fully ready before closing it.
  // We close the last toolbox manually, out of the first while() loop to
  // avoid races and be sure we end up we no toolbox and waited for all the
  // requests to be done.
  while (ready != 3) {
    await wait(100);
  }
  toggle();
  while (destroyed != 3) {
    await wait(100);
  }

  is(created, 3, "right number of created events");
  is(ready, 3, "right number of ready events");
  is(destroy, 3, "right number of destroy events");
  is(destroyed, 3, "right number of destroyed events");

  gDevTools.off("toolbox-created", onCreated);
  gDevTools.off("toolbox-ready", onReady);
  gDevTools.off("toolbox-destroy", onDestroy);
  gDevTools.off("toolbox-destroyed", onDestroyed);

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
