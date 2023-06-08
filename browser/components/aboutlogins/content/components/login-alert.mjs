/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  css,
  ifDefined,
} from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class LoginAlert extends MozLitElement {
  static get properties() {
    return {
      variant: { type: String, reflect: true },
      icon: { type: String },
      titleId: { type: String },
    };
  }

  static styles = css`
    :host {
      display: grid;
      column-gap: 16px;
      grid-template-areas: "icon title action" "icon content content";
      grid-template-columns: min-content 1fr auto;
      padding: 16px 32px;
      color: var(--in-content-text-color);
      background: var(--in-content-box-background);
      border-radius: 4px;
      border: 1px solid var(--in-content-border-color);
      box-shadow: 0 2px 8px 0 var(--grey-90-a10);
    }

    :host([variant="info"]) {
      background: var(--in-content-box-background);
    }

    :host([variant="error"]) {
      background: var(--red-70);
    }

    img {
      grid-area: icon;
      width: 24px;
      -moz-context-properties: fill;
      fill: currentColor;
    }

    h3 {
      grid-area: title;
      font-size: 22px;
      font-weight: normal;
      line-height: 1em;
      margin: 0;
      padding: 0;
    }

    slot[name="action"] {
      grid-area: action;
    }

    slot[name="content"] {
      grid-area: content;
    }
  `;

  render() {
    return html`
      <img src=${ifDefined(this.icon)} />
      <h3 data-l10n-id=${ifDefined(this.titleId)}></h3>
      <div>
        <slot name="action"></slot>
      </div>
      <slot name="content"></slot>
    `;
  }
}

customElements.define("login-alert", LoginAlert);
