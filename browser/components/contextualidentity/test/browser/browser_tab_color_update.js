/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = BrowserTestUtils.addTab(gBrowser, uri, { userContextId });

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser, false, uri);
  return { tab };
}

/**
 * When a container's colour changes, the tab colour should update.
 */
add_task(async function test_tab_color_updates() {
  const kId = 2;
  let { tab } = await openTabInUserContext("https://example.com/", kId);
  let contextIdInfo = ContextualIdentityService.getPublicIdentityFromId(kId);
  ok(
    tab.classList.contains("identity-color-" + contextIdInfo.color),
    `Should use color ${contextIdInfo.color} for tab`
  );

  // The container has a localized name which isn't in the contextIdInfo object.
  // We need to pass a valid name for the update to go through, so make one up.
  let name = "Foo";
  // Don't hardcode a colour so changes in defaults won't cause the following
  // test to fail or be a no-op.
  let otherColor = contextIdInfo.color == "green" ? "orange" : "green";
  registerCleanupFunction(() => ContextualIdentityService.resetDefault());
  ContextualIdentityService.update(kId, name, contextIdInfo.icon, otherColor);

  ok(
    tab.classList.contains("identity-color-" + otherColor),
    `Should use color ${otherColor} for tab after update`
  );

  BrowserTestUtils.removeTab(tab);
});
