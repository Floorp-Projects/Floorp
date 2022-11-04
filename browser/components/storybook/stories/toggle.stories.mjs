/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit";

export default {
  title: "Design System/Atoms/Toggle",
};

const Template = ({ checked, disabled }) =>
  html`
    <link rel="stylesheet" href="chrome://global/skin/in-content/common.css" />
    <link
      rel="stylesheet"
      href="chrome://global/skin/in-content/toggle-button.css"
    />
    <input
      type="checkbox"
      class="toggle-button"
      ?checked=${checked}
      ?disabled=${disabled}
    />
  `;

export const Unchecked = Template.bind({});
Unchecked.args = {
  disabled: false,
  checked: false,
};
export const Checked = Template.bind({});
Checked.args = {
  disabled: false,
  checked: true,
};
export const DisabledUnchecked = Template.bind({});
DisabledUnchecked.args = {
  disabled: true,
  checked: false,
};
export const DisabledChecked = Template.bind({});
DisabledChecked.args = {
  disabled: true,
  checked: true,
};
