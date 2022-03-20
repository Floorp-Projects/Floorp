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
      version: "1.3",
      path: "resource://builtin-themes/light/",
    },
  ],
  [
    "firefox-compact-dark@mozilla.org",
    {
      version: "1.3",
      path: "resource://builtin-themes/dark/",
    },
  ],
  [
    "floorp-edge@mozilla.org",
    {
      version: "1.1.0",
      path: "resource://builtin-themes/motion/",
    },
  ],
  [
    "floorp-lepton@mozilla.org",
    {
      version: "1.1.0",
      path: "resource://builtin-themes/lepton/",
    },
  ],
  [
    "kanaze-erua@floorp.ablaze.one",
    {
      version: "1.0",
      path: "resource://builtin-themes/erua/",
      expiry: "2022-04-15",
    },
  ],
  [
    "kanaze-aria@floorp.ablaze.one",
    {
      version: "1.0",
      path: "resource://builtin-themes/aria/",
      expiry: "2022-04-15",
    },
  ],
  [
    "kanaze-melt@floorp.ablaze.one",
    {
      version: "1.0",
      path: "resource://builtin-themes/melt/",
      expiry: "2022-04-15",
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
    },
  ],
  [
    "lush-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/lush/balanced/",
    },
  ],
  [
    "lush-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/lush/bold/",
    },
  ],
  [
    "abstract-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/abstract/soft/",
    },
  ],
  [
    "abstract-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/abstract/balanced/",
    },
  ],
  [
    "abstract-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/abstract/bold/",
    },
  ],
  [
    "elemental-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/elemental/soft/",
    },
  ],
  [
    "elemental-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/elemental/balanced/",
    },
  ],
  [
    "elemental-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/elemental/bold/",
    },
  ],
  [
    "cheers-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/cheers/soft/",
    },
  ],
  [
    "cheers-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/cheers/balanced/",
    },
  ],
  [
    "cheers-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/cheers/bold/",
    },
  ],
  [
    "graffiti-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/graffiti/soft/",
    },
  ],
  [
    "graffiti-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/graffiti/balanced/",
    },
  ],
  [
    "graffiti-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/graffiti/bold/",
    },
  ],
  [
    "foto-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/foto/soft/",
    },
  ],
  [
    "foto-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/foto/balanced/",
    },
  ],
  [
    "foto-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "resource://builtin-themes/monochromatic/foto/bold/",
    },
  ],
]);
