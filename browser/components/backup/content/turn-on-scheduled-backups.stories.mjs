/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "chrome://global/content/elements/moz-card.mjs";
import "./turn-on-scheduled-backups.mjs";

window.MozXULElement.insertFTLIfNeeded("locales-preview/backupSettings.ftl");
window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");

export default {
  title: "Domain-specific UI Widgets/Backup/Turn On Scheduled Backups",
  component: "turn-on-scheduled-backups",
  argTypes: {},
};

const Template = ({ backupFilePath }) => html`
  <moz-card style="width: 27.8rem;">
    <turn-on-scheduled-backups
      .backupFilePath=${backupFilePath}
    ></turn-on-scheduled-backups>
  </moz-card>
`;

export const RecommendedFolder = Template.bind({});
RecommendedFolder.args = {
  backupFilePath: "Documents",
};
