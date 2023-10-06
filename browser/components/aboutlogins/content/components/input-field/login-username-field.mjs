/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { editableFieldTemplate, stylesTemplate } from "./input-field.mjs";

class LoginUsernameField extends MozLitElement {
  static properties = {
    value: { type: String, reflect: true },
    readonly: { type: Boolean, reflect: true },
  };

  render() {
    return html`
      ${stylesTemplate()}
      <label
        class="field-label"
        data-l10n-id="login-item-username-label"
      ></label>
      ${editableFieldTemplate({
        type: "text",
        value: this.value,
        disabled: this.readonly,
      })}
    `;
  }
}

customElements.define("login-username-field", LoginUsernameField);
