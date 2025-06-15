"use strict";

add_task(async function test_about_compat_loads_properly() {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:compat",
    waitForLoad: true,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    is(
      content.origin,
      "moz-extension://9a310967-e580-48bf-b3e8-4eafebbc122d",
      "Expected origin of about:compat"
    );

    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("#overrides tr[data-id]"),
      "UA overrides are listed"
    );
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("#interventions tr[data-id]"),
      "interventions are listed"
    );
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("#smartblock tr[data-id]"),
      "SmartBlock shims are listed"
    );
    ok(true, "Interventions are listed");
  });

  await BrowserTestUtils.removeTab(tab);
});
