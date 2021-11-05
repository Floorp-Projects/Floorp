/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BuiltInThemeConfig"];

/**
 * A list of themes built in to the browser. Params for the objects contained
 * within the array:
 * @param {string} id
 *   The unique identifier for the theme.
 * @param {string} version
 *   The theme add-on's semantic version, as defined in its manifest.
 * @param {string} path
 *   Path to the add-on files.
 * @param {string} [expiry]
 *  Date in YYYY-MM-DD format. Optional. If defined, the theme can no longer be
 *  used after this date, unless the user has permission to retain it.
 */
const BuiltInThemeConfig = [
  {
    id: "firefox-compact-light@mozilla.org",
    version: "1.2",
    path: "resource://builtin-themes/light/",
  },
  {
    id: "firefox-compact-dark@mozilla.org",
    version: "1.2",
    path: "resource://builtin-themes/dark/",
  },
  {
    id: "firefox-alpenglow@mozilla.org",
    version: "1.4",
    path: "resource://builtin-themes/alpenglow/",
  },
  {
    id: "lush-soft-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/lush/soft/",
    expiry: "2022-01-11",
  },
  {
    id: "lush-balanced-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/lush/balanced/",
    expiry: "2022-01-11",
  },
  {
    id: "lush-bold-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/lush/bold/",
    expiry: "2022-01-11",
  },
  {
    id: "abstract-soft-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/abstract/soft/",
    expiry: "2022-01-11",
  },
  {
    id: "abstract-balanced-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/abstract/balanced/",
    expiry: "2022-01-11",
  },
  {
    id: "abstract-bold-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/abstract/bold/",
    expiry: "2022-01-11",
  },
  {
    id: "elemental-soft-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/elemental/soft/",
    expiry: "2022-01-11",
  },
  {
    id: "elemental-balanced-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/elemental/balanced/",
    expiry: "2022-01-11",
  },
  {
    id: "elemental-bold-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/elemental/bold/",
    expiry: "2022-01-11",
  },
  {
    id: "cheers-soft-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/cheers/soft/",
    expiry: "2022-01-11",
  },
  {
    id: "cheers-balanced-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/cheers/balanced/",
    expiry: "2022-01-11",
  },
  {
    id: "cheers-bold-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/cheers/bold/",
    expiry: "2022-01-11",
  },
  {
    id: "graffiti-soft-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/graffiti/soft/",
    expiry: "2022-01-11",
  },
  {
    id: "graffiti-balanced-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/graffiti/balanced/",
    expiry: "2022-01-11",
  },
  {
    id: "graffiti-bold-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/graffiti/bold/",
    expiry: "2022-01-11",
  },
  {
    id: "foto-soft-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/foto/soft/",
    expiry: "2022-01-11",
  },
  {
    id: "foto-balanced-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/foto/balanced/",
    expiry: "2022-01-11",
  },
  {
    id: "foto-bold-colorway@mozilla.org",
    version: "1.0",
    path: "resource://builtin-themes/monochromatic/foto/bold/",
    expiry: "2022-01-11",
  },
];
