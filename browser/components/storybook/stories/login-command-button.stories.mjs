/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit.all.mjs";
// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "../../aboutlogins/content/components/login-command-button.mjs";

const BUTTON_TYPES = {
  Edit: "login-item-edit-button",
  "Copy Username": "login-item-copy-username-button-text",
  "Copy Password": "login-item-copy-password-button-text",
  Remove: "about-logins-login-item-remove-button",
};

const BUTTON_ICONS = {
  Edit: "chrome://global/skin/icons/edit.svg",
  "Copy Username": "chrome://global/skin/icons/edit-copy.svg",
  "Copy Password": "chrome://global/skin/icons/edit-copy.svg",
  Remove: "chrome://global/skin/icons/delete.svg",
};

const BUTTON_VARIANT = {
  Regular: "",
  Danger: "primary danger-button",
  Primary: "primary",
  Icon: "ghost-button icon-button",
};

export default {
  title: "Domain-specific UI Widgets/Credential Management/Command Button",
  component: "login-command-button",
  argTypes: {
    l10nId: {
      options: Object.keys(BUTTON_TYPES),
      mapping: BUTTON_TYPES,
      control: { type: "select" },
      defaultValue: "Edit",
    },
    icon: {
      options: Object.keys(BUTTON_ICONS),
      mapping: BUTTON_ICONS,
      control: { type: "select" },
      defaultValue: "Edit",
    },
    variant: {
      options: Object.keys(BUTTON_VARIANT),
      mapping: BUTTON_VARIANT,
      control: { type: "select" },
      defaultValue: "Regular",
    },
  },
};

window.MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");

const Template = ({ onClick, l10nId, icon, variant, disabled, tooltip }) => {
  return html`
    <login-command-button
      .onClick=${onClick}
      .l10nId=${l10nId}
      .icon=${icon}
      .variant=${variant}
      .disabled=${disabled}
      .tooltip=${tooltip}
    >
    </login-command-button>
  `;
};

export const WorkingButton = Template.bind({});
WorkingButton.args = {
  onClick: () => alert("I work Woohoo!!"),
  l10nId: "login-item-edit-button",
  icon: "chrome://global/skin/icons/edit.svg",
  variant: "",
  disabled: false,
  tooltip: "Lorem ipsum dolor",
};

export const DisabledButton = Template.bind({});
DisabledButton.args = {
  onClick: () => alert("I shouldn't be working..."),
  l10nId: "login-item-edit-button",
  icon: "chrome://global/skin/icons/edit.svg",
  variant: "",
  disabled: true,
  tooltip: "Lorem ipsum dolor",
};
