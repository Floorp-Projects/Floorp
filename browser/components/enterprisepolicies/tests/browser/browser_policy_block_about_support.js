/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_setup(async function() {
  await setupPolicyEngineWithJson({
    policies: {
      BlockAboutSupport: true,
    },
  });
});

add_task(async function test_help_menu() {
  buildHelpMenu();
  let troubleshootingInfoMenu = document.getElementById("troubleShooting");
  is(
    troubleshootingInfoMenu.getAttribute("disabled"),
    "true",
    "The `More Troubleshooting Information` item should be disabled"
  );
});

add_task(async function test_about_memory() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:memory"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    let aboutSupportLink = content.document.querySelector(
      "a[href='about:support']"
    );

    Assert.ok(
      !aboutSupportLink,
      "The link to about:support at the bottom of the page should not exist"
    );
  });

  await BrowserTestUtils.removeTab(tab);
});
