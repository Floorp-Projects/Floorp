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
  #isRecommendationsEnabled;

  static get queries() {
    return {
      recommendationsToggleEl: "#shopping-settings-recommendations-toggle",
      optOutButtonEl: "#shopping-settings-opt-out-button",
    };
  }

  onToggleRecommendations() {
    // TODO: we will need to hide the recommendations card here
    this.#isRecommendationsEnabled = this.recommendationsToggleEl.pressed;
  }

  onDisableShopping() {
    RPMSetPref("browser.shopping.experience2023.optedIn", 2);
  }

  render() {
    // TODO: for now, we will hardcode the value until a pref is made.
    this.#isRecommendationsEnabled = true;

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
              <moz-toggle id="shopping-settings-recommendations-toggle" pressed=${
                this.#isRecommendationsEnabled
              } data-l10n-id="shopping-settings-recommendations-toggle" data-l10n-attrs="label" @toggle=${
      this.onToggleRecommendations
    }></moz-toggle/>
              <button id="shopping-settings-opt-out-button" data-l10n-id="shopping-settings-opt-out-button" @click=${
                this.onDisableShopping
              }></button>
          </div>
        </shopping-card>
        `;
  }
}

customElements.define("shopping-settings", ShoppingSettings);
