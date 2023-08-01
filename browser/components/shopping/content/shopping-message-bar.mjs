/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

class ShoppingMessageBar extends MozLitElement {
  #MESSAGE_TYPES_RENDER_TEMPLATE_MAPPING = new Map([
    ["stale", this.getStaleWarningTemplate()],
  ]);

  static properties = {
    type: { type: String },
  };

  getStaleWarningTemplate() {
    // TODO: Bug 1843142 - add proper stale analysis link once finalized
    return html` <message-bar type="warning">
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-stale-analysis-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-stale-analysis-message"
        ></span>
        <a
          target="_blank"
          data-l10n-id="shopping-message-bar-warning-stale-analysis-link"
          href="#"
        ></a>
      </article>
    </message-bar>`;
  }

  render() {
    let messageBarTemplate = this.#MESSAGE_TYPES_RENDER_TEMPLATE_MAPPING.get(
      this.type
    );
    if (messageBarTemplate) {
      return html`
        <link
          rel="stylesheet"
          href="chrome://browser/content/shopping/shopping-message-bar.css"
        />
        ${messageBarTemplate}
      `;
    }
    return null;
  }

  firstUpdated() {
    // message-bar does not support adding a header and does not align it with the icon.
    // Override styling to make them align.
    let messageBar = this.renderRoot.querySelector("message-bar");
    let messageBarContainer = messageBar.shadowRoot.querySelector(".container");
    let icon = messageBarContainer.querySelector(".icon");

    messageBarContainer.style.alignItems = "start";
    messageBarContainer.style.padding = "0.5rem 0.75rem";
    messageBarContainer.style.gap = "0.75rem";
    icon.style.paddingBlockStart = "0";
  }
}

customElements.define("shopping-message-bar", ShoppingMessageBar);
