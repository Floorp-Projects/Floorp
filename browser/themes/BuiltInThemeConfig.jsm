/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BuiltInThemeConfig"];

/**
 * A Map of themes built in to the browser. Params for the objects contained
 * within the map:
 * @param {string} id
 *   The unique identifier for the theme. The map's key.
 * @param {string} version
 *   The theme add-on's semantic version, as defined in its manifest.
 * @param {string} path
 *   Path to the add-on files.
 * @param {string} [expiry]
 *  Date in YYYY-MM-DD format. Optional. If defined, the theme can no longer be
 *  used after this date, unless the user has permission to retain it.
 */
const BuiltInThemeConfig = new Map([
  [
    "firefox-compact-light@mozilla.org",
    {
      version: "1.2",
      path: "resource://builtin-themes/light/",
    },
  ],
  [
    "firefox-compact-dark@mozilla.org",
    {
      version: "1.2",
      path: "resource://builtin-themes/dark/",
    },
  ],
  [
    "firefox-alpenglow@mozilla.org",
    {
      version: "1.4",
      path: "resource://builtin-themes/alpenglow/",
    },
  ],
  [
    "2022red-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/2022red/",
      expiry: "2022-05-03",
    },
  ],
  [
    "2022orange-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/2022orange/",
      expiry: "2022-05-03",
    },
  ],
  [
    "2022green-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/2022green/",
      expiry: "2022-05-03",
    },
  ],
  [
    "2022yellow-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/2022yellow/",
      expiry: "2022-05-03",
    },
  ],
  [
    "2022purple-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/2022purple/",
      expiry: "2022-05-03",
    },
  ],
  [
    "2022blue-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/2022blue/",
      expiry: "2022-05-03",
    },
  ],
  [
    "lush-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/lush/soft/",
      expiry: "2022-02-08",
    },
  ],
  [
    "lush-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/lush/balanced/",
      expiry: "2022-02-08",
    },
  ],
  [
    "lush-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/lush/bold/",
      expiry: "2022-02-08",
    },
  ],
  [
    "abstract-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/abstract/soft/",
      expiry: "2022-02-08",
    },
  ],
  [
    "abstract-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/abstract/balanced/",
      expiry: "2022-02-08",
    },
  ],
  [
    "abstract-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/abstract/bold/",
      expiry: "2022-02-08",
    },
  ],
  [
    "elemental-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/elemental/soft/",
      expiry: "2022-02-08",
    },
  ],
  [
    "elemental-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/elemental/balanced/",
      expiry: "2022-02-08",
    },
  ],
  [
    "elemental-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/elemental/bold/",
      expiry: "2022-02-08",
    },
  ],
  [
    "cheers-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/cheers/soft/",
      expiry: "2022-02-08",
    },
  ],
  [
    "cheers-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/cheers/balanced/",
      expiry: "2022-02-08",
    },
  ],
  [
    "cheers-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/cheers/bold/",
      expiry: "2022-02-08",
    },
  ],
  [
    "graffiti-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/graffiti/soft/",
      expiry: "2022-02-08",
    },
  ],
  [
    "graffiti-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/graffiti/balanced/",
      expiry: "2022-02-08",
    },
  ],
  [
    "graffiti-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/graffiti/bold/",
      expiry: "2022-02-08",
    },
  ],
  [
    "foto-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/foto/soft/",
      expiry: "2022-02-08",
    },
  ],
  [
    "foto-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/foto/balanced/",
      expiry: "2022-02-08",
    },
  ],
  [
    "foto-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/foto/bold/",
      expiry: "2022-02-08",
    },
  ],
]);
