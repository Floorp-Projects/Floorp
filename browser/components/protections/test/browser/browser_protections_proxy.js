/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.monitor.enabled", false],
      ["browser.contentblocking.report.lockwise.enabled", false],
    ],
  });
});

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Secure Proxy card should be hidden by default");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const proxyCard = content.document.querySelector(".proxy-card");
      return !proxyCard["data-enabled"];
    }, "Proxy card is not enabled.");

    const proxyCard = content.document.querySelector(".proxy-card");
    ok(ContentTaskUtils.is_hidden(proxyCard), "Proxy card is hidden.");
  });

  info("Enable showing the Secure Proxy card");
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.proxy.enabled",
    true
  );

  info(
    "Check that secure proxy card is hidden if user's language is not en-US"
  );
  Services.prefs.setCharPref("intl.accept_languages", "en-CA");
  await reloadTab(tab);
  await checkProxyCardVisibility(tab, true);

  info(
    "Check that secure proxy card is shown if user's location is in the US."
  );
  // Set language back to en-US
  Services.prefs.setCharPref("intl.accept_languages", "en-US");
  Services.prefs.setCharPref("browser.search.region", "US");
  await reloadTab(tab);
  await checkProxyCardVisibility(tab, false);

  info(
    "Check that secure proxy card is hidden if user's location is not in the US."
  );
  Services.prefs.setCharPref("browser.search.region", "CA");
  await reloadTab(tab);
  await checkProxyCardVisibility(tab, true);

  info(
    "Check that secure proxy card is hidden if the extension is already installed."
  );
  // Make sure we set the region back to "US"
  Services.prefs.setCharPref("browser.search.region", "US");
  const id = "secure-proxy@mozilla.com";
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id } },
      name: "Firefox Proxy",
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  await reloadTab(tab);
  await checkProxyCardVisibility(tab, true);
  await extension.unload();

  Services.prefs.setBoolPref(
    "browser.contentblocking.report.proxy.enabled",
    false
  );

  await BrowserTestUtils.removeTab(tab);
});

async function checkProxyCardVisibility(tab, shouldBeHidden) {
  await ContentTask.spawn(
    tab.linkedBrowser,
    { _shouldBeHidden: shouldBeHidden },
    async function({ _shouldBeHidden }) {
      await ContentTaskUtils.waitForCondition(() => {
        const proxyCard = content.document.querySelector(".proxy-card");
        return ContentTaskUtils.is_hidden(proxyCard) === _shouldBeHidden;
      });

      const visibilityState = _shouldBeHidden ? "hidden" : "shown";
      ok(true, `Proxy card is ${visibilityState}.`);
    }
  );
}
