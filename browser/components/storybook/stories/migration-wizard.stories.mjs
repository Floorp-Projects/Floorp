/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Imported for side-effects.
import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/panel-list.js";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/migration/content/migration-wizard.mjs";
import { MigrationWizardConstants } from "chrome://browser/content/migration/migration-wizard-constants.mjs";

// Imported for side-effects.
// eslint-disable-next-line import/no-unassigned-import
import "toolkit-widgets/named-deck.js";

export default {
  title: "Domain-specific UI Widgets/Migration Wizard",
  component: "migration-wizard",
};

const FAKE_MIGRATOR_LIST = [
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "chrome",
    displayName: "Chrome",
    resourceTypes: [
      "HISTORY",
      "FORMDATA",
      "PASSWORDS",
      "BOOKMARKS",
      "PAYMENT_METHODS",
      "EXTENSIONS",
    ],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/chrome.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "chrome",
    displayName: "Chrome",
    resourceTypes: [
      "HISTORY",
      "FORMDATA",
      "PASSWORDS",
      "BOOKMARKS",
      "PAYMENT_METHODS",
      "EXTENSIONS",
    ],
    profile: { id: "person-2", name: "Person 2" },
    brandImage: "chrome://browser/content/migration/brands/chrome.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "ie",
    displayName: "Microsoft Internet Explorer",
    resourceTypes: ["HISTORY", "BOOKMARKS"],
    profile: null,
    brandImage: "chrome://global/skin/icons/defaultFavicon.svg",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "edge",
    displayName: "Microsoft Edge Legacy",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: null,
    brandImage: "chrome://global/skin/icons/defaultFavicon.svg",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "chromium-edge",
    displayName: "Microsoft Edge",
    resourceTypes: [
      "HISTORY",
      "FORMDATA",
      "PASSWORDS",
      "BOOKMARKS",
      "PAYMENT_METHODS",
    ],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/edge.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "brave",
    displayName: "Brave",
    resourceTypes: [
      "HISTORY",
      "FORMDATA",
      "PASSWORDS",
      "BOOKMARKS",
      "PAYMENT_METHODS",
    ],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/brave.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "internal-testing",
    displayName: "Internal Testing Migrator",
    resourceTypes: ["HISTORY", "PASSWORDS", "BOOKMARKS"],
    profile: null,
    brandImage: "chrome://global/skin/icons/defaultFavicon.svg",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "safari",
    displayName: "Safari",
    resourceTypes: ["HISTORY", "PASSWORDS", "BOOKMARKS"],
    profile: null,
    brandImage: "chrome://browser/content/migration/brands/safari.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "opera",
    displayName: "Opera",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/opera.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "opera-gx",
    displayName: "Opera GX",
    resourceTypes: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/operagx.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "vivaldi",
    displayName: "Vivaldi",
    resourceTypes: ["HISTORY"],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://browser/content/migration/brands/vivaldi.png",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
    key: "no-resources-browser",
    displayName: "Browser with no resources",
    resourceTypes: [],
    profile: { id: "Default", name: "Default" },
    brandImage: "chrome://global/skin/icons/defaultFavicon.svg",
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.FILE,
    key: "file-password-csv",
    displayName: "Passwords from CSV file",
    brandImage: "chrome://branding/content/document.ico",
    resourceTypes: [],
    hasPermissions: true,
  },
  {
    type: MigrationWizardConstants.MIGRATOR_TYPES.FILE,
    key: "file-bookmarks",
    displayName: "Bookmarks from file",
    brandImage: "chrome://branding/content/document.ico",
    resourceTypes: [],
    hasPermissions: true,
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
    <migration-wizard
      ?dialog-mode=${dialogMode}
      .state=${state}
    ></migration-wizard>
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
    migrators: FAKE_MIGRATOR_LIST,
    showImportAll: false,
  },
};

export const MainSelectorVariant2 = Template.bind({});
MainSelectorVariant2.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.SELECTION,
    migrators: FAKE_MIGRATOR_LIST,
    showImportAll: true,
  },
};

export const NoPermissionMessage = Template.bind({});
NoPermissionMessage.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.SELECTION,
    migrators: [
      {
        type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
        key: "chromium",
        displayName: "Chromium",
        resourceTypes: [],
        profile: null,
        brandImage: "chrome://browser/content/migration/brands/chromium.png",
        hasPermissions: false,
        permissionsPath: "/home/user/snap/chromium/common/chromium",
      },
      {
        type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
        key: "safari",
        displayName: "Safari",
        resourceTypes: ["HISTORY", "PASSWORDS", "BOOKMARKS"],
        profile: null,
        brandImage: "chrome://browser/content/migration/brands/safari.png",
        hasPermissions: false,
        permissionsPath: "/Users/user/Library/Safari",
      },
      {
        type: MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
        key: "vivaldi",
        displayName: "Vivaldi",
        resourceTypes: ["HISTORY"],
        profile: { id: "Default", name: "Default" },
        brandImage: "chrome://browser/content/migration/brands/vivaldi.png",
        hasPermissions: true,
      },
      {
        type: MigrationWizardConstants.MIGRATOR_TYPES.FILE,
        key: "file-password-csv",
        displayName: "Passwords from CSV file",
        brandImage: "chrome://branding/content/document.ico",
        resourceTypes: [],
        hasPermissions: true,
      },
      {
        type: MigrationWizardConstants.MIGRATOR_TYPES.FILE,
        key: "file-bookmarks",
        displayName: "Bookmarks from file",
        brandImage: "chrome://branding/content/document.ico",
        resourceTypes: [],
        hasPermissions: true,
      },
    ],
    showImportAll: false,
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
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PAYMENT_METHODS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
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
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 logins and passwords",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "10 extensions",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "Addresses, credit cards, form history",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PAYMENT_METHODS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "6 payment methods",
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
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 bookmarks",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 logins and passwords",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "From the last 180 days",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "1 extension",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "Addresses, credit cards, form history",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PAYMENT_METHODS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "6 payment methods",
      },
    },
  },
};

export const SuccessWithWarnings = Template.bind({});
SuccessWithWarnings.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.PROGRESS,
    key: "chrome",
    progress: {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 bookmarks",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.WARNING,
        message: "Something didn't work correctly.",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "From the last 180 days",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "1 extension",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "Addresses, credit cards, form history",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PAYMENT_METHODS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "6 payment methods",
      },
    },
  },
};

export const ExtensionsPartialSuccess = Template.bind({});
ExtensionsPartialSuccess.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.PROGRESS,
    key: "chrome",
    progress: {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 bookmarks",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 logins and passwords",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "From the last 180 days",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.INFO,
        message: "5 of 10 extensions",
        linkText: "Learn how Firefox matches extensions",
        linkURL:
          "https://support.mozilla.org/kb/import-data-another-browser#w_import-extensions-from-chrome/",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "Addresses, credit cards, form history",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PAYMENT_METHODS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "6 payment methods",
      },
    },
  },
};

export const ExtensionsImportFailure = Template.bind({});
ExtensionsImportFailure.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.PROGRESS,
    key: "chrome",
    progress: {
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 bookmarks",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 logins and passwords",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "From the last 180 days",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.WARNING,
        message: "No matching extensions",
        linkText: "Browse extensions for Firefox",
        linkURL: "https://addons.mozilla.org/",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "Addresses, credit cards, form history",
      },
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PAYMENT_METHODS]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "6 payment methods",
      },
    },
  },
};

export const FileImportProgress = Template.bind({});
FileImportProgress.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.FILE_IMPORT_PROGRESS,
    title: "Importing Passwords",
    progress: {
      [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES
        .PASSWORDS_FROM_FILE]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.LOADING,
      },
    },
  },
};

export const FileImportSuccess = Template.bind({});
FileImportSuccess.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.FILE_IMPORT_PROGRESS,
    title: "Passwords Imported Successfully",
    progress: {
      [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES.PASSWORDS_NEW]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "2 added",
      },
      [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES
        .PASSWORDS_UPDATED]: {
        value: MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        message: "14 updated",
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
    hasFileMigrators: true,
  },
};

export const FileImportError = Template.bind({});
FileImportError.args = {
  dialogMode: true,
  state: {
    page: MigrationWizardConstants.PAGES.SELECTION,
    migrators: FAKE_MIGRATOR_LIST,
    showImportAll: false,
    migratorKey: "file-password-csv",
    fileImportErrorMessage: "Some file import error message",
  },
};
