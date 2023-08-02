/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit.all.mjs";
// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "../../aboutlogins/content/components/login-timeline.mjs";

export default {
  title: "Domain-specific UI Widgets/Credential Management/Timeline",
  component: "login-timeline",
};

window.MozXULElement.insertFTLIfNeeded("browser/aboutLogins.ftl");

const Template = ({ historyItems }) =>
  html` <login-timeline .history=${historyItems}></login-timeline> `;

const ACTION_ID_CREATED = "login-item-timeline-action-created";
const ACTION_ID_UPDATED = "login-item-timeline-action-updated";
const ACTION_ID_USED = "login-item-timeline-action-used";

export const EmptyTimeline = Template.bind({});
EmptyTimeline.args = {
  historyItems: [],
};

export const TypicalTimeline = Template.bind({});
TypicalTimeline.args = {
  historyItems: [
    { actionId: ACTION_ID_CREATED, time: 1463526500267 },
    { actionId: ACTION_ID_UPDATED, time: 1653621219569 },
    { actionId: ACTION_ID_USED, time: 1561813190300 },
  ],
};

export const AllSameDayTimeline = Template.bind({});
AllSameDayTimeline.args = {
  historyItems: [
    { actionId: ACTION_ID_CREATED, time: 1463526500267 },
    { actionId: ACTION_ID_UPDATED, time: 1463526500267 },
    { actionId: ACTION_ID_USED, time: 1463526500267 },
  ],
};
