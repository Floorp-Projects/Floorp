/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 874061: test for how the browser and web consoles display messages coming
// from private windows. See bug for description of expected behavior.

"use strict";

function test() {
  const TEST_URI = "data:text/html;charset=utf8,<p>hello world! bug 874061" +
                   "<button onclick='console.log(\"foobar bug 874061\");" +
                   "fooBazBaz.yummy()'>click</button>";
  let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
                            .getService(Ci.nsIConsoleAPIStorage);
  let privateWindow, privateBrowser, privateTab, privateContent;
  let hud, expectedMessages, nonPrivateMessage;

  // This test is slightly more involved: it opens the web console twice,
  // a new private window once, and the browser console twice. We can get
  // a timeout with debug builds on slower machines.
  requestLongerTimeout(2);
  start();

  function start() {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "data:text/html;charset=utf8," +
                                                   "<p>hello world! I am not private!");
    gBrowser.selectedBrowser.addEventListener("load", onLoadTab, true);
  }

  function onLoadTab() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoadTab, true);
    info("onLoadTab()");

    // Make sure we have a clean state to start with.
    Services.console.reset();
    ConsoleAPIStorage.clearEvents();

    // Add a non-private message to the browser console.
    ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
      content.console.log("bug874061-not-private");
    });

    nonPrivateMessage = {
      name: "console message from a non-private window",
      text: "bug874061-not-private",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    };

    privateWindow = OpenBrowserWindow({ private: true });
    ok(privateWindow, "new private window");
    ok(PrivateBrowsingUtils.isWindowPrivate(privateWindow), "window's private");

    whenDelayedStartupFinished(privateWindow, onPrivateWindowReady);
  }

  function onPrivateWindowReady() {
    info("private browser window opened");
    privateBrowser = privateWindow.gBrowser;

    privateTab = privateBrowser.selectedTab = privateBrowser.addTab(TEST_URI);
    privateBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      info("private tab opened");
      privateBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
      privateContent = privateBrowser.selectedBrowser.contentWindow;
      ok(PrivateBrowsingUtils.isBrowserPrivate(privateBrowser.selectedBrowser),
         "tab window is private");
      openConsole(privateTab).then(consoleOpened);
    }, true);
  }

  function addMessages() {
    let button = privateContent.document.querySelector("button");
    ok(button, "button in page");
    EventUtils.synthesizeMouse(button, 2, 2, {}, privateContent);
  }

  function consoleOpened(injectedHud) {
    hud = injectedHud;
    ok(hud, "web console opened");

    addMessages();
    expectedMessages = [
      {
        name: "script error",
        text: "fooBazBaz is not defined",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        name: "console message",
        text: "foobar bug 874061",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
    ];

    // Make sure messages are displayed in the web console as they happen, even
    // if this is a private tab.
    waitForMessages({
      webconsole: hud,
      messages: expectedMessages,
    }).then(testCachedMessages);
  }

  function testCachedMessages() {
    info("testCachedMessages()");
    closeConsole(privateTab).then(() => {
      info("web console closed");
      openConsole(privateTab).then(consoleReopened);
    });
  }

  function consoleReopened(injectedHud) {
    hud = injectedHud;
    ok(hud, "web console reopened");

    // Make sure that cached messages are displayed in the web console, even
    // if this is a private tab.
    waitForMessages({
      webconsole: hud,
      messages: expectedMessages,
    }).then(testBrowserConsole);
  }

  function testBrowserConsole() {
    info("testBrowserConsole()");
    closeConsole(privateTab).then(() => {
      info("web console closed");
      HUDService.toggleBrowserConsole().then(onBrowserConsoleOpen);
    });
  }

  // Make sure that the cached messages from private tabs are not displayed in
  // the browser console.
  function checkNoPrivateMessages() {
    let text = hud.outputNode.textContent;
    is(text.indexOf("fooBazBaz"), -1, "no exception displayed");
    is(text.indexOf("bug 874061"), -1, "no console message displayed");
  }

  function onBrowserConsoleOpen(injectedHud) {
    hud = injectedHud;
    ok(hud, "browser console opened");

    checkNoPrivateMessages();
    addMessages();
    expectedMessages.push(nonPrivateMessage);

    // Make sure that live messages are displayed in the browser console, even
    // from private tabs.
    waitForMessages({
      webconsole: hud,
      messages: expectedMessages,
    }).then(testPrivateWindowClose);
  }

  function testPrivateWindowClose() {
    info("close the private window and check if private messages are removed");
    hud.jsterm.once("private-messages-cleared", () => {
      isnot(hud.outputNode.textContent.indexOf("bug874061-not-private"), -1,
            "non-private messages are still shown after private window closed");
      checkNoPrivateMessages();

      info("close the browser console");
      HUDService.toggleBrowserConsole().then(() => {
        info("reopen the browser console");
        executeSoon(() =>
          HUDService.toggleBrowserConsole().then(onBrowserConsoleReopen));
      });
    });
    privateWindow.BrowserTryToCloseWindow();
  }

  function onBrowserConsoleReopen(injectedHud) {
    hud = injectedHud;
    ok(hud, "browser console reopened");

    // Make sure that the non-private message is still shown after reopen.
    waitForMessages({
      webconsole: hud,
      messages: [nonPrivateMessage],
    }).then(() => {
      // Make sure that no private message is displayed after closing the
      // private window and reopening the Browser Console.
      checkNoPrivateMessages();
      executeSoon(finishTest);
    });
  }
}
