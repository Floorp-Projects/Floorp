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

add_task(async function test() {
  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, true);

  const url = "data:text/html,Scratchpad test for bug 740948";
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  await new Promise((resolve) => openScratchpad(resolve));
  await runTests();

  Services.prefs.clearUserPref(DEVTOOLS_CHROME_ENABLED);
});

async function runTests() {
  const sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  // Test that Reload And Run command is enabled in the content
  // context and disabled in the browser context.

  const reloadAndRun = gScratchpadWindow.document
    .getElementById("sp-cmd-reloadAndRun");
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

  const browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, null, function() {
    ok(content.document.body.innerHTML !== "Modified text",
      "Before reloading, HTML is intact.");
  });

  const reloaded = BrowserTestUtils.browserLoaded(browser);
  sp.reloadAndRun();
  await reloaded;

  await ContentTask.spawn(browser, null, async function() {
    // If `evt` is not defined, the scratchpad code has not run yet,
    // so we need to await the "foo" event.
    if (!content.wrappedJSObject.evt) {
      await new Promise((resolve) => {
        content.addEventListener("foo", resolve, {once: true});
      });
    }
    is(content.document.body.innerHTML, "Modified text",
      "After reloading, HTML is different.");
  });
}

