"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

add_task(async function test_multistage_aboutwelcome_newtab_urlbar_focus() {
  const sandbox = sinon.createSandbox();

  const TEST_CONTENT = [
    {
      id: "TEST_SCREEN",
      content: {
        position: "split",
        logo: {},
        title: "Test newtab url focus",
        primary_button: {
          label: "Next",
          action: {
            navigate: true,
          },
        },
      },
    },
  ];
  await setAboutWelcomePref(true);
  await ExperimentAPI.ready();
  await pushPrefs(["browser.aboutwelcome.newtabUrlBarFocus", true]);

  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: {
      id: "my-mochitest-experiment",
      screens: TEST_CONTENT,
    },
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  const browser = tab.linkedBrowser;
  let focused = BrowserTestUtils.waitForEvent(gURLBar.inputField, "focus");
  await onButtonClick(browser, "button.primary");
  await focused;
  Assert.ok(gURLBar.focused, "focus should be on url bar");

  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
    sandbox.restore();
  });
  await doExperimentCleanup();
});
