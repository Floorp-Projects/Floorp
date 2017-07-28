/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Toggling the toolbox three time can take more than 45s on slow test machine
requestLongerTimeout(2);

// Test toggling the toolbox quickly and see if there is any race breaking it.

const URL = "data:text/html;charset=utf-8,Toggling devtools quickly";

add_task(function* () {
  // Make sure this test starts with the selectedTool pref cleared. Previous
  // tests select various tools, and that sets this pref.
  Services.prefs.clearUserPref("devtools.toolbox.selectedTool");

  let tab = yield addTab(URL);

  let created = 0, ready = 0, destroy = 0, destroyed = 0;
  let onCreated = () => {
    created++;
  };
  let onReady = () => {
    ready++;
  };
  let onDestroy = () => {
    destroy++;
  };
  let onDestroyed = () => {
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
    yield wait(50);
  }
  info("Toggled the toolbox 3 times");

  // Now wait for the 3rd toolbox to be fully ready before closing it.
  // We close the last toolbox manually, out of the first while() loop to
  // avoid races and be sure we end up we no toolbox and waited for all the
  // requests to be done.
  while (ready != 3) {
    yield wait(100);
  }
  toggle();
  while (destroyed != 3) {
    yield wait(100);
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
  EventUtils.synthesizeKey("VK_F12", {});
}
