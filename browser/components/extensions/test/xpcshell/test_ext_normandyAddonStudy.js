"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

const { AddonStudies } = ChromeUtils.import(
  "resource://normandy/lib/AddonStudies.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
var { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const { addonStudyFactory } = NormandyTestUtils.factories;

AddonTestUtils.init(this);

// All tests run privileged unless otherwise specified not to.
function createExtension(backgroundScript, permissions, isPrivileged = true) {
  let extensionData = {
    background: backgroundScript,
    manifest: {
      applications: {
        gecko: {
          id: "test@shield.mozilla.com",
        },
      },
      permissions,
    },
    isPrivileged,
  };
  return ExtensionTestUtils.loadExtension(extensionData);
}

async function run(test) {
  let extension = createExtension(
    test.backgroundScript,
    test.permissions || ["normandyAddonStudy"],
    test.isPrivileged
  );
  const promiseValidation = test.validationScript
    ? test.validationScript(extension)
    : Promise.resolve();

  await extension.startup();

  await promiseValidation;

  if (test.doneSignal) {
    await extension.awaitFinish(test.doneSignal);
  }

  await extension.unload();
}

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(
  async function test_normandyAddonStudy_without_normandyAddonStudy_permission_privileged() {
    await run({
      backgroundScript: () => {
        browser.test.assertTrue(
          !browser.normandyAddonStudy,
          "'normandyAddonStudy' permission is required"
        );
        browser.test.notifyPass("normandyAddonStudy_permission");
      },
      permissions: [],
      doneSignal: "normandyAddonStudy_permission",
    });
  }
);

add_task(async function test_normandyAddonStudy_without_privilege() {
  await run({
    backgroundScript: () => {
      browser.test.assertTrue(
        !browser.normandyAddonStudy,
        "Extension must be privileged"
      );
      browser.test.notifyPass("normandyAddonStudy_permission");
    },
    isPrivileged: false,
    doneSignal: "normandyAddonStudy_permission",
  });
});

add_task(async function test_getStudy_works() {
  const study = addonStudyFactory({
    addonId: "test@shield.mozilla.com",
  });

  const testWrapper = AddonStudies.withStudies([study]);
  const test = testWrapper(async () => {
    await run({
      backgroundScript: async () => {
        const result = await browser.normandyAddonStudy.getStudy();
        browser.test.sendMessage("study", result);
      },
      validationScript: async extension => {
        let studyResult = await extension.awaitMessage("study");
        deepEqual(
          studyResult,
          study,
          "normandyAddonStudy.getStudy returns the correct study"
        );
      },
    });
  });

  await test();
});

add_task(async function test_endStudy_works() {
  const study = addonStudyFactory({
    addonId: "test@shield.mozilla.com",
  });

  const testWrapper = AddonStudies.withStudies([study]);
  const test = testWrapper(async () => {
    await run({
      backgroundScript: async () => {
        await browser.normandyAddonStudy.endStudy("test");
      },
      validationScript: async () => {
        // Check that `AddonStudies.markAsEnded` was called
        await TestUtils.topicObserved(
          "shield-study-ended",
          (subject, message) => {
            return message === `${study.recipeId}`;
          }
        );

        const addon = await AddonManager.getAddonByID(study.addonId);
        equal(addon, undefined, "Addon should be uninstalled.");
      },
    });
  });

  await test();
});

add_task(async function test_getClientMetadata_works() {
  const study = addonStudyFactory({
    addonId: "test@shield.mozilla.com",
    slug: "test-slug",
    branch: "test-branch",
  });

  const testWrapper = AddonStudies.withStudies([study]);
  const test = testWrapper(async () => {
    await run({
      backgroundScript: async () => {
        const metadata = await browser.normandyAddonStudy.getClientMetadata();
        browser.test.sendMessage("clientMetadata", metadata);
      },
      validationScript: async extension => {
        let clientMetadata = await extension.awaitMessage("clientMetadata");

        ok(
          clientMetadata.updateChannel ===
            Services.appinfo.defaultUpdateChannel,
          "clientMetadata contains correct updateChannel"
        );

        ok(
          clientMetadata.fxVersion === Services.appinfo.version,
          "clientMetadata contains correct fxVersion"
        );

        ok("clientID" in clientMetadata, "clientMetadata contains a clientID");
      },
    });
  });

  await test();
});

add_task(async function test_onUnenroll_works() {
  const study = addonStudyFactory({
    addonId: "test@shield.mozilla.com",
  });

  const testWrapper = AddonStudies.withStudies([study]);
  const test = testWrapper(async () => {
    await run({
      backgroundScript: () => {
        browser.normandyAddonStudy.onUnenroll.addListener(reason => {
          browser.test.sendMessage("unenrollReason", reason);
        });
        browser.test.sendMessage("bgpageReady");
      },
      validationScript: async extension => {
        await extension.awaitMessage("bgpageReady");
        await AddonStudies.markAsEnded(study, "test");
        const unenrollReason = await extension.awaitMessage("unenrollReason");
        equal(unenrollReason, "test", "Unenroll listener should be called.");
      },
    });
  });

  await test();
});
