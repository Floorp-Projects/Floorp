"use strict";

/*
 * Bug 1325014 - Adding tab related to current tab inherits current tab's container usercontextid unless otherwise specified
 */

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {userContextId: 1});

  gBrowser.selectedTab = tab;
  let relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {relatedToCurrent: true});
  is(relatedTab.getAttribute("usercontextid"), 1, "Related tab (relatedToCurrent) inherits current tab's usercontextid");
  await BrowserTestUtils.removeTab(relatedTab);

  gBrowser.selectedTab = tab;
  relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {relatedToCurrent: true, userContextId: 2});
  is(relatedTab.getAttribute("usercontextid"), 2, "Related tab (relatedToCurrent) with overridden usercontextid");
  await BrowserTestUtils.removeTab(relatedTab);

  gBrowser.selectedTab = tab;
  relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {referrerURI: gBrowser.currentURI});
  is(relatedTab.getAttribute("usercontextid"), 1, "Related tab (referrer) inherits current tab's usercontextid");
  await BrowserTestUtils.removeTab(relatedTab);

  gBrowser.selectedTab = tab;
  relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {referrerURI: gBrowser.currentURI, userContextId: 2});
  is(relatedTab.getAttribute("usercontextid"), 2, "Related tab (referrer) with overridden usercontextid");
  await BrowserTestUtils.removeTab(relatedTab);

  await BrowserTestUtils.removeTab(tab);
});
