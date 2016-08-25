/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from shared-head.js */

"use strict";

/**
 * Check that the detached devtools window title is not updated when switching
 * the selected frame. Also check that frames command button has 'open'
 * attribute set when the list of frames is opened.
 */

var {Toolbox} = require("devtools/client/framework/toolbox");
const URL = URL_ROOT + "browser_toolbox_window_title_frame_select_page.html";
const IFRAME_URL = URL_ROOT + "browser_toolbox_window_title_changes_page.html";

add_task(function* () {
  Services.prefs.setBoolPref("devtools.command-button-frames.enabled", true);

  yield addTab(URL);
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, null,
    Toolbox.HostType.BOTTOM);

  let onTitleChanged = waitForTitleChange(toolbox);
  yield toolbox.selectTool("inspector");
  yield onTitleChanged;

  yield toolbox.switchHost(Toolbox.HostType.WINDOW);
  // Wait for title change event *after* switch host, in order to listen
  // for the event on the WINDOW host window, which only exists after switchHost
  yield waitForTitleChange(toolbox);

  is(getTitle(), `Developer Tools - Page title - ${URL}`,
    "Devtools title correct after switching to detached window host");

  // Wait for tick to avoid unexpected 'popuphidden' event, which
  // blocks the frame popup menu opened below. See also bug 1276873
  yield waitForTick();

  // Open frame menu and wait till it's available on the screen.
  // Also check 'open' attribute on the command button.
  let btn = toolbox.doc.getElementById("command-button-frames");
  ok(!btn.getAttribute("open"), "The open attribute must not be present");
  let menu = toolbox.showFramesMenu({target: btn});
  yield once(menu, "open");

  is(btn.getAttribute("open"), "true", "The open attribute must be set");

  // Verify that the frame list menu is populated
  let frames = menu.items;
  is(frames.length, 2, "We have both frames in the list");

  let topFrameBtn = frames.filter(b => b.label == URL)[0];
  let iframeBtn = frames.filter(b => b.label == IFRAME_URL)[0];
  ok(topFrameBtn, "Got top level document in the list");
  ok(iframeBtn, "Got iframe document in the list");

  // Listen to will-navigate to check if the view is empty
  let willNavigate = toolbox.target.once("will-navigate");

  onTitleChanged = waitForTitleChange(toolbox);

  // Only select the iframe after we are able to select an element from the top
  // level document.
  let newRoot = toolbox.getPanel("inspector").once("new-root");
  info("Select the iframe");
  iframeBtn.click();

  yield willNavigate;
  yield newRoot;
  yield onTitleChanged;

  info("Navigation to the iframe is done, the inspector should be back up");
  is(getTitle(), `Developer Tools - Page title - ${URL}`,
    "Devtools title was not updated after changing inspected frame");

  info("Cleanup toolbox and test preferences.");
  yield toolbox.destroy();
  toolbox = null;
  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.selectedTool");
  Services.prefs.clearUserPref("devtools.toolbox.sideEnabled");
  Services.prefs.clearUserPref("devtools.command-button-frames.enabled");
  finish();
});

function getTitle() {
  return Services.wm.getMostRecentWindow("devtools:toolbox").document.title;
}
