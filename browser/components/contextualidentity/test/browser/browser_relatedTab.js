"use strict";

/*
 * Bug 1325014 - Adding tab related to current tab inherits current tab's container usercontextid unless otherwise specified
 */

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    userContextId: 1,
  });
  let ReferrerInfo = Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  );

  gBrowser.selectedTab = tab;
  let relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    relatedToCurrent: true,
  });
  is(
    relatedTab.getAttribute("usercontextid"),
    1,
    "Related tab (relatedToCurrent) inherits current tab's usercontextid"
  );
  BrowserTestUtils.removeTab(relatedTab);

  gBrowser.selectedTab = tab;
  relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    relatedToCurrent: true,
    userContextId: 2,
  });
  is(
    relatedTab.getAttribute("usercontextid"),
    2,
    "Related tab (relatedToCurrent) with overridden usercontextid"
  );
  BrowserTestUtils.removeTab(relatedTab);

  gBrowser.selectedTab = tab;
  let referrerInfo = new ReferrerInfo(
    Ci.nsIReferrerInfo.EMPTY,
    true,
    gBrowser.currentURI
  );
  relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    referrerInfo,
  });
  is(
    relatedTab.getAttribute("usercontextid"),
    1,
    "Related tab (referrer) inherits current tab's usercontextid"
  );
  BrowserTestUtils.removeTab(relatedTab);

  gBrowser.selectedTab = tab;
  referrerInfo = new ReferrerInfo(
    Ci.nsIReferrerInfo.EMPTY,
    true,
    gBrowser.currentURI
  );
  relatedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    referrerInfo,
    userContextId: 2,
  });
  is(
    relatedTab.getAttribute("usercontextid"),
    2,
    "Related tab (referrer) with overridden usercontextid"
  );
  BrowserTestUtils.removeTab(relatedTab);

  BrowserTestUtils.removeTab(tab);
});
