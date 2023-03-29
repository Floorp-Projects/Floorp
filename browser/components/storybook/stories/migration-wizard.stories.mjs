/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Imported for side-effects.
import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "toolkit-widgets/panel-list.js";
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
  {
    key: "chrome",
    displayName: "Chrome",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/chrome.png",
  },
  {
    key: "chrome",
    displayName: "Chrome",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "person-2", name: "Person 2" },
    brandImage: "chrome://browser/content/migration/brands/chrome.png",
  },
  {
    key: "ie",
    displayName: "Microsoft Internet Explorer",
    resourceTypes: ["HISTORY", "BOOKMARKS"],
    profile: null,
    brandImage: "chrome://browser/content/migration/brands/ie.png",
  },
  {
    key: "edge",
    displayName: "Microsoft Edge Legacy",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: null,
  },
  {
    key: "chromium-edge",
    displayName: "Microsoft Edge",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/edge.png",
  },
  {
    key: "brave",
    displayName: "Brave",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/brave.png",
  },
  {
    key: "internal-testing",
    displayName: "Internal Testing Migrator",
    resourceTypes: ["HISTORY", "PASSWORDS", "BOOKMARKS"],
    profile: null,
  },
  {
    key: "safari",
    displayName: "Safari",
    resourceTypes: ["HISTORY", "PASSWORDS", "BOOKMARKS"],
    profile: null,
    brandImage: "chrome://browser/content/migration/brands/safari.png",
  },
  {
    key: "opera",
    displayName: "Opera",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/opera.png",
  },
  {
    key: "opera-gx",
    displayName: "Opera GX",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/operagx.png",
  },
  {
    key: "vivaldi",
    displayName: "Vivaldi",
    resourceTypes: ["HISTORY"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/vivaldi.png",
  },
];

const Template = ({ state, dialogMode }) => html`
  <style>
    @media (prefers-reduced-motion: no-preference) {
      migration-wizard::part(progress-spinner) {
        mask: url(./migration/progress-mask.svg);
      }
    }
  </style>

  <div class="card card-no-hover" style="width: fit-content">
    <migration-wizard ?dialog-mode=${dialogMode} .state=${state}>
      <panel-list></panel-list>
    </migration-wizard>
  </div>
`;

export const LoadingSkeleton = Template.bind({});
LoadingSkeleton.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.LOADING,
  },
};

export const MainSelectorVariant1 = Template.bind({});
MainSelectorVariant1.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.SELECTION,
    migrators: FAKE_BROWSER_LIST,
    showImportAll: false,
  },
};

export const MainSelectorVariant2 = Template.bind({});
MainSelectorVariant2.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.SELECTION,
    migrators: FAKE_BROWSER_LIST,
    showImportAll: true,
  },
};

export const Progress = Template.bind({});
Progress.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.PROGRESS,
    key: "chrome",
    progress: {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]: {
        inProgress: true,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        inProgress: true,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        inProgress: true,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        inProgress: true,
      },
    },
  },
};

export const PartialProgress = Template.bind({});
PartialProgress.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.PROGRESS,
    key: "chrome",
    progress: {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]: {
        inProgress: true,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        inProgress: false,
        message: "14 logins and passwords",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        inProgress: true,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        inProgress: false,
        message: "Addresses, credit cards, form history",
      },
    },
  },
};

export const Success = Template.bind({});
Success.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.PROGRESS,
    key: "chrome",
    progress: {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]: {
        inProgress: false,
        message: "14 bookmarks",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        inProgress: false,
        message: "14 logins and passwords",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        inProgress: false,
        message: "From the last 180 days",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        inProgress: false,
        message: "Addresses, credit cards, form history",
      },
    },
  },
};

export const SafariPermissions = Template.bind({});
SafariPermissions.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.SAFARI_PERMISSION,
  },
};

export const SafariPasswordPermissions = Template.bind({});
SafariPasswordPermissions.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.SAFARI_PASSWORD_PERMISSION,
  },
};

export const NoBrowsersFound = Template.bind({});
NoBrowsersFound.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.NO_BROWSERS_FOUND,
  },
};
