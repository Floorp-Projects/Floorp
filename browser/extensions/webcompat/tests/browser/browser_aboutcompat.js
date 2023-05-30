"use strict";

add_task(async function test_about_compat_loads_properly() {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:compat",
    waitForLoad: true,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    ok(
      content.document.querySelectorAll("#overrides tr[data-id]").length > 1,
      "UA overrides are listed"
    );
    ok(
      content.document.querySelectorAll("#interventions tr[data-id]").length >
        1,
      "interventions are listed"
    );
    ok(
      content.document.querySelectorAll("#smartblock tr[data-id]").length > 1,
      "SmartBlock shims are listed"
    );
  });

  await BrowserTestUtils.removeTab(tab);
});
