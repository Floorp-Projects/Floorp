/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_HTTP_DOMAIN = "https://example.org/";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);

const TEST_NORMAL = `${TEST_ROOT}dummy_page.html`;
const TEST_COOP_COEP = `${TEST_ROOT}file_coop_coep.html`;

// Test for bug 1378377.
add_task(async function () {
  // Set prefs to ensure file content process.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.remote.separateFileUriProcess", true]],
  });

  await BrowserTestUtils.withNewTab(TEST_NORMAL, async function (fileBrowser) {
    ok(
      E10SUtils.isWebRemoteType(fileBrowser.remoteType),
      "Check that tab normally has web remote type."
    );

    await SpecialPowers.spawn(fileBrowser, [], () => {
      ok(
        !content.crossOriginIsolated,
        "Tab content is not cross-origin isolated"
      );
    });
  });

  await BrowserTestUtils.withNewTab(
    TEST_COOP_COEP,
    async function (fileBrowser) {
      ok(
        fileBrowser.remoteType.startsWith(
          E10SUtils.WEB_REMOTE_COOP_COEP_TYPE_PREFIX
        ),
        "Check that COOP+COEP tab normally has webCOOP+COEP remote type."
      );
      await SpecialPowers.spawn(fileBrowser, [], () => {
        ok(content.crossOriginIsolated, "Tab content is cross-origin isolated");
      });
    }
  );

  // Set prefs to whitelist TEST_HTTP for file:// URI use.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["capability.policy.policynames", "allowFileURI"],
      ["capability.policy.allowFileURI.sites", TEST_HTTP_DOMAIN],
      ["capability.policy.allowFileURI.checkloaduri.enabled", "allAccess"],
    ],
  });

  await BrowserTestUtils.withNewTab(TEST_NORMAL, async function (fileBrowser) {
    is(
      fileBrowser.remoteType,
      E10SUtils.FILE_REMOTE_TYPE,
      "Check that tab now has file remote type."
    );
    await SpecialPowers.spawn(fileBrowser, [], () => {
      ok(
        !content.crossOriginIsolated,
        "Tab content is not cross-origin isolated"
      );
    });
  });

  await BrowserTestUtils.withNewTab(
    TEST_COOP_COEP,
    async function (fileBrowser) {
      is(
        fileBrowser.remoteType,
        E10SUtils.FILE_REMOTE_TYPE,
        "Check that tab now has file remote type."
      );
      await SpecialPowers.spawn(fileBrowser, [], () => {
        ok(
          !content.crossOriginIsolated,
          "Tab content is not cross-origin isolated"
        );
      });
    }
  );
});
