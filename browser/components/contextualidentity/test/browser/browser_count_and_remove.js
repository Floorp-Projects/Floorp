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
  is(ContextualIdentityService.countContainerTabs(1), 1, "1 container tab created with id 1");
  is(ContextualIdentityService.countContainerTabs(2), 0, "0 container tabs created with id 2");

  openTabInUserContext(1);
  is(ContextualIdentityService.countContainerTabs(), 2, "2 container tabs created");
  is(ContextualIdentityService.countContainerTabs(1), 2, "2 container tabs created with id 1");
  is(ContextualIdentityService.countContainerTabs(2), 0, "0 container tabs created with id 2");

  openTabInUserContext(2);
  is(ContextualIdentityService.countContainerTabs(), 3, "3 container tab created");
  is(ContextualIdentityService.countContainerTabs(1), 2, "2 container tabs created with id 1");
  is(ContextualIdentityService.countContainerTabs(2), 1, "1 container tab created with id 2");

  ContextualIdentityService.closeContainerTabs(1);
  is(ContextualIdentityService.countContainerTabs(), 1, "1 container tab created");
  is(ContextualIdentityService.countContainerTabs(1), 0, "0 container tabs created with id 1");
  is(ContextualIdentityService.countContainerTabs(2), 1, "1 container tab created with id 2");

  ContextualIdentityService.closeContainerTabs();
  is(ContextualIdentityService.countContainerTabs(), 0, "0 container tabs at the end.");
  is(ContextualIdentityService.countContainerTabs(1), 0, "0 container tabs at the end with id 1.");
  is(ContextualIdentityService.countContainerTabs(2), 0, "0 container tabs at the end with id 2.");
});
