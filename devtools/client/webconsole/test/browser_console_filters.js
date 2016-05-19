/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the Browser Console does not use the same filter prefs as the Web
// Console. See bug 878186.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>browser console filters";
const WEB_CONSOLE_PREFIX = "devtools.webconsole.filter.";
const BROWSER_CONSOLE_PREFIX = "devtools.browserconsole.filter.";

add_task(function* () {
  yield loadTab(TEST_URI);

  info("open the web console");
  let hud = yield openConsole();
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

  // We need to let the console opening event loop to finish.
  let deferred = promise.defer();
  executeSoon(() => closeConsole().then(() => deferred.resolve(null)));
  yield deferred.promise;

  info("web console closed");
  hud = yield HUDService.toggleBrowserConsole();
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
});
