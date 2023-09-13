"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_TOPLEVEL_URI = TEST_PATH + "auto_upgrading_identity.html";

// auto upgrading mixed content should not indicate passive mixed content loaded
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", true],
      ["security.mixed_content.upgrade_display_content.image", true],
    ],
  });
  await BrowserTestUtils.withNewTab(
    TEST_TOPLEVEL_URI,
    async function (browser) {
      await ContentTask.spawn(browser, {}, async function () {
        let testImg = content.document.getElementById("testimage");
        ok(
          testImg.src.includes("auto_upgrading_identity.png"),
          "sanity: correct image is loaded"
        );
      });
      // Ensure the identiy handler does not show mixed content!
      ok(
        !gIdentityHandler._isMixedPassiveContentLoaded,
        "Auto-Upgrading Mixed Content: Identity should note indicate mixed content"
      );
    }
  );
});

// regular mixed content test should indicate passive mixed content loaded
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["security.mixed_content.upgrade_display_content", false]],
  });
  await BrowserTestUtils.withNewTab(
    TEST_TOPLEVEL_URI,
    async function (browser) {
      await ContentTask.spawn(browser, {}, async function () {
        let testImg = content.document.getElementById("testimage");
        ok(
          testImg.src.includes("auto_upgrading_identity.png"),
          "sanity: correct image is loaded"
        );
      });
      // Ensure the identiy handler does show mixed content!
      ok(
        gIdentityHandler._isMixedPassiveContentLoaded,
        "Regular Mixed Content: Identity should indicate mixed content"
      );
    }
  );
});
