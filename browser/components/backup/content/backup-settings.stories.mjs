/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./backup-settings.mjs";

export default {
  title: "Domain-specific UI Widgets/Backup/Backup Settings",
  component: "backup-settings",
  argTypes: {},
};

const Template = ({ backupServiceState }) => html`
  <backup-settings .backupServiceState=${backupServiceState}></backup-settings>
`;

export const BackingUpNotInProgress = Template.bind({});
BackingUpNotInProgress.args = {
  backupServiceState: {
    backupInProgress: false,
  },
};

export const BackingUpInProgress = Template.bind({});
BackingUpInProgress.args = {
  backupServiceState: {
    backupInProgress: true,
  },
};
