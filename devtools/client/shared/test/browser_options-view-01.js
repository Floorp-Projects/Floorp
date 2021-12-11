/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that options-view OptionsView responds to events correctly.

const { OptionsView } = require("devtools/client/shared/options-view");

const BRANCH = "devtools.debugger.";
const BLACK_BOX_PREF = "auto-black-box";
const PRETTY_PRINT_PREF = "auto-pretty-print";

const originalBlackBox = Services.prefs.getBoolPref(BRANCH + BLACK_BOX_PREF);
const originalPrettyPrint = Services.prefs.getBoolPref(
  BRANCH + PRETTY_PRINT_PREF
);

add_task(async function() {
  info("Setting a couple of preferences");
  Services.prefs.setBoolPref(BRANCH + BLACK_BOX_PREF, false);
  Services.prefs.setBoolPref(BRANCH + PRETTY_PRINT_PREF, true);

  info("Opening a test tab and a toolbox host to create the options view in");
  await addTab("about:blank");
  const { host, win } = await createHost("bottom", OPTIONS_VIEW_URL);

  await testOptionsView(win);

  info("Closing the host and current tab");
  host.destroy();
  gBrowser.removeCurrentTab();

  info("Resetting the preferences");
  Services.prefs.setBoolPref(BRANCH + BLACK_BOX_PREF, originalBlackBox);
  Services.prefs.setBoolPref(BRANCH + PRETTY_PRINT_PREF, originalPrettyPrint);
});

async function testOptionsView(win) {
  const events = [];
  const options = createOptionsView(win);
  await options.initialize();

  const $ = win.document.querySelector.bind(win.document);

  options.on("pref-changed", pref => events.push(pref));

  const ppEl = $("menuitem[data-pref='auto-pretty-print']");
  const bbEl = $("menuitem[data-pref='auto-black-box']");

  // Test default config
  is(ppEl.getAttribute("checked"), "true", "`true` prefs are checked on start");
  is(bbEl.getAttribute("checked"), "", "`false` prefs are unchecked on start");

  // Test buttons update when preferences update outside of the menu
  Services.prefs.setBoolPref(BRANCH + PRETTY_PRINT_PREF, false);
  Services.prefs.setBoolPref(BRANCH + BLACK_BOX_PREF, true);

  is(
    options.getPref(PRETTY_PRINT_PREF),
    false,
    "getPref returns correct value"
  );
  is(options.getPref(BLACK_BOX_PREF), true, "getPref returns correct value");

  is(
    ppEl.getAttribute("checked"),
    "",
    "menuitems update when preferences change"
  );
  is(
    bbEl.getAttribute("checked"),
    "true",
    "menuitems update when preferences change"
  );

  // Tests events are fired when preferences update outside of the menu
  is(events.length, 2, "two 'pref-changed' events fired");
  is(
    events[0],
    "auto-pretty-print",
    "correct pref passed in 'pref-changed' event (auto-pretty-print)"
  );
  is(
    events[1],
    "auto-black-box",
    "correct pref passed in 'pref-changed' event (auto-black-box)"
  );

  // Test buttons update when clicked and preferences are updated
  await click(options, win, ppEl);
  is(ppEl.getAttribute("checked"), "true", "menuitems update when clicked");
  is(
    Services.prefs.getBoolPref(BRANCH + PRETTY_PRINT_PREF),
    true,
    "preference updated via click"
  );

  await click(options, win, bbEl);
  is(bbEl.getAttribute("checked"), "", "menuitems update when clicked");
  is(
    Services.prefs.getBoolPref(BRANCH + BLACK_BOX_PREF),
    false,
    "preference updated via click"
  );

  // Tests events are fired when preferences updated via click
  is(events.length, 4, "two 'pref-changed' events fired");
  is(
    events[2],
    "auto-pretty-print",
    "correct pref passed in 'pref-changed' event (auto-pretty-print)"
  );
  is(
    events[3],
    "auto-black-box",
    "correct pref passed in 'pref-changed' event (auto-black-box)"
  );

  await options.destroy();
}

function createOptionsView(win) {
  return new OptionsView({
    branchName: BRANCH,
    menupopup: win.document.querySelector("#options-menupopup"),
  });
}

async function click(view, win, menuitem) {
  const opened = view.once("options-shown");
  const closed = view.once("options-hidden");

  const button = win.document.querySelector("#options-button");
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  await opened;
  is(button.getAttribute("open"), "true", "button has `open` attribute");

  EventUtils.synthesizeMouseAtCenter(menuitem, {}, win);
  await closed;
  ok(!button.hasAttribute("open"), "button does not have `open` attribute");
}
