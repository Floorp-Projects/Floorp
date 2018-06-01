"use strict";
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that the style editor manages to finalize its stylesheet loading phase
// even if one stylesheet is missing, and that an error message is displayed.

const TESTCASE_URI = TEST_BASE + "missing.html";

add_task(async function() {
  const { ui, toolbox, panel } = await openStyleEditorForURL(TESTCASE_URI);

  // Note that we're not testing for a specific number of stylesheet editors
  // below because the test-page is loaded with chrome:// URL and, right now,
  // that means UA stylesheets are shown. So we avoid hardcoding the number of
  // stylesheets here.
  ok(ui.editors.length, "The UI contains style sheets.");

  const rootEl = panel.panelWindow.document.getElementById("style-editor-chrome");
  ok(!rootEl.classList.contains("loading"), "The loading indicator is hidden");

  const notifBox = toolbox.getNotificationBox();
  const notif = notifBox.getCurrentNotification();
  ok(notif, "The notification box contains a message");
  ok(notif.label.includes("Style sheet could not be loaded"),
    "The error message is the correct one");
  ok(notif.label.includes("missing-stylesheet.css"),
    "The error message contains the missing stylesheet's URL");
});
