/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit.all.mjs";
// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "toolkit-widgets/named-deck.js";

export default {
  title: "Widgets/Functional/Button Group",
  component: "button-group",
  argTypes: {
    orientation: {
      options: ["horizontal", "vertical"],
      control: { type: "radio" },
    },
  },
  parameters: {
    fluent: `
button-group-one = One
button-group-two = Two
button-group-three = Three
button-group-four = Four
    `,
  },
};

const Template = ({ orientation }) => html`
  <button-group orientation=${orientation}>
    <button data-l10n-id="button-group-one"></button>
    <button data-l10n-id="button-group-two"></button>
    <button data-l10n-id="button-group-three"></button>
    <button data-l10n-id="button-group-four"></button>
  </button-group>

  <p>
    The <code>button-group</code> element will group focus to the buttons,
    requiring left/right or up/down to switch focus between its child elements.
    It accepts an <code>orientation</code> property, which determines if
    left/right or up/down are used to change the focused button.
  </p>
`;
export const ButtonGroup = Template.bind({});
ButtonGroup.args = {
  orientation: "horizontal",
};
