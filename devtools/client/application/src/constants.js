/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionTypes = {
  // manifest substate
  UPDATE_MANIFEST: "UPDATE_MANIFEST",
  // page substate
  UPDATE_DOMAIN: "UPDATE_DOMAIN",
  // ui substate
  UPDATE_SELECTED_PAGE: "UPDATE_SELECTED_PAGE",
  // workers substate
  UPDATE_CAN_DEBUG_WORKERS: "UPDATE_CAN_DEBUG_WORKERS",
  UPDATE_WORKERS: "UPDATE_WORKERS",
};

const PAGE_TYPES = {
  MANIFEST: "manifest",
  SERVICE_WORKERS: "service-workers",
};

const DEFAULT_PAGE = PAGE_TYPES.MANIFEST;

const MANIFEST_DATA = {
  background_color: "#F9D",
  dir: "auto",
  display: "browser",
  icons: [
    {
      src:
        "https://design.firefox.com/icons/icons/desktop/default-browser-16.svg",
      type: "type/png",
      size: "16x16",
    },
    {
      src:
        "https://design.firefox.com/icons/icons/desktop/default-browser-16.svg",
      type: "type/png",
      size: "32x32",
    },
    {
      src:
        "https://design.firefox.com/icons/icons/desktop/default-browser-16.svg",
      type: "type/png",
      size: "64x64",
    },
  ],
  lang: "en-US",
  moz_manifest_url: "",
  moz_validation: [
    { warn: "Icons item at index 0 is invalid." },
    {
      warn:
        "Icons item at index 2 is invalid. Icons item at index 2 is invalid. Icons item at index 2 is invalid. Icons item at index 2 is invalid.",
    },
  ],
  name:
    "Name is a verrry long name and the name is longer tha you thinnk because it is loooooooooooooooooooooooooooooooooooooooooooooooong",
  orientation: "landscape",
  scope: "./",
  short_name: "Na",
  start_url: "root",
  theme_color: "#345",
};

// flatten constants
module.exports = Object.assign(
  {},
  { DEFAULT_PAGE, PAGE_TYPES, MANIFEST_DATA },
  actionTypes
);
