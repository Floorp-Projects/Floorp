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

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/aboutlogins/components/login-alert.css"
      />
      <img src=${ifDefined(this.icon)} />
      <h3 data-l10n-id=${ifDefined(this.titleId)}></h3>
      <div>
        <slot name="action"></slot>
      </div>
      <slot name="content"></slot>
    `;
  }
}

export class VulnerablePasswordAlert extends MozLitElement {
  static get properties() {
    return {
      hostname: { type: String, reflect: true },
    };
  }

  constructor() {
    super();
    this.hostname = "";
  }
  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/aboutlogins/components/login-alert.css"
      />
      <login-alert
        variant="info"
        icon="chrome://browser/content/aboutlogins/icons/vulnerable-password.svg"
        titleId="about-logins-vulnerable-alert-title"
      >
        <div slot="content">
          <span
            class="alert-text"
            data-l10n-id="about-logins-vulnerable-alert-text2"
          ></span>
          <a
            class="alert-link"
            data-l10n-id="about-logins-vulnerable-alert-link"
            data-l10n-args=${JSON.stringify({
              hostname: this.hostname,
            })}
            href=${this.hostname}
            rel="noreferrer"
            target="_blank"
          ></a>
        </div>
        <a
          slot="action"
          class="alert-learn-more-link"
          data-l10n-id="about-logins-vulnerable-alert-learn-more-link"
          href="https://support.mozilla.org/1/firefox/114.0.1/Darwin/en-CA/lockwise-alerts"
          rel="noreferrer"
          target="_blank"
        ></a>
      </login-alert>
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
      <link
        rel="stylesheet"
        href="chrome://browser/content/aboutlogins/components/login-alert.css"
      />
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

customElements.define(
  "login-vulnerable-password-alert",
  VulnerablePasswordAlert
);
customElements.define("login-breach-alert", LoginBreachAlert);
customElements.define("login-alert", LoginAlert);
