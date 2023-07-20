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
 * @property {string} viewAllPage - The location hash for the 'View all' header link to navigate to
 */
class CardContainer extends MozLitElement {
  constructor() {
    super();
    this.isExpanded = true;
  }

  static properties = {
    sectionLabel: { type: String },
    hideHeader: { type: Boolean },
    isInnerCard: { type: Boolean },
    preserveCollapseState: { type: Boolean },
    viewAllPage: { type: String },
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
    if (this.preserveCollapseState && this.viewAllPage) {
      this.openStatePref = `browser.tabs.firefox-view.ui-state.${this.viewAllPage}.open`;
      this.isExpanded = Services.prefs.getBoolPref(this.openStatePref, true);
    }
  }

  disconnectedCallback() {}

  onToggleContainer() {
    this.isExpanded = this.detailsExpanded;
    if (this.preserveCollapseState && this.viewAllPage) {
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
            ?withViewAll=${ifDefined(this.viewAllPage)}
          >
            <span class="icon chevron-icon" aria-role="presentation"></span>
            <slot name="header"></slot>
          </summary>
          <a
            href="about:firefoxview-next#${this.viewAllPage}"
            class="view-all-link"
            data-l10n-id="firefoxview-view-all-link"
            ?hidden=${!this.viewAllPage}
          ></a>
          <slot name="main"></slot>
          <slot name="footer" class="card-container-footer"></slot>
        </details>
      </section>
    `;
  }
}
customElements.define("card-container", CardContainer);
