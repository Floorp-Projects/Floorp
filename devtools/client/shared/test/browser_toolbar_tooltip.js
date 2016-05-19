/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar works properly

// /////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Protocol error (unknownError): Error: Got an invalid root window in DocumentWalker");

const TEST_URI = "data:text/html;charset=utf-8,<p>Tooltip Tests</p>";
const PREF_DEVTOOLS_THEME = "devtools.theme";

registerCleanupFunction(() => {
  // Set preferences back to their original values
  Services.prefs.clearUserPref(PREF_DEVTOOLS_THEME);
});

add_task(function* showToolbar() {
  yield addTab(TEST_URI);

  info("Starting browser_toolbar_tooltip.js");

  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in runTest");

  let showPromise = observeOnce(DeveloperToolbar.NOTIFICATIONS.SHOW);
  document.getElementById("menu_devToolbar").doCommand();
  yield showPromise;
});

add_task(function* testDimensions() {
  let tooltipPanel = DeveloperToolbar.tooltipPanel;

  DeveloperToolbar.focusManager.helpRequest();
  yield DeveloperToolbar.inputter.setInput("help help");

  DeveloperToolbar.inputter.setCursor({ start: "help help".length });
  is(tooltipPanel._dimensions.start, "help ".length,
          "search param start, when cursor at end");
  ok(getLeftMargin() > 30, "tooltip offset, when cursor at end");

  DeveloperToolbar.inputter.setCursor({ start: "help".length });
  is(tooltipPanel._dimensions.start, 0,
          "search param start, when cursor at end of command");
  ok(getLeftMargin() > 9, "tooltip offset, when cursor at end of command");

  DeveloperToolbar.inputter.setCursor({ start: "help help".length - 1 });
  is(tooltipPanel._dimensions.start, "help ".length,
          "search param start, when cursor at penultimate position");
  ok(getLeftMargin() > 30, "tooltip offset, when cursor at penultimate position");

  DeveloperToolbar.inputter.setCursor({ start: 0 });
  is(tooltipPanel._dimensions.start, 0,
          "search param start, when cursor at start");
  ok(getLeftMargin() > 9, "tooltip offset, when cursor at start");
});

add_task(function* testThemes() {
  let tooltipPanel = DeveloperToolbar.tooltipPanel;
  ok(tooltipPanel.document, "Tooltip panel is initialized");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");

  yield DeveloperToolbar.inputter.setInput("");
  yield DeveloperToolbar.inputter.setInput("help help");
  is(tooltipPanel.document.documentElement.getAttribute("devtoolstheme"),
     "dark", "Tooltip panel has correct theme");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");

  yield DeveloperToolbar.inputter.setInput("");
  yield DeveloperToolbar.inputter.setInput("help help");
  is(tooltipPanel.document.documentElement.getAttribute("devtoolstheme"),
     "light", "Tooltip panel has correct theme");
});


add_task(function* hideToolbar() {
  info("Ending browser_toolbar_tooltip.js");
  yield DeveloperToolbar.inputter.setInput("");

  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible in hideToolbar");

  info("Hide toolbar");
  let hidePromise = observeOnce(DeveloperToolbar.NOTIFICATIONS.HIDE);
  document.getElementById("menu_devToolbar").doCommand();
  yield hidePromise;

  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in hideToolbar");

  info("Done test");
});

function getLeftMargin() {
  let style = DeveloperToolbar.tooltipPanel._panel.style.marginLeft;
  return parseInt(style.slice(0, -2), 10);
}

function observeOnce(topic, ownsWeak = false) {
  return new Promise(function (resolve, reject) {
    let resolver = function (subject) {
      Services.obs.removeObserver(resolver, topic);
      resolve(subject);
    };
    Services.obs.addObserver(resolver, topic, ownsWeak);
  }.bind(this));
}
