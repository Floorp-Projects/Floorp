/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { classMap, html } from "lit.all.mjs";

export default {
  title: "UI Widgets/Button",
  component: "button",
  parameters: {
    fluent: `
button-regular = Regular
button-primary = Primary
button-disabled = Disabled
button-danger = Danger
    `,
  },
};

const Template = ({
  disabled,
  primary,
  l10nId,
  ghostButton,
  icon,
  dangerButton,
}) =>
  html`
    <style>
      .icon-button {
        background-image: url("${icon}");
      }
    </style>
    <button
      ?disabled=${disabled}
      class=${classMap({
        primary,
        "ghost-button": ghostButton,
        "icon-button": icon,
        "danger-button": dangerButton,
      })}
      data-l10n-id=${l10nId}
    ></button>
  `;

export const RegularButton = Template.bind({});
RegularButton.args = {
  l10nId: "button-regular",
  primary: false,
  disabled: false,
};
export const PrimaryButton = Template.bind({});
PrimaryButton.args = {
  l10nId: "button-primary",
  primary: true,
  disabled: false,
};
export const DisabledButton = Template.bind({});
DisabledButton.args = {
  l10nId: "button-disabled",
  primary: false,
  disabled: true,
};

export const DangerButton = Template.bind({});
DangerButton.args = {
  l10nId: "button-danger",
  primary: true,
  disabled: false,
  dangerButton: true,
};

export const GhostIconButton = Template.bind({});
GhostIconButton.args = {
  icon: "chrome://browser/skin/login.svg",
  disabled: false,
  ghostButton: true,
};
