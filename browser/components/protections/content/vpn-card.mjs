/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
export default class VPNCard {
  constructor(document) {
    this.doc = document;
  }

  init() {
    const vpnLink = this.doc.getElementById("get-vpn-link");
    const vpnBannerLink = this.doc.getElementById("vpn-banner-link");
    vpnLink.href = RPMGetStringPref(
      "browser.contentblocking.report.vpn.url",
      ""
    );
    vpnBannerLink.href = RPMGetStringPref(
      "browser.contentblocking.report.vpn-promo.url",
      ""
    );

    vpnLink.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "vpn_card_link");
    });
    let androidVPNAppLink = document.getElementById(
      "vpn-google-playstore-link"
    );
    androidVPNAppLink.href = RPMGetStringPref(
      "browser.contentblocking.report.vpn-android.url"
    );
    androidVPNAppLink.addEventListener("click", () => {
      document.sendTelemetryEvent("click", "vpn_app_link_android");
    });
    let iosVPNAppLink = document.getElementById("vpn-app-store-link");
    iosVPNAppLink.href = RPMGetStringPref(
      "browser.contentblocking.report.vpn-ios.url"
    );
    iosVPNAppLink.addEventListener("click", () => {
      document.sendTelemetryEvent("click", "vpn_app_link_ios");
    });

    const vpnBanner = this.doc.querySelector(".vpn-banner");
    const exitIcon = vpnBanner.querySelector(".exit-icon");
    vpnBannerLink.addEventListener("click", () => {
      this.doc.sendTelemetryEvent("click", "vpn_banner_link");
    });
    // User has closed the vpn banner, hide it.
    exitIcon.addEventListener("click", () => {
      vpnBanner.classList.add("hidden");
      this.doc.sendTelemetryEvent("click", "vpn_banner_close");
    });

    this.showVPNCard();
  }

  // Show the VPN card if user is located in areas, and on platforms, it serves
  async showVPNCard() {
    const showVPNBanner = this.showVPNBanner.bind(this);
    RPMSendQuery("FetchShowVPNCard", {}).then(shouldShow => {
      if (!shouldShow) {
        return;
      }
      const vpnCard = this.doc.querySelector(".vpn-card");

      // add 'subscribed' class if user is subscribed to vpn
      RPMSendQuery("FetchVPNSubStatus", {}).then(async hasVPN => {
        if (hasVPN) {
          vpnCard.classList.add("subscribed");
          vpnCard
            .querySelector(".card-title")
            .setAttribute("data-l10n-id", "vpn-title-subscribed");

          // hide the promo banner if the user is already subscribed to vpn
          await RPMSetBoolPref(
            "browser.contentblocking.report.hide_vpn_banner",
            true
          );
        }

        vpnCard.classList.remove("hidden");
        showVPNBanner();
      });
    });
  }

  showVPNBanner() {
    if (
      RPMGetBoolPref("browser.contentblocking.report.hide_vpn_banner", false) ||
      !RPMGetBoolPref("browser.vpn_promo.enabled", false)
    ) {
      return;
    }

    const vpnBanner = this.doc.querySelector(".vpn-banner");
    vpnBanner.classList.remove("hidden");
    this.doc.sendTelemetryEvent("show", "vpn_banner");
    // VPN banner only shows on the first visit, flip a pref so it does not show again.
    RPMSetBoolPref("browser.contentblocking.report.hide_vpn_banner", true);
  }
}
