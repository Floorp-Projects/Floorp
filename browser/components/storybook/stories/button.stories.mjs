/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { classMap, html } from "lit.all.mjs";

export default {
  title: "UI Widgets/Button",
  component: "button",
  argTypes: {
    size: {
      options: ["default", "small"],
      control: { type: "radio" },
    },
  },
  parameters: {
    status: "stable",
    fluent: `
button-regular = Regular
button-primary = Primary
button-disabled = Disabled
button-danger = Danger
button-small = Small
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
  size,
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
        "small-button": size === "small",
      })}
      data-l10n-id=${l10nId}
    ></button>
  `;

export const RegularButton = Template.bind({});
RegularButton.args = {
  l10nId: "button-regular",
  primary: false,
  disabled: false,
  size: "default",
};
export const PrimaryButton = Template.bind({});
PrimaryButton.args = {
  l10nId: "button-primary",
  primary: true,
  disabled: false,
  size: "default",
};
export const DisabledButton = Template.bind({});
DisabledButton.args = {
  l10nId: "button-disabled",
  primary: false,
  disabled: true,
  size: "default",
};

export const DangerButton = Template.bind({});
DangerButton.args = {
  l10nId: "button-danger",
  primary: true,
  disabled: false,
  dangerButton: true,
  size: "default",
};

export const GhostIconButton = Template.bind({});
GhostIconButton.args = {
  icon: "chrome://browser/skin/login.svg",
  disabled: false,
  ghostButton: true,
  size: "default",
};
export const SmallButton = Template.bind({});
SmallButton.args = {
  l10nId: "button-small",
  primary: true,
  disabled: false,
  size: "small",
};
