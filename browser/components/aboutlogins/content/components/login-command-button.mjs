/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Note: For now, to display the tooltip for a <login-command-button> you need to
 * use data-l10n-id attribute instead of the l10nId attribute in the tag.
 * Bug 1844869 will make an attempt to fix this.
 */

import {
  html,
  ifDefined,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export const stylesTemplate = () => html`<link
    rel="stylesheet"
    href="chrome://global/skin/in-content/common.css"
  />
  <link
    rel="stylesheet"
    href="chrome://browser/content/aboutlogins/components/login-command-button.css"
  />`;

export const LoginCommandButton = ({
  onClick,
  l10nId,
  icon,
  variant,
  disabled,
  buttonText,
}) => html`<button
  class=${variant}
  data-l10n-id=${ifDefined(l10nId)}
  ?disabled=${disabled}
  @click=${ifDefined(onClick)}
>
  <img src=${ifDefined(icon)} role="presentation" />

  <span data-l10n-id=${ifDefined(buttonText)}></span>
</button>`;

export class CreateLoginButton extends MozLitElement {
  static get properties() {
    return {
      disabled: { type: Boolean, reflect: true },
    };
  }

  constructor() {
    super();
    this.disabled = false;
  }
  render() {
    return html`
      ${stylesTemplate()}
      ${LoginCommandButton({
        l10nId: "create-login-button",
        variant: "icon-button",
        icon: "chrome://global/skin/icons/plus.svg",
        disabled: this.disabled,
      })}
    `;
  }
}

export class EditButton extends MozLitElement {
  static get properties() {
    return {
      disabled: { type: Boolean, reflect: true },
    };
  }

  constructor() {
    super();
    this.disabled = false;
  }
  render() {
    return html`
      ${stylesTemplate()}
      ${LoginCommandButton({
        buttonText: "login-item-edit-button",
        variant: "ghost-button",
        icon: "chrome://global/skin/icons/edit.svg",
        disabled: this.disabled,
      })}
    `;
  }
}

export class DeleteButton extends MozLitElement {
  static get properties() {
    return {
      disabled: { type: Boolean, reflect: true },
    };
  }

  constructor() {
    super();
    this.disabled = false;
  }
  render() {
    return html` ${stylesTemplate()}
    ${LoginCommandButton({
      buttonText: "about-logins-login-item-remove-button",
      variant: "ghost-button",
      icon: "chrome://global/skin/icons/delete.svg",
      disabled: this.disabled,
    })}`;
  }
}

export class CopyUsernameButton extends MozLitElement {
  static get properties() {
    return {
      copiedText: { type: Boolean, reflect: true },
      disabled: { type: Boolean, reflect: true },
    };
  }

  constructor() {
    super();
    this.copiedText = false;
    this.disabled = false;
  }
  render() {
    this.className = this.copiedText ? "copied-button" : "copy-button";
    return html` ${stylesTemplate()}
    ${when(
      this.copiedText,
      () =>
        html`${LoginCommandButton({
          buttonText: "login-item-copied-username-button-text",
          icon: "chrome://global/skin/icons/check.svg",
          disabled: this.disabled,
        })}`,
      () =>
        html`${LoginCommandButton({
          variant: "text-button",
          buttonText: "login-item-copy-username-button-text",
          disabled: this.disabled,
        })}`
    )}`;
  }
}

export class CopyPasswordButton extends MozLitElement {
  static get properties() {
    return {
      copiedText: { type: Boolean, reflect: true },
      disabled: { type: Boolean, reflect: true },
    };
  }

  constructor() {
    super();
    this.copiedText = false;
    this.disabled = false;
  }
  render() {
    this.className = this.copiedText ? "copied-button" : "copy-button";
    return html` ${stylesTemplate()}
    ${when(
      this.copiedText,
      () =>
        html`${LoginCommandButton({
          buttonText: "login-item-copied-password-button-text",
          icon: "chrome://global/skin/icons/check.svg",
          disabled: this.disabled,
        })}`,
      () =>
        html`${LoginCommandButton({
          variant: "text-button",
          buttonText: "login-item-copy-password-button-text",
          disabled: this.disabled,
        })}`
    )}`;
  }
}

customElements.define("copy-password-button", CopyPasswordButton);
customElements.define("copy-username-button", CopyUsernameButton);
customElements.define("delete-button", DeleteButton);
customElements.define("edit-button", EditButton);
customElements.define("create-login-button", CreateLoginButton);
