/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that cached messages from nested iframes are displayed in the
// Web/Browser Console.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-consoleiframes.html";

function test()
{
  expectUncaughtException();
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    // Test for cached nsIConsoleMessages.
    Services.console.logStringMessage("test1 for bug859756");

    info("open web console");
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud)
{
  ok(hud, "web console opened");

  waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "main file",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        text: "blah",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR
      },
      {
        text: "iframe 1",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        count: 2
      },
      {
        text: "iframe 2",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG
      }
    ],
  }).then(() => {
    closeConsole(null, onWebConsoleClose);
  });
}

function onWebConsoleClose()
{
  info("web console closed");
  HUDConsoleUI.toggleBrowserConsole().then(onBrowserConsoleOpen);
}

function onBrowserConsoleOpen(hud)
{
  ok(hud, "browser console opened");
  Services.console.logStringMessage("test2 for bug859756");

  waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "main file",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },      
      {
        text: "blah",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR
      },
      {
        text: "iframe 1",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        count: 2
      },
      {
        text: "iframe 2",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG
      }
    ],
  }).then(() => {
    closeConsole(null, finishTest);
  });
}
