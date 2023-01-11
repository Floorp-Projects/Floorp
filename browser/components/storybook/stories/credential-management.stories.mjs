/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit";
// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "../../aboutlogins/content/components/login-timeline.mjs";

export default {
  title: "Widgets/Credential Management/Timeline",
};

window.MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");

const Template = ({ history }) =>
  html`
    <login-timeline .history=${history}></login-timeline>
  `;

export const EmptyTimeline = Template.bind({});
EmptyTimeline.args = {
  history: [],
};

export const TypicalTimeline = Template.bind({});
TypicalTimeline.args = {
  history: [
    { action: "Created", time: 1463526500267 },
    { action: "Updated", time: 1653621219569 },
    { action: "Used", time: 1561813190300 },
  ],
};

export const AllSameDayTimeline = Template.bind({});
AllSameDayTimeline.args = {
  history: [
    { action: "Created", time: 1463526500267 },
    { action: "Updated", time: 1463526500267 },
    { action: "Used", time: 1463526500267 },
  ],
};
