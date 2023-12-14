/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./login-alert.mjs";

export default {
  title: "Domain-specific UI Widgets/Credential Management/Login Alert",
  component: "login-alert",
};

window.MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");

export const BasicLoginAlert = ({ variant, icon }) => {
  return html`
    <login-alert
      .variant=${variant}
      .icon=${icon}
      titleId="about-logins-login-intro-heading-message"
    >
      <a slot="action"> Some action </a>
      <div slot="content">
        Alert extra content, could be a description for more context.
      </div>
    </login-alert>
  `;
};

BasicLoginAlert.argTypes = {
  variant: {
    options: ["info", "error", "warning"],
    control: { type: "radio" },
    defaultValue: "info",
  },
  icon: {
    options: {
      info: "chrome://global/skin/icons/info-filled.svg",
      "breached-website":
        "chrome://browser/content/aboutlogins/icons/breached-website.svg",
      "vulnerable-password":
        "chrome://browser/content/aboutlogins/icons/vulnerable-password.svg",
    },
    control: { type: "select" },
    defaultValue: "chrome://global/skin/icons/info-filled.svg",
  },
};

export const VulnerablePasswordAlert = ({ hostname }) =>
  html`
    <login-vulnerable-password-alert
      .hostname=${hostname}
    ></login-vulnerable-password-alert>
  `;

VulnerablePasswordAlert.args = {
  hostname: "https://www.example.com",
};

export const LoginBreachAlert = ({ date, hostname }) =>
  html`
    <login-breach-alert
      .date=${date}
      .hostname=${hostname}
    ></login-breach-alert>
  `;

LoginBreachAlert.argTypes = {
  date: {
    control: { type: "date" },
    defaultValue: 1684849435571,
  },
  hostname: {
    control: { type: "text" },
    defaultValue: "https://www.example.com",
  },
};
