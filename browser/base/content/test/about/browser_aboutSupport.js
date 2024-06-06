/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:support" },
    async function (browser) {
      let keyLocationServiceGoogleStatus = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          let textBox = content.document.getElementById(
            "key-location-service-google-box"
          );
          await ContentTaskUtils.waitForCondition(
            () => content.document.l10n.getAttributes(textBox).id,
            "Google location service API key status loaded"
          );
          return content.document.l10n.getAttributes(textBox).id;
        }
      );
      ok(
        keyLocationServiceGoogleStatus,
        "Google location service API key status shown"
      );

      let keySafebrowsingGoogleStatus = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          let textBox = content.document.getElementById(
            "key-safebrowsing-google-box"
          );
          await ContentTaskUtils.waitForCondition(
            () => content.document.l10n.getAttributes(textBox).id,
            "Google Safebrowsing API key status loaded"
          );
          return content.document.l10n.getAttributes(textBox).id;
        }
      );
      ok(
        keySafebrowsingGoogleStatus,
        "Google Safebrowsing API key status shown"
      );

      let keyMozillaStatus = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          let textBox = content.document.getElementById("key-mozilla-box");
          await ContentTaskUtils.waitForCondition(
            () => content.document.l10n.getAttributes(textBox).id,
            "Mozilla API key status loaded"
          );
          return content.document.l10n.getAttributes(textBox).id;
        }
      );
      ok(keyMozillaStatus, "Mozilla API key status shown");
    }
  );
});

add_task(async function test_nimbus_experiments() {
  await ExperimentAPI.ready();
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "aboutwelcome",
    value: { enabled: true },
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:support" },
    async function (browser) {
      let experimentName = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          await ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector(
                "#remote-experiments-tbody tr:first-child td"
              )?.innerText
          );
          return content.document.querySelector(
            "#remote-experiments-tbody tr:first-child td"
          ).innerText;
        }
      );
      ok(
        experimentName.match("Nimbus"),
        "Rendered the expected experiment slug"
      );
    }
  );

  doExperimentCleanup();
});

add_task(async function test_remote_configuration() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.aboutwelcome.featureId,
      value: { enabled: true },
    },
    { isRollout: true }
  );

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:support" },
    async function (browser) {
      let [userFacingName, branch] = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          await ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector(
                "#remote-features-tbody tr:first-child td"
              )?.innerText
          );
          let rolloutName = content.document.querySelector(
            "#remote-features-tbody tr:first-child td"
          ).innerText;
          let branchName = content.document.querySelector(
            "#remote-features-tbody tr:first-child td:nth-child(2)"
          ).innerText;

          return [rolloutName, branchName];
        }
      );
      ok(
        userFacingName.match("NimbusTestUtils recipe"),
        `Rendered the expected rollout ${userFacingName}`
      );
      ok(branch.match("control"), "Rendered the expected rollout branch");
    }
  );

  doCleanup();
});

add_task(async function test_remote_settings() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(RemoteSettings, "inspect").resolves({
    isSynchronizationBroken: false,
    lastCheck: 1715698289,
    localTimestamp: '"1715698176626"',
    history: {
      "settings-sync": [
        { status: "SUCCESS", datetime: "2024-05-14T14:49:36.626Z", infos: {} },
      ],
    },
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:support" },
    async browser => {
      const localTimestamp = await SpecialPowers.spawn(
        browser,
        [],
        async () => {
          const sel = "#support-remote-settings-local-timestamp";
          await ContentTaskUtils.waitForCondition(
            () => content.document.querySelector(sel)?.innerText
          );
          return content.document.querySelector(sel).innerText;
        }
      );
      Assert.equal(
        localTimestamp,
        '"1715698176626"',
        "Rendered the local timestamp"
      );
    }
  );

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});
