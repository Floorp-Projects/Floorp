"use strict";

const kURIs = [
  "moz-action:foo,",
  "moz-action:foo",
];

add_task(function*() {
  for (let uri of kURIs) {
    let dataURI = `data:text/html,<a id=a href="${uri}" target=_blank>Link</a>`;
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, dataURI);

    let tabSwitchPromise = BrowserTestUtils.switchTab(gBrowser, function() {});
    yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
      content.document.getElementById("a").click();
    });
    yield tabSwitchPromise;
    isnot(gBrowser.selectedTab, tab, "Switched to new tab!");
    is(gURLBar.value, "about:blank", "URL bar should be displaying about:blank");
    let newTab = gBrowser.selectedTab;
    yield BrowserTestUtils.switchTab(gBrowser, tab);
    yield BrowserTestUtils.switchTab(gBrowser, newTab);
    is(gBrowser.selectedTab, newTab, "Switched to new tab again!");
    is(gURLBar.value, "about:blank", "URL bar should be displaying about:blank after tab switch");
    // Finally, check that directly setting it produces the right results, too:
    URLBarSetURI(makeURI(uri));
    is(gURLBar.value, "about:blank", "URL bar should still be displaying about:blank");
    yield BrowserTestUtils.removeTab(newTab);
    yield BrowserTestUtils.removeTab(tab);
  }
});
