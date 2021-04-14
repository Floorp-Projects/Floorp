/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
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

  let recipe = ExperimentFakes.recipe(`foo${Date.now()}`, {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        feature: {
          featureId: "newtab",
          enabled: true,
          value: {
            newNewtabExperienceEnabled: false,
            customizationMenuEnabled: false,
          },
        },
      },
      {
        slug: "control",
        ratio: 1,
        feature: {
          featureId: "newtab",
          enabled: true,
          value: {
            newNewtabExperienceEnabled: false,
            customizationMenuEnabled: false,
          },
        },
      },
    ],
    bucketConfig: {
      namespace: "mstest-utils",
      randomizationUnit: "normandy_id",
      start: 0,
      count: 1000,
      total: 1000,
    },
  });

  await ExperimentManager.enroll(recipe);

  await BrowserTestUtils.waitForCondition(
    () => ExperimentManager.store.getAllActive().length === 1,
    "Wait for enrollment to finish"
  );

  await SpecialPowers.spawn(browser, [], () => {
    let newtabExperience = content.document.querySelector(".newtab-experience");
    ok(!newtabExperience, "Newtab experience deactive");
  });

  await ExperimentManager.unenroll(recipe.slug, "cleanup");

  await BrowserTestUtils.waitForCondition(
    () => ExperimentManager.store.getAllActive().length === 0,
    "Wait for enrollment to finish"
  );

  await SpecialPowers.spawn(browser, [], () => {
    let newtabExperience = content.document.querySelector(".newtab-experience");
    ok(newtabExperience, "Newtab experience is on again");
  });

  BrowserTestUtils.removeTab(tab);
});
