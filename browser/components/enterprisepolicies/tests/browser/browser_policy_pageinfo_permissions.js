/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ORIGIN = "https://example.com";

/* Verifies that items on the page info page are properly disabled
   when the corresponding policies are locked */
add_task(async function test_pageinfo_permissions() {
  await setupPolicyEngineWithJson({
    policies: {
      Permissions: {
        Camera: {
          BlockNewRequests: true,
          Locked: true,
        },
        Microphone: {
          BlockNewRequests: true,
          Locked: true,
        },
        Location: {
          BlockNewRequests: true,
          Locked: true,
        },
        Notifications: {
          BlockNewRequests: true,
          Locked: true,
        },
        VirtualReality: {
          BlockNewRequests: true,
          Locked: true,
        },
        Autoplay: {
          Default: "block-audio",
          Locked: true,
        },
      },
      InstallAddonsPermission: {
        Default: false,
      },
      PopupBlocking: {
        Locked: true,
      },
      Cookies: {
        Locked: true,
      },
    },
  });

  let permissions = [
    "geo",
    "autoplay-media",
    "install",
    "popup",
    "desktop-notification",
    "cookie",
    "camera",
    "microphone",
    "xr",
  ];

  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function (browser) {
    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "permTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    for (let i = 0; i < permissions.length; i++) {
      let permission = permissions[i];
      let checkbox = await TestUtils.waitForCondition(() =>
        pageInfo.document.getElementById(`${permission}Def`)
      );

      ok(checkbox.disabled, `${permission} checkbox should be disabled`);
    }

    pageInfo.close();
  });
});
