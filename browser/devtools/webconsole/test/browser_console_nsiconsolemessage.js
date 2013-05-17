/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that nsIConsoleMessages show in the Browser Console and in the Web
// Console. See bug 859756.

const TEST_URI = "data:text/html;charset=utf8,<title>bug859756</title>\n" +
                 "<p>hello world\n<p>nsIConsoleMessages ftw!";

let gWebConsole, gJSTerm, gVariablesView;

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    // Test for cached nsIConsoleMessages.
    sendMessage("test1 for bug859756", "cat2012");

    openConsole(null, consoleOpened);
  }, true);
}

function sendMessage(aMessage, aCategory)
{
  let windowID = WebConsoleUtils.getInnerWindowId(content);
  let consoleMsg = Cc["@mozilla.org/consolemessage;1"]
                   .createInstance(Ci.nsIConsoleMessage);
  consoleMsg.initMessage(aMessage, aCategory, windowID);
  Services.console.logMessage(consoleMsg);
}

function consoleOpened(hud)
{
  gWebConsole = hud;
  gJSTerm = hud.jsterm;

  // Send a message with no window ID.
  Services.console.logStringMessage("do-not-show-me");

  sendMessage("test2 for bug859756", "cat2013");

  waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "test1 for bug859756",
        category: CATEGORY_JS,
      },
      {
        text: "test2 for bug859756",
        category: CATEGORY_JS,
      },
    ],
  }).then(onLogMessages);
}

function onLogMessages()
{
  let text = gWebConsole.outputNode.textContent;
  is(text.indexOf("do-not-show-me"), -1,
     "message without window ID is not displayed");
  closeConsole(null, onWebConsoleClose);
}

function onWebConsoleClose()
{
  HUDConsoleUI.toggleBrowserConsole().then(onBrowserConsoleOpen);
}

function onBrowserConsoleOpen(hud)
{
  waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "test1 for bug859756",
        category: CATEGORY_JS,
      },
      {
        text: "test2 for bug859756",
        category: CATEGORY_JS,
      },
      {
        text: "do-not-show-me",
        category: CATEGORY_JS,
      },
    ],
  }).then(finishTest);
}
