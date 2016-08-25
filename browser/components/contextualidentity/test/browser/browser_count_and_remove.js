/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/ContextualIdentityService.jsm");

function openTabInUserContext(userContextId) {
  let tab = gBrowser.addTab("about:blank", {userContextId});
  gBrowser.selectedTab = tab;
}

add_task(function* setup() {
  // make sure userContext is enabled.
  yield SpecialPowers.pushPrefEnv({"set": [
          ["privacy.userContext.enabled", true]
        ]});
});

add_task(function* test() {
  is(ContextualIdentityService.countContainerTabs(), 0, "0 container tabs by default.");

  openTabInUserContext(1);
  is(ContextualIdentityService.countContainerTabs(), 1, "1 container tab created");

  openTabInUserContext(1);
  is(ContextualIdentityService.countContainerTabs(), 2, "2 container tab created");

  openTabInUserContext(2);
  is(ContextualIdentityService.countContainerTabs(), 3, "3 container tab created");

  ContextualIdentityService.closeAllContainerTabs();
  is(ContextualIdentityService.countContainerTabs(), 0, "0 container tab at the end.");
});
