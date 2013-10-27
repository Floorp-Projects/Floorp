/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the basic features of the Browser Console, bug 587757.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html?" + Date.now();

function test()
{
  Services.obs.addObserver(function observer(aSubject) {
    Services.obs.removeObserver(observer, "web-console-created");
    aSubject.QueryInterface(Ci.nsISupportsString);

    let hud = HUDService.getBrowserConsole();
    ok(hud, "browser console is open");
    is(aSubject.data, hud.hudId, "notification hudId is correct");

    executeSoon(() => consoleOpened(hud));
  }, "web-console-created", false);

  let hud = HUDService.getBrowserConsole();
  ok(!hud, "browser console is not open");
  info("wait for the browser console to open with ctrl-shift-j");
  EventUtils.synthesizeKey("j", { accelKey: true, shiftKey: true }, content);
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
  xhr.onload = () => console.log("xhr loaded, status is: " + xhr.status);
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
    xhrRequest = text.indexOf("test-console.html");
  }

  function showResults()
  {
    isnot(chromeConsole, -1, "chrome window console.log() is displayed");
    isnot(contentConsole, -1, "content window console.log() is displayed");
    isnot(execValue, -1, "jsterm eval result is displayed");
    isnot(exception, -1, "exception is displayed");
    isnot(xhrRequest, -1, "xhr request is displayed");
  }

  waitForSuccess({
    name: "messages displayed",
    validatorFn: () => {
      performChecks();
      return chromeConsole > -1 &&
             contentConsole > -1 &&
             execValue > -1 &&
             exception > -1 &&
             xhrRequest > -1;
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
