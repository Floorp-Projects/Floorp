/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, classMap } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { editableFieldTemplate, stylesTemplate } from "./input-field.mjs";

class LoginPasswordField extends MozLitElement {
  static CONCEALED_PASSWORD_TEXT = " ".repeat(8);

  static properties = {
    _value: { type: String, state: true },
    readonly: { type: Boolean, reflect: true },
    visible: { type: Boolean, reflect: true },
  };

  static queries = {
    input: "input",
    button: "button",
  };

  set value(newValue) {
    this._value = newValue;
  }

  get #type() {
    return this.visible ? "text" : "password";
  }

  get #password() {
    return this.readonly && !this.visible
      ? LoginPasswordField.CONCEALED_PASSWORD_TEXT
      : this._value;
  }

  render() {
    return html`
      ${stylesTemplate()}
      <label
        class="field-label"
        data-l10n-id="login-item-password-label"
      ></label>
      ${editableFieldTemplate({
        type: this.#type,
        value: this.#password,
        labelId: "login-item-password-label",
        disabled: this.readonly,
        onFocus: this.handleFocus,
        onBlur: this.handleBlur,
      })}
      <button
        class=${classMap({
          revealed: this.visible,
          "reveal-password-button": true,
        })}
        data-l10n-id="login-item-password-reveal-checkbox"
        @click=${this.toggleVisibility}
      ></button>
    `;
  }

  handleFocus(ev) {
    if (ev.relatedTarget !== this.button) {
      this.visible = true;
    }
  }

  handleBlur(ev) {
    if (ev.relatedTarget !== this.button) {
      this.visible = false;
    }
  }

  toggleVisibility() {
    this.visible = !this.visible;
    if (this.visible) {
      this.onPasswordVisible?.();
    }
    this.input.focus();
  }
}

customElements.define("login-password-field", LoginPasswordField);
