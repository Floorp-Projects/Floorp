"use strict";

const kURL =
  "http://example.com/browser/browser/base/content/test/general/dummy_page.html";
  "data:text/html,<a href=''>Middle-click me</a>";

/*
 * Check that when manually opening content JS links in new tabs/windows,
 * we use the correct principal, and we don't clear the URL bar.
 */
add_task(async function() {
 await BrowserTestUtils.withNewTab(kURL, async function(browser) {
   let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
   await ContentTask.spawn(browser, null, async function() {
     let a = content.document.createElement("a");
     a.href = "javascript:document.write('spoof'); void(0);";
     a.textContent = "Some link";
     content.document.body.appendChild(a);
   });
   info("Added element");
   await BrowserTestUtils.synthesizeMouseAtCenter("a", {button: 1}, browser);
   let newTab = await newTabPromise;
   is(newTab.linkedBrowser.contentPrincipal.origin, "http://example.com",
      "Principal should be for example.com");
   await BrowserTestUtils.switchTab(gBrowser, newTab);
   info(gURLBar.value);
   isnot(gURLBar.value, "", "URL bar should not be empty.");
   await BrowserTestUtils.removeTab(newTab);
 });
});
