/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  classMap,
  html,
  ifDefined,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * A collapsible card container to be used throughout Firefox View
 *
 * @property {string} sectionLabel - The aria-label used for the section landmark if the header is hidden with hideHeader
 * @property {boolean} hideHeader - Optional property given if the card container should not display a header
 * @property {boolean} isEmptyState - Optional property given if the card is used within an empty state
 * @property {boolean} isInnerCard - Optional property given if the card a nested card within another card and given a border rather than box-shadow
 * @property {boolean} preserveCollapseState - Whether or not the expanded/collapsed state should persist
 * @property {string} shortPageName - Page name that the 'View all' link will navigate to and the preserveCollapseState pref will use
 * @property {boolean} showViewAll - True if you need to display a 'View all' header link to navigate
 * @property {boolean} toggleDisabled - Optional property given if the card container should not be collapsible
 */
class CardContainer extends MozLitElement {
  initiallyExpanded = true;

  static properties = {
    sectionLabel: { type: String },
    hideHeader: { type: Boolean },
    isExpanded: { type: Boolean },
    isEmptyState: { type: Boolean },
    isInnerCard: { type: Boolean },
    preserveCollapseState: { type: Boolean },
    shortPageName: { type: String },
    showViewAll: { type: Boolean },
    toggleDisabled: { type: Boolean },
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

  get detailsOpenPrefValue() {
    const prefName = this.shortPageName
      ? `browser.tabs.firefox-view.ui-state.${this.shortPageName}.open`
      : null;
    if (prefName && Services.prefs.prefHasUserValue(prefName)) {
      return Services.prefs.getBoolPref(prefName);
    }
    return null;
  }

  connectedCallback() {
    super.connectedCallback();
    this.isExpanded = this.detailsOpenPrefValue ?? this.initiallyExpanded;
  }

  onToggleContainer() {
    if (this.isExpanded == this.detailsExpanded) {
      return;
    }
    this.isExpanded = this.detailsExpanded;

    if (!this.shortPageName) {
      return;
    }

    if (this.preserveCollapseState) {
      const prefName = this.shortPageName
        ? `browser.tabs.firefox-view.ui-state.${this.shortPageName}.open`
        : null;
      Services.prefs.setBoolPref(prefName, this.isExpanded);
    }

    // Record telemetry
    Services.telemetry.recordEvent(
      "firefoxview_next",
      this.isExpanded ? "card_expanded" : "card_collapsed",
      "card_container",
      null,
      {
        data_type: this.shortPageName,
      }
    );
  }

  viewAllClicked() {
    this.dispatchEvent(
      new CustomEvent("card-container-view-all", {
        bubbles: true,
        composed: true,
      })
    );
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
        ${when(
          this.toggleDisabled,
          () => html`<div
            class=${classMap({
              "card-container": true,
              inner: this.isInnerCard,
              "empty-state": this.isEmptyState && !this.isInnerCard,
            })}
          >
            <span
              id="header"
              class="card-container-header"
              ?hidden=${ifDefined(this.hideHeader)}
              toggleDisabled
              ?withViewAll=${this.showViewAll}
            >
              <slot name="header"></slot>
              <slot name="secondary-header"></slot>
            </span>
            <a
              href="about:firefoxview-next#${this.shortPageName}"
              @click=${this.viewAllClicked}
              class="view-all-link"
              data-l10n-id="firefoxview-view-all-link"
              ?hidden=${!this.showViewAll}
            ></a>
            <slot name="main"></slot>
            <slot name="footer" class="card-container-footer"></slot>
          </div>`,
          () => html`<details
            class=${classMap({
              "card-container": true,
              inner: this.isInnerCard,
              "empty-state": this.isEmptyState && !this.isInnerCard,
            })}
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
              @click=${this.viewAllClicked}
              class="view-all-link"
              data-l10n-id="firefoxview-view-all-link"
              ?hidden=${!this.showViewAll}
            ></a>
            <slot name="main"></slot>
            <slot name="footer" class="card-container-footer"></slot>
          </details>`
        )}
      </section>
    `;
  }
}
customElements.define("card-container", CardContainer);
