/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unassigned-import
import "toolkit-widgets/panel-list.js";
import { html, ifDefined } from "lit.all.mjs";

export default {
  title: "Design System/Components/Panel Menu",
  parameters: {
    actions: {
      handles: ["click"],
    },
  },
};

const openMenu = e => {
  e.target
    .getRootNode()
    .querySelector("panel-list")
    .toggle(e);
};

const Template = ({ isOpen, items }) =>
  html`
    <style>
      panel-item[icon="passwords"]::part(button) {
        background-image: url("chrome://browser/skin/login.svg");
      }
      panel-item[icon="settings"]::part(button) {
        background-image: url("chrome://global/skin/icons/settings.svg");
      }
      button {
        position: absolute;
        background-image: url("chrome://global/skin/icons/more.svg");
      }
      .end {
        inset-inline-end: 30px;
      }

      .bottom {
        inset-block-end: 30px;
      }
    </style>
    <button class="ghost-button icon-button" @click=${openMenu}></button>
    <button class="ghost-button icon-button end" @click=${openMenu}></button>
    <button class="ghost-button icon-button bottom" @click=${openMenu}></button>
    <button
      class="ghost-button icon-button bottom end"
      @click=${openMenu}
    ></button>
    <panel-list ?stay-open=${isOpen} ?open=${isOpen}>
      ${items.map(i =>
        i == "<hr>"
          ? html`
              <hr />
            `
          : html`
              <panel-item
                icon=${i.icon ?? ""}
                ?checked=${i.checked}
                ?badged=${i.badged}
                accesskey=${ifDefined(i.accesskey)}
              >
                ${i.text ?? i}
              </panel-item>
            `
      )}
    </panel-list>
  `;

export const Simple = Template.bind({});
Simple.args = {
  isOpen: false,
  items: [
    "Item One",
    { text: "Item Two (accesskey w)", accesskey: "w" },
    "Item Three",
    "<hr>",
    { text: "Checked", checked: true },
    { text: "Badged, look at me", badged: true, icon: "settings" },
  ],
};

export const Icons = Template.bind({});
Icons.args = {
  isOpen: false,
  items: [
    { text: "Passwords", icon: "passwords" },
    { text: "Settings", icon: "settings" },
  ],
};

export const Open = Template.bind({});
Open.args = {
  ...Simple.args,
  isOpen: true,
};
