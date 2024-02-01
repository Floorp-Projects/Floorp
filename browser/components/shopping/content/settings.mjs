/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { html } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-toggle.mjs";

class ShoppingSettings extends MozLitElement {
  static properties = {
    adsEnabled: { type: Boolean },
    adsEnabledByUser: { type: Boolean },
  };

  static get queries() {
    return {
      recommendationsToggleEl: "#shopping-settings-recommendations-toggle",
      optOutButtonEl: "#shopping-settings-opt-out-button",
      shoppingCardEl: "shopping-card",
      adsLearnMoreLinkEl: "#shopping-ads-learn-more-link",
      fakespotLearnMoreLinkEl: "#powered-by-fakespot-link",
    };
  }

  onToggleRecommendations() {
    this.adsEnabledByUser = this.recommendationsToggleEl.pressed;
    let action = this.adsEnabledByUser ? "enabled" : "disabled";
    Glean.shopping.surfaceAdsSettingToggled.record({ action });
    RPMSetPref(
      "browser.shopping.experience2023.ads.userEnabled",
      this.adsEnabledByUser
    );
  }

  onDisableShopping() {
    // Unfortunately, order matters here. As soon as we set the optedIn pref
    // to the opted out state, the sidebar gets torn down, and the active pref
    // is never flipped, leaving the toolbar button in the active state.
    RPMSetPref("browser.shopping.experience2023.active", false);
    RPMSetPref("browser.shopping.experience2023.optedIn", 2);
    Glean.shopping.surfaceOptOutButtonClicked.record();
  }

  fakespotLinkClicked(e) {
    if (e.target.localName == "a" && e.button == 0) {
      Glean.shopping.surfacePoweredByFakespotLinkClicked.record();
    }
  }

  render() {
    // Whether we show recommendations at all (including offering a user
    // control for them) is controlled via a nimbus-enabled pref.
    let canShowRecommendationToggle = this.adsEnabled;

    let toggleMarkup = canShowRecommendationToggle
      ? html`
        <moz-toggle
          id="shopping-settings-recommendations-toggle"
          ?pressed=${this.adsEnabledByUser}
          data-l10n-id="shopping-settings-recommendations-toggle"
          data-l10n-attrs="label"
          @toggle=${this.onToggleRecommendations}>
        </moz-toggle/>
        <span id="shopping-ads-learn-more" data-l10n-id="shopping-settings-recommendations-learn-more2">
          <a
            id="shopping-ads-learn-more-link"
            target="_blank"
            href="${window.RPMGetFormatURLPref(
              "app.support.baseURL"
            )}review-checker-review-quality?utm_campaign=learn-more&utm_medium=inproduct&utm_term=core-sidebar#w_ads_for_relevant_products"
            data-l10n-name="review-quality-url"
          ></a>
        </span>`
      : null;

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/settings.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-page.css"
      />
      <shopping-card
        data-l10n-id="shopping-settings-label"
        data-l10n-attrs="label"
        type="accordion"
      >
        <div id="shopping-settings-wrapper" slot="content">
          ${toggleMarkup}
          <button
            class="shopping-button"
            id="shopping-settings-opt-out-button"
            data-l10n-id="shopping-settings-opt-out-button"
            @click=${this.onDisableShopping}
          ></button>
        </div>
      </shopping-card>
      <p
        id="powered-by-fakespot"
        class="deemphasized"
        data-l10n-id="powered-by-fakespot"
        @click=${this.fakespotLinkClicked}
      >
        <a
          id="powered-by-fakespot-link"
          data-l10n-name="fakespot-link"
          target="_blank"
          href="https://www.fakespot.com/our-mission?utm_source=review-checker&utm_campaign=fakespot-by-mozilla&utm_medium=inproduct&utm_term=core-sidebar"
        ></a>
      </p>
    `;
  }
}

customElements.define("shopping-settings", ShoppingSettings);
