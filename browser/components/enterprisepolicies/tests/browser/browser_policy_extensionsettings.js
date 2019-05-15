/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const BASE_URL = "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser/";

/**
 * Wait for the given PopupNotification to display
 *
 * @param {string} name
 *        The name of the notification to wait for.
 *
 * @returns {Promise}
 *          Resolves with the notification window.
 */
function promisePopupNotificationShown(name) {
  return new Promise(resolve => {
    function popupshown() {
      let notification = PopupNotifications.getNotification(name);
      if (!notification) { return; }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstElementChild);
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

add_task(async function test_install_source_blocked_link() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://blocks.other.install.sources/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-install-origin-blocked");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("policytest").click();
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_install_source_blocked_installtrigger() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://blocks.other.install.sources/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-install-origin-blocked");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("policytest_installtrigger").click();
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_install_source_blocked_otherdomain() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://mochi.test/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-install-origin-blocked");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("policytest_otherdomain").click();
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_install_source_blocked_direct() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://blocks.other.install.sources/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-install-origin-blocked");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {baseUrl: BASE_URL}, async function({baseUrl}) {
    content.document.location.href = baseUrl + "policytest_v0.1.xpi";
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_install_source_allowed_link() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://mochi.test/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("policytest").click();
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_install_source_allowed_installtrigger() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://mochi.test/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("policytest_installtrigger").click();
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_install_source_allowed_otherdomain() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://mochi.test/*", "http://example.org/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("policytest_otherdomain").click();
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_install_source_allowed_direct() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "*": {
          "install_sources": ["http://mochi.test/*"],
        },
      },
    },
  });
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
    opening: BASE_URL + "extensionsettings.html",
    waitForStateStop: true});

  await ContentTask.spawn(tab.linkedBrowser, {baseUrl: BASE_URL}, async function({baseUrl}) {
    content.document.location.href = baseUrl + "policytest_v0.1.xpi";
  });
  await popupPromise;
  BrowserTestUtils.removeTab(tab);
});
