/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the basic features of the Browser Console, bug 587757.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-eval-in-stackframe.html";

function test()
{
  HUDConsoleUI.toggleBrowserConsole().then(consoleOpened);
}

function consoleOpened(hud)
{
  hud.jsterm.clearOutput(true);

  expectUncaughtException();
  executeSoon(() => {
    foobarExceptionBug587757();
  });

  // Add a message from a chrome window.
  hud.iframeWindow.console.log("bug587757a");

  // Add a message from a content window.
  content.console.log("bug587757b");

  // Test eval.
  hud.jsterm.execute("document.location.href");

  // Check for network requests.
  let xhr = new XMLHttpRequest();
  xhr.onload = () => info("xhr loaded, status is: " + xhr.status);
  xhr.open("get", TEST_URI, true);
  xhr.send();

  let chromeConsole = -1;
  let contentConsole = -1;
  let execValue = -1;
  let exception = -1;
  let xhrRequest = false;

  let output = hud.outputNode;
  function performChecks()
  {
    let text = output.textContent;
    chromeConsole = text.indexOf("bug587757a");
    contentConsole = text.indexOf("bug587757b");
    execValue = text.indexOf("browser.xul");
    exception = text.indexOf("foobarExceptionBug587757");

    xhrRequest = false;
    let urls = output.querySelectorAll(".webconsole-msg-url");
    for (let url of urls) {
      if (url.value.indexOf(TEST_URI) > -1) {
        xhrRequest = true;
        break;
      }
    }
  }

  function showResults()
  {
    isnot(chromeConsole, -1, "chrome window console.log() is displayed");
    isnot(contentConsole, -1, "content window console.log() is displayed");
    isnot(execValue, -1, "jsterm eval result is displayed");
    isnot(exception, -1, "exception is displayed");
    ok(xhrRequest, "xhr request is displayed");
  }

  waitForSuccess({
    name: "messages displayed",
    validatorFn: () => {
      performChecks();
      return chromeConsole > -1 &&
             contentConsole > -1 &&
             execValue > -1 &&
             exception > -1 &&
             xhrRequest;
    },
    successFn: () => {
      showResults();
      executeSoon(finishTest);
    },
    failureFn: () => {
      showResults();
      info("output: " + output.textContent);
      executeSoon(finishTest);
    },
  });
}
