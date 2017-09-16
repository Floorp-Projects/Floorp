/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ignoreAllUncaughtExceptions();

add_task(async function() {
  info("Cmd+k should focus the search box in the toolbar when it's present");

  Services.prefs.setBoolPref("browser.search.widget.inNavBar", true);

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter("#brandLogo", {}, browser);

    let doc = window.document;
    let searchInput = doc.getElementById("searchbar").textbox.inputField;
    isnot(searchInput, doc.activeElement, "Search bar should not be the active element.");

    EventUtils.synthesizeKey("k", { accelKey: true });
    await promiseWaitForCondition(() => doc.activeElement === searchInput);
    is(searchInput, doc.activeElement, "Search bar should be the active element.");
  });

  Services.prefs.clearUserPref("browser.search.widget.inNavBar");
});
