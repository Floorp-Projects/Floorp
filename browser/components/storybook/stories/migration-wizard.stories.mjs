/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/migration/content/migration-wizard.mjs";
import { MigrationWizardConstants } from "chrome://browser/content/migration/migration-wizard-constants.mjs";

// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "toolkit-widgets/named-deck.js";

export default {
  title: "Design System/Components/Migration Wizard",
};

const FAKE_BROWSER_LIST = [
  "Google Chrome - Profile 1",
  "Google Chrome - Profile 2",
  "Internet Explorer",
  "Edge",
  "Brave",
  "Safari",
  "Vivaldi",
  "Opera",
  "Opera GX",
];

const Template = state => {
  let wiz = document.createElement("migration-wizard");
  wiz.setState(state);
  return wiz;
};

export const MainSelectorVariant1 = Template.bind({});
MainSelectorVariant1.args = {
  page: MigrationWizardConstants.PAGES.SELECTION,
  migrators: FAKE_BROWSER_LIST,
  withImportAll: false,
};

export const MainSelectorVariant2 = Template.bind({});
MainSelectorVariant2.args = {
  page: MigrationWizardConstants.PAGES.SELECTION,
  migrators: FAKE_BROWSER_LIST,
  withImportAll: true,
};

export const Progress = Template.bind({});
Progress.args = {
  page: MigrationWizardConstants.PAGES.PROGRESS,
};

export const Success = Template.bind({});
Success.args = {
  page: MigrationWizardConstants.PAGES.PROGRESS,
};

export const SafariPermissions = Template.bind({});
SafariPermissions.args = {
  page: MigrationWizardConstants.PAGES.SAFARI_PERMISSION,
};
