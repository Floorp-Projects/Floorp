"use strict";

add_task(async function test_about_compat_loads_properly() {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:compat",
    waitForLoad: true,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
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
