/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 740948 */

var DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";
var EDITOR_TEXT = [
  "var evt = new CustomEvent('foo', { bubbles: true });",
  "document.body.innerHTML = 'Modified text';",
  "window.dispatchEvent(evt);"
].join("\n");

function test()
{
  requestLongerTimeout(2);
  waitForExplicitFinish();
  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, true);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,Scratchpad test for bug 740948";
}

function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  // Test that Reload And Run command is enabled in the content
  // context and disabled in the browser context.

  let reloadAndRun = gScratchpadWindow.document.
    getElementById("sp-cmd-reloadAndRun");
  ok(reloadAndRun, "Reload And Run command exists");
  ok(!reloadAndRun.hasAttribute("disabled"),
      "Reload And Run command is enabled");

  sp.setBrowserContext();
  ok(reloadAndRun.hasAttribute("disabled"),
      "Reload And Run command is disabled in the browser context.");

  // Switch back to the content context and run our predefined
  // code. This code modifies the body of our document and dispatches
  // a custom event 'foo'. We listen to that event and check the
  // body to make sure that the page has been reloaded and Scratchpad
  // code has been executed.

  sp.setContentContext();
  sp.setText(EDITOR_TEXT);

  let browser = gBrowser.selectedBrowser;

  let deferred = promise.defer();
  browser.addEventListener("DOMWindowCreated", function onWindowCreated() {
    browser.removeEventListener("DOMWindowCreated", onWindowCreated, true);

    browser.contentWindow.addEventListener("foo", function onFoo() {
      browser.contentWindow.removeEventListener("foo", onFoo, true);

      is(browser.contentWindow.document.body.innerHTML, "Modified text",
        "After reloading, HTML is different.");

      Services.prefs.clearUserPref(DEVTOOLS_CHROME_ENABLED);
      deferred.resolve();
    }, true);
  }, true);

  ok(browser.contentWindow.document.body.innerHTML !== "Modified text",
      "Before reloading, HTML is intact.");
  sp.reloadAndRun().then(deferred.promise).then(finish);
}

