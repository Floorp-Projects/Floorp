/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./login-password-field.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./login-username-field.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./login-origin-field.mjs";

export default {
  title: "Domain-specific UI Widgets/Credential Management/Input Fields",
};

window.MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");

export const LoginUsernameField = ({ value, readonly }) => {
  return html`
    <div style="max-width: 500px">
      <login-username-field .value=${value} .readonly=${readonly}>
      </login-username-field>
    </div>
  `;
};

LoginUsernameField.argTypes = {
  value: {
    control: "text",
    defaultValue: "username",
  },
  readonly: {
    control: "boolean",
    defaultValue: false,
  },
};

export const LoginOriginField = ({ value, readonly }) => {
  return html`
    <div style="max-width: 500px">
      <login-origin-field .value=${value} .readonly=${readonly}>
      </login-origin-field>
    </div>
  `;
};

LoginOriginField.argTypes = {
  value: {
    control: "text",
    defaultValue: "https://example.com",
  },
  readonly: {
    control: "boolean",
    defaultValue: false,
  },
};

export const LoginPasswordField = ({
  readonly,
  visible,
  value = "longpassword".repeat(2),
}) => {
  return html`
    <div style="max-width: 500px">
      <login-password-field
        .value=${value}
        .readonly=${readonly}
        .visible=${visible}
        .onPasswordVisible=${() => alert("auth...")}
      >
      </login-password-field>
    </div>
  `;
};

LoginPasswordField.argTypes = {
  readonly: {
    control: "boolean",
    defaultValue: true,
  },
  visible: {
    control: "boolean",
    defaultValue: false,
  },
};

export const LoginPasswordFieldDisplayMode = ({
  visible,
  value = "longpassword".repeat(2),
}) => {
  return html`
    <div style="max-width: 500px">
      <login-password-field
        .value=${value}
        .readonly=${true}
        .visible=${visible}
      >
      </login-password-field>
    </div>
  `;
};

LoginPasswordFieldDisplayMode.argTypes = {
  visible: {
    control: "boolean",
    defaultValue: false,
  },
};

export const LoginPasswordFieldEditMode = ({
  visible,
  value = "longpassword".repeat(2),
}) => {
  return html`
    <div style="max-width: 500px">
      <login-password-field
        .value=${value}
        .readonly=${false}
        .visible=${visible}
      >
      </login-password-field>
    </div>
  `;
};

LoginPasswordFieldEditMode.argTypes = {
  visible: {
    control: "boolean",
    defaultValue: false,
  },
};
