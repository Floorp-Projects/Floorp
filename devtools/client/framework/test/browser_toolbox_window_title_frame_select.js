/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the detached devtools window title is not updated when switching
 * the selected frame. Also check that frames command button has 'open'
 * attribute set when the list of frames is opened.
 */

var {Toolbox} = require("devtools/client/framework/toolbox");
const URL = URL_ROOT + "browser_toolbox_window_title_frame_select_page.html";
const IFRAME_URL = URL_ROOT + "browser_toolbox_window_title_changes_page.html";
const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

add_task(async function() {
  Services.prefs.setBoolPref("devtools.command-button-frames.enabled", true);

  await addTab(URL);
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = await gDevTools.showToolbox(target, null,
    Toolbox.HostType.BOTTOM);

  await toolbox.switchHost(Toolbox.HostType.WINDOW);
  // Wait for title change event *after* switch host, in order to listen
  // for the event on the WINDOW host window, which only exists after switchHost
  await waitForTitleChange(toolbox);

  is(getTitle(), `Developer Tools - Page title - ${URL}`,
    "Devtools title correct after switching to detached window host");

  // Wait for tick to avoid unexpected 'popuphidden' event, which
  // blocks the frame popup menu opened below. See also bug 1276873
  await waitForTick();

  const btn = toolbox.doc.getElementById("command-button-frames");

  await testShortcutToOpenFrames(btn, toolbox);

  // Open frame menu and wait till it's available on the screen.
  // Also check 'aria-expanded' attribute on the command button.
  is(btn.getAttribute("aria-expanded"), "false",
     "The aria-expanded attribute must be set to false");
  btn.click();

  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  is(btn.getAttribute("aria-expanded"), "true",
     "The aria-expanded attribute must be set to true");

  // Verify that the frame list menu is populated
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command"));
  is(frames.length, 2, "We have both frames in the list");

  const topFrameBtn =
        frames.filter(b => b.querySelector(".label").textContent == URL)[0];
  const iframeBtn =
        frames.filter(b => b.querySelector(".label").textContent == IFRAME_URL)[0];
  ok(topFrameBtn, "Got top level document in the list");
  ok(iframeBtn, "Got iframe document in the list");

  // Listen to will-navigate to check if the view is empty
  const willNavigate = toolbox.target.once("will-navigate");

  const onTitleChanged = waitForTitleChange(toolbox);

  // Only select the iframe after we are able to select an element from the top
  // level document.
  const newRoot = toolbox.getPanel("inspector").once("new-root");
  info("Select the iframe");
  iframeBtn.click();

  await willNavigate;
  await newRoot;
  await onTitleChanged;

  info("Navigation to the iframe is done, the inspector should be back up");
  is(getTitle(), `Developer Tools - Page title - ${URL}`,
    "Devtools title was not updated after changing inspected frame");

  info("Cleanup toolbox and test preferences.");
  await toolbox.destroy();
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

async function testShortcutToOpenFrames(btn, toolbox) {
  info("Tests if shortcut Alt+Down opens the frames");
  // focus the button so that keyPress can be performed
  btn.focus();
  // perform keyPress - Alt+Down
  const shortcut = L10N.getStr("toolbox.showFrames.key");
  synthesizeKeyShortcut(shortcut, toolbox.win);

  const panel = toolbox.doc.getElementById("command-button-frames-panel");
  ok(panel, "popup panel has created.");
  await waitUntil(() => panel.classList.contains("tooltip-visible"));

  is(btn.getAttribute("aria-expanded"), "true",
     "The aria-expanded attribute must be set to true");

  // pressing Esc should hide the menu again
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await waitUntil(() => !panel.classList.contains("tooltip-visible"));

  is(btn.getAttribute("aria-expanded"), "false",
     "The aria-expanded attribute must be set to false");
}
