/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AboutProtectionsParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutProtectionsParent.sys.mjs"
);

let { Region } = ChromeUtils.importESModule(
  "resource://gre/modules/Region.sys.mjs"
);

const initialHomeRegion = Region._home;
const initialCurrentRegion = Region._current;

async function checkVPNCardVisibility(tab, shouldBeHidden, subscribed = false) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ _shouldBeHidden: shouldBeHidden, _subscribed: subscribed }],
    async function ({ _shouldBeHidden, _subscribed }) {
      await ContentTaskUtils.waitForCondition(() => {
        const vpnCard = content.document.querySelector(".vpn-card");
        const subscribedStateCorrect =
          vpnCard.classList.contains("subscribed") == _subscribed;
        return (
          ContentTaskUtils.isHidden(vpnCard) === _shouldBeHidden &&
          subscribedStateCorrect
        );
      });

      const visibilityState = _shouldBeHidden ? "hidden" : "shown";
      ok(true, `VPN card is ${visibilityState}.`);
    }
  );
}

async function checkVPNPromoBannerVisibility(tab, shouldBeHidden) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ _shouldBeHidden: shouldBeHidden }],
    async function ({ _shouldBeHidden }) {
      await ContentTaskUtils.waitForCondition(() => {
        const vpnBanner = content.document.querySelector(".vpn-banner");
        return ContentTaskUtils.isHidden(vpnBanner) === _shouldBeHidden;
      });

      const visibilityState = _shouldBeHidden ? "hidden" : "shown";
      ok(true, `VPN banner is ${visibilityState}.`);
    }
  );
}

async function setCurrentRegion(region) {
  Region._setCurrentRegion(region);
}

async function setHomeRegion(region) {
  // _setHomeRegion sets a char pref to the value of region. A non-string value will result in an error, so default to an empty string when region is falsey.
  Region._setHomeRegion(region || "");
}

async function revertRegions() {
  setCurrentRegion(initialCurrentRegion);
  setHomeRegion(initialHomeRegion);
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.monitor.enabled", false],
      ["browser.contentblocking.report.lockwise.enabled", false],
      ["browser.vpn_promo.enabled", true],
    ],
  });
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  setCurrentRegion("us");
  const avLocales = Services.locale.availableLocales;

  registerCleanupFunction(() => {
    Services.locale.availableLocales = avLocales;
  });
});

add_task(async function testVPNCardVisibility() {
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  await promiseSetHomeRegion("us");
  setCurrentRegion("us");

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Enable showing the VPN card");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.vpn_promo.enabled", true],
      ["browser.contentblocking.report.vpn_regions", "us,ca,nz,sg,my,gb"],
      [
        "browser.vpn_promo.disallowed_regions",
        "ae,by,cn,cu,iq,ir,kp,om,ru,sd,sy,tm,tr",
      ],
    ],
  });

  info(
    "Check that vpn card is hidden if neither the user's home nor current location is on the regions list."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  setCurrentRegion("ls");
  await promiseSetHomeRegion("ls");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  info(
    "Check that vpn card is hidden if user's location is in the list of disallowed regions."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  setCurrentRegion("sy");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  info(
    "Check that vpn card shows a different version if user has subscribed to Mozilla vpn."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(true));
  setCurrentRegion("us");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNCardVisibility(tab, false, true);

  info(
    "VPN card should be hidden when vpn not enabled, though all other conditions are true"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", false]],
  });
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  await BrowserTestUtils.removeTab(tab);
  revertRegions();
});

add_task(async function testVPNPromoBanner() {
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));

  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Enable showing the VPN card and banner");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.vpn_promo.enabled", true],
      ["browser.contentblocking.report.vpn_regions", "us,ca,nz,sg,my,gb"],
      [
        "browser.vpn_promo.disallowed_regions",
        "ae,by,cn,cu,iq,ir,kp,om,ru,sd,sy,tm,tr",
      ],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });

  info("Check that vpn banner is shown if user's region is supported");
  setCurrentRegion("us");
  await BrowserTestUtils.reloadTab(tab);
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
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  // VPN Banner flips this pref each time it shows, flip back between each instruction.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.hide_vpn_banner", false]],
  });

  info(
    "Check that VPN banner is hidden if user's location is not on the regions list."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  setCurrentRegion("ls");
  await setHomeRegion("ls'");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  info(
    "Check that VPN banner is hidden if user's location is in the disallowed regions list."
  );
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  setCurrentRegion("sy");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  info(
    "VPN banner should be hidden when vpn not enabled, though all other conditions are true"
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.vpn_promo.enabled", false],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.vpn_promo.enabled", true],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });

  info("If user is subscribed to VPN already the promo banner should not show");
  AboutProtectionsParent.setTestOverride(getVPNOverrides(true));
  setCurrentRegion("us");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);

  await BrowserTestUtils.removeTab(tab);
  revertRegions();
});

// Expect the vpn card and banner to not show as we are expressly excluding China. Even when cn is in the supported region pref.
add_task(async function testVPNDoesNotShowChina() {
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  setCurrentRegion("us");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Enable showing the VPN card and banners");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.vpn_promo.enabled", true],
      ["browser.contentblocking.report.vpn_regions", "us,ca,nz,sg,my,gb,cn"],
      [
        "browser.vpn_promo.disallowed_regions",
        "ae,by,cn,cu,iq,ir,kp,om,ru,sd,sy,tm,tr",
      ],
      ["browser.contentblocking.report.hide_vpn_banner", false],
    ],
  });

  info(
    "set home location to China, even though user is currently in the US, expect vpn card to be hidden"
  );
  await promiseSetHomeRegion("CN");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  // VPN Banner flips this pref each time it shows, flip back between each instruction.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.report.hide_vpn_banner", false]],
  });

  info("home region is US, but current location is China");
  AboutProtectionsParent.setTestOverride(getVPNOverrides(false));
  await promiseSetHomeRegion("US");
  setCurrentRegion("CN");
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNPromoBannerVisibility(tab, true);
  await BrowserTestUtils.reloadTab(tab);
  await checkVPNCardVisibility(tab, true);

  await BrowserTestUtils.removeTab(tab);
  revertRegions();
});
