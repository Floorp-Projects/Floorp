/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit";

import "toolkit-widgets/message-bar.js";

const MESSAGE_TYPES = {
  default: "",
  success: "success",
  error: "error",
  warning: "warning",
};

export default {
  title: "Design System/Components/Message Bar",
  argTypes: {
    type: {
      options: Object.keys(MESSAGE_TYPES),
      mapping: MESSAGE_TYPES,
      control: { type: "select" },
    },
  },
};

const Template = ({ dismissable, type }) =>
  html`
    <message-bar type=${type} ?dismissable=${dismissable}>
      <span>A very expressive and slightly whimsical message goes here.</span>
      <button>Click me, please!</button>
    </message-bar>
  `;

export const Basic = Template.bind({});
Basic.args = { type: "", dismissable: false };

export const Dismissable = Template.bind({});
Dismissable.args = { type: "", dismissable: true };
