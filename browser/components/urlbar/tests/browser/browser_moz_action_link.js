"use strict";

const kURIs = ["moz-action:foo,", "moz-action:foo"];

add_task(async function() {
  for (let uri of kURIs) {
    let dataURI = `data:text/html,<a id=a href="${uri}" target=_blank rel="opener">Link</a>`;
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, dataURI);

    let tabSwitchPromise = BrowserTestUtils.switchTab(gBrowser, function() {});
    await ContentTask.spawn(tab.linkedBrowser, null, async function() {
      content.document.getElementById("a").click();
    });
    await tabSwitchPromise;
    isnot(gBrowser.selectedTab, tab, "Switched to new tab!");
    is(
      gURLBar.value,
      "about:blank",
      "URL bar should be displaying about:blank"
    );
    let newTab = gBrowser.selectedTab;
    await BrowserTestUtils.switchTab(gBrowser, tab);
    await BrowserTestUtils.switchTab(gBrowser, newTab);
    is(gBrowser.selectedTab, newTab, "Switched to new tab again!");
    is(
      gURLBar.value,
      "about:blank",
      "URL bar should be displaying about:blank after tab switch"
    );
    // Finally, check that directly setting it produces the right results, too:
    URLBarSetURI(makeURI(uri));
    is(
      gURLBar.value,
      "about:blank",
      "URL bar should still be displaying about:blank"
    );
    BrowserTestUtils.removeTab(newTab);
    BrowserTestUtils.removeTab(tab);
  }
});
