/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { classMap, html } from "lit.all.mjs";

export default {
  title: "Design System/Atoms/Button",
};

const Template = ({
  disabled,
  primary,
  text,
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
    >
      ${text}
    </button>
  `;

export const RegularButton = Template.bind({});
RegularButton.args = {
  text: "Regular",
  primary: false,
  disabled: false,
};
export const PrimaryButton = Template.bind({});
PrimaryButton.args = {
  text: "Primary",
  primary: true,
  disabled: false,
};
export const DisabledButton = Template.bind({});
DisabledButton.args = {
  text: "Disabled",
  primary: false,
  disabled: true,
};

export const DangerButton = Template.bind({});
DangerButton.args = {
  text: "Danger",
  primary: true,
  disabled: false,
  dangerButton: true,
};

export const GhostIconButton = Template.bind({});
GhostIconButton.args = {
  text: "",
  icon: "chrome://browser/skin/login.svg",
  disabled: false,
  ghostButton: true,
};
