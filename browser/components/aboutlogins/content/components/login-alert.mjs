/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  guard,
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

  static stylesheetUrl = window.IS_STORYBOOK
    ? "./login-alert.css"
    : "chrome://browser/content/aboutlogins/components/login-alert.css";

  render() {
    return html`
      <link rel="stylesheet" href=${this.constructor.stylesheetUrl} />
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

  static stylesheetUrl = window.IS_STORYBOOK
    ? "./login-alert.css"
    : "chrome://browser/content/aboutlogins/components/login-alert.css";

  constructor() {
    super();
    this.date = 0;
    this.hostname = "";
  }

  get displayHostname() {
    try {
      return new URL(this.hostname).hostname;
    } catch (err) {
      return this.hostname;
    }
  }

  render() {
    return html`
      <link rel="stylesheet" href=${this.constructor.stylesheetUrl} />
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
            data-l10n-args=${guard([this.hostname], () =>
              JSON.stringify({
                hostname: this.displayHostname,
              })
            )}
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
