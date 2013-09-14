/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-601177-log-levels.html";

function test()
{
  //requestLongerTimeout(20);
  //FIXME
  Services.prefs.setBoolPref("javascript.options.strict", true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("javascript.options.strict");
  });

  addTab("data:text/html;charset=utf-8,Web Console test for bug 601177: log levels");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);

  function consoleOpened(hud)
  {
    expectUncaughtException();
    content.location = TEST_URI;

    info("waiting for messages");

    waitForMessages({
      webconsole: hud,
      messages: [
        {
          text: "test-bug-601177-log-levels.html",
          category: CATEGORY_NETWORK,
          severity: SEVERITY_LOG,
        },
        {
          text: "test-bug-601177-log-levels.js",
          category: CATEGORY_NETWORK,
          severity: SEVERITY_LOG,
        },
        {
          text: "test-image.png",
          category: CATEGORY_NETWORK,
          severity: SEVERITY_LOG,
        },
        {
          text: "foobar-known-to-fail.png",
          category: CATEGORY_NETWORK,
          severity: SEVERITY_ERROR,
        },
        {
          text: "foobarBug601177exception",
          category: CATEGORY_JS,
          severity: SEVERITY_ERROR,
        },
        {
          text: "undefinedPropertyBug601177",
          category: CATEGORY_JS,
          severity: SEVERITY_WARNING,
        },
        {
          text: "foobarBug601177strictError",
          category: CATEGORY_JS,
          severity: SEVERITY_WARNING,
        },
      ],
    }).then(finishTest);
  }
}
