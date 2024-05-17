/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "./backup-settings.mjs";

window.MozXULElement.insertFTLIfNeeded("locales-preview/backupSettings.ftl");
window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");

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
    backupFilePath: "Documents",
    backupInProgress: false,
    scheduledBackupsEnabled: false,
  },
};

export const BackingUpInProgress = Template.bind({});
BackingUpInProgress.args = {
  backupServiceState: {
    backupFilePath: "Documents",
    backupInProgress: true,
    scheduledBackupsEnabled: false,
  },
};
