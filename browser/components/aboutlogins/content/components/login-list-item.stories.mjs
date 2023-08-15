/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./login-list-lit-item.mjs";

export default {
  title: "Domain-specific UI Widgets/Credential Management/Login List Item",
  component: "login-list-item",
};

window.MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");

export const NewLoginListItem = ({ selected }) => {
  return html` <new-list-item .selected=${selected}> </new-list-item> `;
};

NewLoginListItem.argTypes = {
  selected: {
    options: [true, false],
    control: { type: "radio" },
    defaultValue: false,
  },
};

export const LoginListItem = ({
  title,
  username,
  notificationIcon,
  selected,
}) => {
  return html`
    <login-list-item
      .title=${title}
      .username=${username}
      .notificationIcon=${notificationIcon}
      .selected=${selected}
    >
    </login-list-item>
  `;
};

LoginListItem.argTypes = {
  notificationIcon: {
    options: ["default", "breached", "vulnerable"],
    control: { type: "radio" },
    defaultValue: "default",
  },
  selected: {
    options: [true, false],
    control: { type: "radio" },
    defaultValue: false,
  },
};

LoginListItem.args = {
  title: "https://www.example.com",
  username: "test-username",
};
