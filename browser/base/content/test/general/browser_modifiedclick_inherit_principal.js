"use strict";

const kURL =
  "http://example.com/browser/browser/base/content/test/general/dummy_page.html";
  "data:text/html,<a href=''>Middle-click me</a>";

/*
 * Check that when manually opening content JS links in new tabs/windows,
 * we use the correct principal, and we don't clear the URL bar.
 */
add_task(function* () {
 yield BrowserTestUtils.withNewTab(kURL, function* (browser) {
   let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
   yield ContentTask.spawn(browser, null, function* () {
     let a = content.document.createElement("a");
     a.href = "javascript:document.write('spoof'); void(0);";
     a.textContent = "Some link";
     content.document.body.appendChild(a);
   });
   info("Added element");
   yield BrowserTestUtils.synthesizeMouseAtCenter("a", {button: 1}, browser);
   let newTab = yield newTabPromise;
   is(newTab.linkedBrowser.contentPrincipal.origin, "http://example.com",
      "Principal should be for example.com");
   yield BrowserTestUtils.switchTab(gBrowser, newTab);
   info(gURLBar.value);
   isnot(gURLBar.value, "", "URL bar should not be empty.");
   yield BrowserTestUtils.removeTab(newTab);
 });
});
