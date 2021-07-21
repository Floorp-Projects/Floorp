/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

add_task(async function test_enroll_newNewtabExperience() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");
  let browser = tab.linkedBrowser;

  // Wait for React to render something
  await BrowserTestUtils.waitForCondition(
    () =>
      SpecialPowers.spawn(
        browser,
        [],
        () => content.document.getElementById("root").children.length
      ),
    "Should render activity stream content"
  );

  await SpecialPowers.spawn(browser, [], () => {
    let newtabExperience = content.document.querySelector(".newtab-experience");
    ok(newtabExperience, "Newtab experience is on by default");
  });

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "newtab",
    value: {
      enabled: true,
      newNewtabExperienceEnabled: false,
      customizationMenuEnabled: false,
    },
  });

  await SpecialPowers.spawn(browser, [], () => {
    let newtabExperience = content.document.querySelector(".newtab-experience");
    ok(!newtabExperience, "Newtab experience deactive");
  });

  await doExperimentCleanup();

  await SpecialPowers.spawn(browser, [], () => {
    let newtabExperience = content.document.querySelector(".newtab-experience");
    ok(newtabExperience, "Newtab experience is on again");
  });

  BrowserTestUtils.removeTab(tab);
});
