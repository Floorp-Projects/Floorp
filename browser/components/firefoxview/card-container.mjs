/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  classMap,
  html,
  ifDefined,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * A collapsible card container to be used throughout Firefox View
 *
 * @property {string} sectionLabel - The aria-label used for the section landmark if the header is hidden with hideHeader
 * @property {boolean} hideHeader - Optional property given if the card container should not display a header
 * @property {boolean} isInnerCard - Optional property given if the card a nested card within another card and given a border rather than box-shadow
 * @property {boolean} preserveCollapseState - Whether or not the expanded/collapsed state should persist
 * @property {string} shortPageName - Page name that the 'View all' link will navigate to and the preserveCollapseState pref will use
 * @property {boolean} showViewAll - True if you need to display a 'View all' header link to navigate
 */
class CardContainer extends MozLitElement {
  constructor() {
    super();
    this.isExpanded = true;
  }

  static properties = {
    sectionLabel: { type: String },
    hideHeader: { type: Boolean },
    isExpanded: { type: Boolean },
    isInnerCard: { type: Boolean },
    preserveCollapseState: { type: Boolean },
    shortPageName: { type: String },
    showViewAll: { type: Boolean },
  };

  static queries = {
    detailsEl: "details",
    mainSlot: "slot[name=main]",
    summaryEl: "summary",
    viewAllLink: ".view-all-link",
  };

  get detailsExpanded() {
    return this.detailsEl.hasAttribute("open");
  }

  connectedCallback() {
    super.connectedCallback();
    if (this.preserveCollapseState && this.shortPageName) {
      this.openStatePref = `browser.tabs.firefox-view.ui-state.${this.shortPageName}.open`;
      this.isExpanded = Services.prefs.getBoolPref(this.openStatePref, true);
    }
  }

  disconnectedCallback() {}

  onToggleContainer() {
    this.isExpanded = this.detailsExpanded;
    if (this.preserveCollapseState) {
      Services.prefs.setBoolPref(this.openStatePref, this.isExpanded);
    }
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/card-container.css"
      />
      <section
        aria-labelledby="header"
        aria-label=${ifDefined(this.sectionLabel)}
      >
        <details
          class=${classMap({ "card-container": true, inner: this.isInnerCard })}
          ?open=${this.isExpanded}
          @toggle=${this.onToggleContainer}
        >
          <summary
            id="header"
            class="card-container-header"
            ?hidden=${ifDefined(this.hideHeader)}
            ?withViewAll=${this.showViewAll}
          >
            <span
              class="icon chevron-icon"
              aria-role="presentation"
              data-l10n-id="firefoxview-collapse-button-${this.isExpanded
                ? "hide"
                : "show"}"
            ></span>
            <slot name="header"></slot>
          </summary>
          <a
            href="about:firefoxview-next#${this.shortPageName}"
            class="view-all-link"
            data-l10n-id="firefoxview-view-all-link"
            ?hidden=${!this.showViewAll}
          ></a>
          <slot name="main"></slot>
          <slot name="footer" class="card-container-footer"></slot>
        </details>
      </section>
    `;
  }
}
customElements.define("card-container", CardContainer);
