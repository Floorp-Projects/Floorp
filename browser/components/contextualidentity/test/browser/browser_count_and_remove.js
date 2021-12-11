/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { ContextualIdentityService } = ChromeUtils.import(
  "resource://gre/modules/ContextualIdentityService.jsm"
);

function openTabInUserContext(userContextId) {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", { userContextId });
  gBrowser.selectedTab = tab;
}

add_task(async function setup() {
  // make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });
});

add_task(async function test() {
  is(
    ContextualIdentityService.countContainerTabs(),
    0,
    "0 container tabs by default."
  );

  openTabInUserContext(1);
  is(
    ContextualIdentityService.countContainerTabs(),
    1,
    "1 container tab created"
  );
  is(
    ContextualIdentityService.countContainerTabs(1),
    1,
    "1 container tab created with id 1"
  );
  is(
    ContextualIdentityService.countContainerTabs(2),
    0,
    "0 container tabs created with id 2"
  );

  openTabInUserContext(1);
  is(
    ContextualIdentityService.countContainerTabs(),
    2,
    "2 container tabs created"
  );
  is(
    ContextualIdentityService.countContainerTabs(1),
    2,
    "2 container tabs created with id 1"
  );
  is(
    ContextualIdentityService.countContainerTabs(2),
    0,
    "0 container tabs created with id 2"
  );

  openTabInUserContext(2);
  is(
    ContextualIdentityService.countContainerTabs(),
    3,
    "3 container tab created"
  );
  is(
    ContextualIdentityService.countContainerTabs(1),
    2,
    "2 container tabs created with id 1"
  );
  is(
    ContextualIdentityService.countContainerTabs(2),
    1,
    "1 container tab created with id 2"
  );

  await ContextualIdentityService.closeContainerTabs(1);
  is(
    ContextualIdentityService.countContainerTabs(),
    1,
    "1 container tab created"
  );
  is(
    ContextualIdentityService.countContainerTabs(1),
    0,
    "0 container tabs created with id 1"
  );
  is(
    ContextualIdentityService.countContainerTabs(2),
    1,
    "1 container tab created with id 2"
  );

  await ContextualIdentityService.closeContainerTabs();
  is(
    ContextualIdentityService.countContainerTabs(),
    0,
    "0 container tabs at the end."
  );
  is(
    ContextualIdentityService.countContainerTabs(1),
    0,
    "0 container tabs at the end with id 1."
  );
  is(
    ContextualIdentityService.countContainerTabs(2),
    0,
    "0 container tabs at the end with id 2."
  );
});
