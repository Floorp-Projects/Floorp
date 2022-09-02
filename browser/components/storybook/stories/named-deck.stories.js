/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit";
import "toolkit-widgets/named-deck.js";

export default {
  title: "Widgets/Functional/Named Deck",
};

export const Tabs = () => html`
  <style>
    button[selected] {
      outline: 2px dashed var(--in-content-primary-button-background);
    }
  </style>
  <button-group>
    <button is="named-deck-button" deck="tabs-deck" name="tab-1">Tab 1</button>
    <button is="named-deck-button" deck="tabs-deck" name="tab-2">Tab 2</button>
    <button is="named-deck-button" deck="tabs-deck" name="tab-3">Tab 3</button>
  </button-group>
  <named-deck id="tabs-deck" is-tabbed>
    <p name="tab-1">This is tab 1</p>
    <p name="tab-2">This is tab 2</p>
    <p name="tab-3">This is tab 3</p>
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
