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

export class LoginBreachAlert extends MozLitElement {
  static get properties() {
    return {
      date: { type: Number, reflect: true },
      hostname: { type: String, reflect: true },
    };
  }

  static styles = css`
    div[slot="content"] {
      margin-block-start: 8px;
    }

    h4 {
      margin: 0;
    }

    a {
      font-weight: 600;
      color: inherit;
    }
  `;

  constructor() {
    super();
    this.date = 0;
    this.hostname = "";
  }

  render() {
    return html`
      <login-alert
        variant="error"
        icon="chrome://browser/content/aboutlogins/icons/breached-website.svg"
        titleId="about-logins-breach-alert-title"
      >
        <div slot="content">
          <h4
            data-l10n-id="about-logins-breach-alert-date"
            data-l10n-args=${JSON.stringify({ date: this.date })}
          ></h4>
          <span data-l10n-id="breach-alert-text"></span>
          <a
            data-l10n-id="about-logins-breach-alert-link"
            data-l10n-args=${JSON.stringify({
              hostname: this.hostname,
            })}
            href=${this.hostname}
            rel="noreferrer"
            target="_blank"
          ></a>
        </div>
      </login-alert>
    `;
  }
}

customElements.define("login-breach-alert", LoginBreachAlert);
customElements.define("login-alert", LoginAlert);
