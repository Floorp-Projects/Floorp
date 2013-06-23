/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that the Browser Console does not use the same filter prefs as the Web
// Console. See bug 878186.

const TEST_URI = "data:text/html;charset=utf8,<p>browser console filters";
const WEB_CONSOLE_PREFIX = "devtools.webconsole.filter.";
const BROWSER_CONSOLE_PREFIX = "devtools.browserconsole.filter.";

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    info("open the web console");
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud)
{
  ok(hud, "web console opened");

  is(Services.prefs.getBoolPref(BROWSER_CONSOLE_PREFIX + "exception"), true,
     "'exception' filter is enabled (browser console)");
  is(Services.prefs.getBoolPref(WEB_CONSOLE_PREFIX + "exception"), true,
     "'exception' filter is enabled (web console)");

  info("toggle 'exception' filter");
  hud.setFilterState("exception", false);

  is(Services.prefs.getBoolPref(BROWSER_CONSOLE_PREFIX + "exception"), true,
     "'exception' filter is enabled (browser console)");
  is(Services.prefs.getBoolPref(WEB_CONSOLE_PREFIX + "exception"), false,
     "'exception' filter is disabled (web console)");

  hud.setFilterState("exception", true);

  executeSoon(() => closeConsole(null, onWebConsoleClose));
}

function onWebConsoleClose()
{
  info("web console closed");
  HUDConsoleUI.toggleBrowserConsole().then(onBrowserConsoleOpen);
}

function onBrowserConsoleOpen(hud)
{
  ok(hud, "browser console opened");

  is(Services.prefs.getBoolPref(BROWSER_CONSOLE_PREFIX + "exception"), true,
     "'exception' filter is enabled (browser console)");
  is(Services.prefs.getBoolPref(WEB_CONSOLE_PREFIX + "exception"), true,
     "'exception' filter is enabled (web console)");

  info("toggle 'exception' filter");
  hud.setFilterState("exception", false);

  is(Services.prefs.getBoolPref(BROWSER_CONSOLE_PREFIX + "exception"), false,
     "'exception' filter is disabled (browser console)");
  is(Services.prefs.getBoolPref(WEB_CONSOLE_PREFIX + "exception"), true,
     "'exception' filter is enabled (web console)");

  hud.setFilterState("exception", true);

  executeSoon(finishTest);
}
