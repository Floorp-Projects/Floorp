/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { html } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-toggle.mjs";

import { FAKESPOT_BASE_URL } from "chrome://global/content/shopping/ProductConfig.mjs";

class ShoppingSettings extends MozLitElement {
  static properties = {
    adsEnabledByUser: { type: Boolean },
  };

  static get queries() {
    return {
      recommendationsToggleEl: "#shopping-settings-recommendations-toggle",
      optOutButtonEl: "#shopping-settings-opt-out-button",
      shoppingCardEl: "shopping-card",
    };
  }

  onToggleRecommendations() {
    this.adsEnabledByUser = this.recommendationsToggleEl.pressed;
    RPMSetPref(
      "browser.shopping.experience2023.ads.userEnabled",
      this.adsEnabledByUser
    );
  }

  onDisableShopping() {
    RPMSetPref("browser.shopping.experience2023.optedIn", 2);
    RPMSetPref("browser.shopping.experience2023.active", false);
  }

  fakespotLinkClicked(e) {
    if (e.target.localName == "a" && e.button == 0) {
      this.dispatchEvent(
        new CustomEvent("ShoppingTelemetryEvent", {
          composed: true,
          bubbles: true,
          detail: "surfacePoweredByFakespotLinkClicked",
        })
      );
    }
  }

  render() {
    // Whether we show recommendations at all (including offering a user
    // control for them) is controlled via a nimbus-enabled pref.
    let canShowRecommendationToggle = RPMGetBoolPref(
      "browser.shopping.experience2023.ads.enabled"
    );
    let toggleMarkup = canShowRecommendationToggle
      ? html`
        <moz-toggle
          id="shopping-settings-recommendations-toggle"
          ?pressed=${this.adsEnabledByUser}
          data-l10n-id="shopping-settings-recommendations-toggle"
          data-l10n-attrs="label"
          @toggle=${this.onToggleRecommendations}>
        </moz-toggle/>
        <span id="shopping-ads-learn-more" data-l10n-id="shopping-settings-recommendations-learn-more">
          <a
            is="moz-support-link"
            support-page="todo"
            data-l10n-name="review-quality-url"
          ></a>
        </span>`
      : null;

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/settings.css"
      />
      <shopping-card
        data-l10n-id="shopping-settings-label"
        data-l10n-attrs="label"
        type="accordion"
      >
        <div id="shopping-settings-wrapper" slot="content">
          ${toggleMarkup}
          <button
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
          data-l10n-name="fakespot-link"
          target="_blank"
          href="${FAKESPOT_BASE_URL}"
        ></a>
      </p>
    `;
  }
}

customElements.define("shopping-settings", ShoppingSettings);
