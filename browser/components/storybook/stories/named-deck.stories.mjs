/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit.all.mjs";
// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "toolkit-widgets/named-deck.js";

export default {
  title: "UI Widgets/Named Deck",
  component: "named-deck",
  parameters: {
    status: "stable",
    fluent: `
named-deck-tab-one = Tab 1
named-deck-tab-two = Tab 2
named-deck-tab-three = Tab 3
named-deck-content-one = This is tab 1
named-deck-content-two = This is tab 2
named-deck-content-three = This is tab 3
button-group-one = One
button-group-two = Two
button-group-three = Three
button-group-four = Four
    `,
  },
};

export const Tabs = () => html`
  <style>
    button[selected] {
      outline: 2px dashed var(--in-content-primary-button-background);
    }
  </style>
  <button-group>
    <button is="named-deck-button" deck="tabs-deck" name="tab-1" data-l10n-id="named-deck-tab-one"></button>
    <button is="named-deck-button" deck="tabs-deck" name="tab-2" data-l10n-id="named-deck-tab-two"></button>
    <button is="named-deck-button" deck="tabs-deck" name="tab-3" data-l10n-id="named-deck-tab-three"></button>
  </button-group>
  <named-deck id="tabs-deck" is-tabbed>
    <p name="tab-1" data-l10n-id="named-deck-content-one"></p>
    <p name="tab-2" data-l10n-id="named-deck-content-two"></p>
    <p name="tab-3" data-l10n-id="named-deck-content-three"></p>
  </named-deck>

  <hr>

  <p>
    The dashed outline is added for emphasis here. It applies to the button with
    the <code>selected</code> attribute, but matches the deck's
    <code>selected-view</code> name.
  </p>

  <p>
    These tabs are a combination of <code>button-group</code>,
    <code>named-deck-button</code>, and <code>named-deck</code>.
    <ul>
      <li>
        <code>button-group</code> makes the tabs a single focusable group,
        using left/right to switch between focused buttons.
      </li>
      <li>
        <code>named-deck-button</code>s are <code>button</code> subclasses
        that are used to control the <code>named-deck</code>.
      </li>
      <li>
        <code>named-deck</code> show only one selected child at a time.
      </li>
    </ul>
  </p>
`;

export const ListMenu = () => html`
  <style>
    .icon-button {
      background-image: url("chrome://global/skin/icons/arrow-left.svg");
    }

    .vertical-group {
      display: flex;
      flex-direction: column;
      width: 200px;
    }
  </style>
  <named-deck id="list-deck" is-tabbed>
    <section name="list">
      <button-group orientation="vertical" class="vertical-group">
        <button is="named-deck-button" deck="list-deck" name="tab-1">
          Tab 1
        </button>
        <button is="named-deck-button" deck="list-deck" name="tab-2">
          Tab 2
        </button>
        <button is="named-deck-button" deck="list-deck" name="tab-3">
          Tab 3
        </button>
      </button-group>
    </section>
    <section name="tab-1">
      <button
        class="icon-button ghost-button"
        is="named-deck-button"
        deck="list-deck"
        name="list"
      ></button>
      <p>This is tab 1</p>
    </section>
    <section name="tab-2">
      <button
        class="icon-button ghost-button"
        is="named-deck-button"
        deck="list-deck"
        name="list"
      ></button>
      <p>This is tab 2</p>
    </section>
    <section name="tab-3">
      <button
        class="icon-button ghost-button"
        is="named-deck-button"
        deck="list-deck"
        name="list"
      ></button>
      <p>This is tab 3</p>
    </section>
  </named-deck>

  <hr />

  <p>
    This is an alternate layout for creating a menu navigation. In this case,
    the first view in the <code>named-deck</code> is the list view which
    contains the <code>named-deck-button</code>s to link to the other views.
    Each view then includes a back <code>named-deck-button</code> which is used
    to navigate back to the first view.
  </p>
`;

const FocusGroupTemplate = ({ orientation }) => html`
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

export const FocusGroup = FocusGroupTemplate.bind({});
FocusGroup.args = {
  orientation: "horizontal",
};
FocusGroup.argTypes = {
  orientation: {
    options: ["horizontal", "vertical"],
    control: { type: "radio" },
  },
};
