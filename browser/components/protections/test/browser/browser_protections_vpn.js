/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  Region: "resource://gre/modules/Region.jsm",
});

const { AboutProtectionsParent } = ChromeUtils.import(
  "resource:///actors/AboutProtectionsParent.jsm"
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.monitor.enabled", false],
      ["browser.contentblocking.report.lockwise.enabled", false],
      ["browser.contentblocking.report.vpn.enabled", true],
    ],
  });
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false, "us"));
  const avLocales = Services.locale.availableLocales;

  registerCleanupFunction(() => {
    Services.locale.availableLocales = avLocales;
  });
});

add_task(async function testVPNCardVisibility() {
  if (Services.sysinfo.getProperty("name") != "Windows_NT") {
    ok(true, "User is on an unsupported platform, the vpn card will not show");
    return;
  }
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false, "my"));
  Region._setHomeRegion("my", false);

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Enable showing the VPN card");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.vpn.enabled", true],
      ["browser.contentblocking.report.vpn_regions", "us,ca,nz,sg,my,gb"],
      ["browser.contentblocking.report.vpn_platforms", "win"],
    ],
  });

  info("Check that vpn card is hidden if user's language is not en*");
  Services.locale.availableLocales = ["ko-KR", "ar"];
  Services.locale.requestedLocales = ["ko-KR"];
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  info("Check that vpn card is shown if user's language is en*");
  // Set language back to en-US
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, false);

  info(
    "Check that vpn card is hidden if user's location is not on the regions list."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false, "ls"));
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  info(
    "Check that vpn card shows a different version if user has subscribed to Mozilla vpn."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(true, "us"));
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, false, true);

  info(
    "VPN card should be hidden when vpn not enabled, though all other conditions are true"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.vpn.enabled", false]],
  });
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  await BrowserTestUtils.removeTab(tab);
});

async function checkVPNCardVisibility(tab, shouldBeHidden, subscribed = false) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ _shouldBeHidden: shouldBeHidden, _subscribed: subscribed }],
    async function({ _shouldBeHidden, _subscribed }) {
      await ContentTaskUtils.waitForCondition(() => {
        const vpnCard = content.document.querySelector(".vpn-card");
        const subscribedStateCorrect =
          vpnCard.classList.contains("subscribed") == _subscribed;
        return (
          ContentTaskUtils.is_hidden(vpnCard) === _shouldBeHidden &&
          subscribedStateCorrect
        );
      });

      const visibilityState = _shouldBeHidden ? "hidden" : "shown";
      ok(true, `VPN card is ${visibilityState}.`);
    }
  );
}

add_task(async function testVPNPromoBanner() {
  if (Services.sysinfo.getProperty("name") != "Windows_NT") {
    ok(true, "User is on an unsupported platform, the vpn card will not show");
    return;
  }
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false, "us"));

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Enable showing the VPN card and banner");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.vpn.enabled", true],
      ["browser.contentblocking.report.vpn_regions", "us,ca,nz,sg,my,gb"],
      ["browser.contentblocking.report.vpn_platforms", "win"],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });

  info("Check that vpn banner is hidden if user's language is not en*");
  Services.locale.availableLocales = ["de"];
  Services.locale.requestedLocales = ["de"];
  await reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  // VPN Banner flips this pref each time it shows, flip back between each instruction.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.hide_vpn_banner", false]],
  });

  info("Check that vpn banner is shown if user's language is en*");
  // Set language back to en-US
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];
  await reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, false);

  is(
    Services.prefs.getBoolPref(
      "browser.contentblocking.report.hide_vpn_banner",
      false
    ),
    true,
    "After showing the banner once, the pref to hide the VPN banner is flipped"
  );
  info("The banner does not show when the pref to hide it is flipped");
  await reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  // VPN Banner flips this pref each time it shows, flip back between each instruction.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.hide_vpn_banner", false]],
  });

  info(
    "Check that VPN banner is hidden if user's location is not on the regions list."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false, "ls"));
  await reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  info(
    "VPN banner should be hidden when vpn not enabled, though all other conditions are true"
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.vpn.enabled", false],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });
  await reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.vpn.enabled", true],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });

  info("If user is subscribed to VPN already the promo banner should not show");
  AboutProtectionsParent.setTestOverride(getVPNOverrides(true, "us"));

  await reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  await BrowserTestUtils.removeTab(tab);
});

async function checkVPNPromoBannerVisibility(tab, shouldBeHidden) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ _shouldBeHidden: shouldBeHidden }],
    async function({ _shouldBeHidden }) {
      await ContentTaskUtils.waitForCondition(() => {
        const vpnBanner = content.document.querySelector(".vpn-banner");
        return ContentTaskUtils.is_hidden(vpnBanner) === _shouldBeHidden;
      });

      const visibilityState = _shouldBeHidden ? "hidden" : "shown";
      ok(true, `VPN banner is ${visibilityState}.`);
    }
  );
}

// Expect the vpn card and banner to not show as we are expressly excluding China. Even when cn is in the supported region pref.
add_task(async function testVPNDoesNotShowChina() {
  if (Services.sysinfo.getProperty("name") != "Windows_NT") {
    ok(true, "User is on an unsupported platform, the vpn card will not show");
    return;
  }
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false, "us"));
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Enable showing the VPN card");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.vpn.enabled", true],
      ["browser.contentblocking.report.vpn_regions", "us,ca,nz,sg,my,gb,cn"],
      ["browser.contentblocking.report.vpn_platforms", "win,mac"],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });

  info("Check that vpn banner and card are able to show when conditions allow");
  Region._setHomeRegion("US", false);
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, false);
  await checkVPNPromoBannerVisibility(tab, false);

  // VPN Banner flips this pref each time it shows, flip back between each instruction.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.hide_vpn_banner", false]],
  });

  info(
    "set home location to China, even though user is currently in the US, expect vpn card to be hidden"
  );
  Region._setHomeRegion("CN", false);
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, true);
  await checkVPNPromoBannerVisibility(tab, true);

  // VPN Banner flips this pref each time it shows, flip back between each instruction.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.hide_vpn_banner", false]],
  });

  info("home region is US, but current location is China");
  Region._setHomeRegion("US", false);
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false, "cn"));
  await reloadTab(tab);
  await checkVPNCardVisibility(tab, true);
  await checkVPNPromoBannerVisibility(tab, true);

  await BrowserTestUtils.removeTab(tab);
});
