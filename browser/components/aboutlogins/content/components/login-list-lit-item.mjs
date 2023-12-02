/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  classMap,
  choose,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class ListItem extends MozLitElement {
  static get properties() {
    return {
      icon: { type: String },
      selected: { type: Boolean },
    };
  }

  constructor() {
    super();
    this.icon = "";
    this.selected = false;
  }

  render() {
    const classes = { selected: this.selected, "list-item": true };
    return html` <link
        rel="stylesheet"
        href="chrome://browser/content/aboutlogins/components/login-list-lit-item.css"
      />
      <li class=${classMap(classes)} role="option">
        <img class="icon" src=${this.icon} />
        <slot name="login-info"></slot>
        <slot name="notificationIcon"></slot>
      </li>`;
  }
}

export class NewListItem extends MozLitElement {
  static properties = {
    icon: { type: String },
    selected: { type: Boolean },
  };

  constructor() {
    super();
    this.id = "new-login-list-item";
    this.selected = false;
    this.icon = "page-icon:undefined";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/aboutlogins/components/login-list-lit-item.css"
      />
      <list-item ?selected=${this.selected} icon=${this.icon}>
        <div class="labels" slot="login-info">
          <span
            class="title"
            dir="auto"
            data-l10n-id="login-list-item-title-new-login2"
          ></span
          ><span class="subtitle" dir="auto"></span>
        </div>
      </list-item>
    `;
  }
}

export class LoginListItem extends MozLitElement {
  static get properties() {
    return {
      favicon: { type: String },
      title: { type: String, reflect: true },
      username: { type: String, reflect: true },
      notificationIcon: { type: String, reflect: true },
      selected: { type: Boolean },
    };
  }

  constructor() {
    super();
    this.favicon = "";
    this.title = "";
    this.username = "";
    this.notificationIcon = "";
    this.selected = false;
  }
  render() {
    switch (this.notificationIcon) {
      case "breached":
        this.classList.add("breached");
        break;
      case "vulnerable":
        this.classList.add("vulnerable");
        break;
      default:
        this.classList.remove("breached");
        this.classList.remove("vulnerable");
        break;
    }

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/aboutlogins/components/login-list-lit-item.css"
      />
      <list-item
        icon=${this.favicon}
        title=${this.title}
        username=${this.username}
        notificationIcon=${this.notificationIcon}
        ?selected=${this.selected}
      >
        <div class="labels" slot="login-info">
          <span class="title" dir="auto">${this.title}</span>
          ${when(
            this.username,
            () => html` <span class="subtitle" dir="auto">
              ${this.username}
            </span>`,
            () => html`<span
              class="subtitle"
              dir="auto"
              data-l10n-id="login-list-item-subtitle-missing-username"
            ></span>`
          )}
        </div>
        <div slot="notificationIcon">
          ${choose(
            this.notificationIcon,
            [
              [
                "breached",
                () =>
                  html`<img
                    class="alert-icon"
                    data-l10n-id="about-logins-list-item-breach-icon"
                    title=""
                    src="chrome://browser/content/aboutlogins/icons/breached-website.svg"
                  />`,
              ],
              [
                "vulnerable",
                () =>
                  html`<img
                    class="alert-icon"
                    data-l10n-id="about-logins-list-item-vulnerable-password-icon"
                    title=""
                    src="chrome://browser/content/aboutlogins/icons/vulnerable-password.svg"
                  />`,
              ],
            ],
            () => html`<img class="alert-icon" title="" src="" />`
          )}
        </div>
      </list-item>
    `;
  }
}

customElements.define("list-item", ListItem);
customElements.define("new-list-item", NewListItem);
customElements.define("login-list-item", LoginListItem);
