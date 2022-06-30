/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["_applyColorwayConfig", "BuiltInThemeConfig"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * A Map of themes built in to the browser, alongwith a Map of collections those themes belong to. Params for the objects contained
 * within the map:
 * @param {string} id
 *   The unique identifier for the theme. The map's key.
 * @param {string} version
 *   The theme add-on's semantic version, as defined in its manifest.
 * @param {string} path
 *   Path to the add-on files.
 * @param {string} [expiry]
 *  Date in YYYY-MM-DD format. Optional. If defined, the themes in the collection can no longer be
 *  used after this date, unless the user has permission to retain it.
 * @param {string} [collection]
 *  The collection id that the theme is a part of. Optional.
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
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022red/",
      collection: "true-colors",
    },
  ],
  [
    "2022orange-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022orange/",
      collection: "true-colors",
    },
  ],
  [
    "2022green-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022green/",
      collection: "true-colors",
    },
  ],
  [
    "2022yellow-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022yellow/",
      collection: "true-colors",
    },
  ],
  [
    "2022purple-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022purple/",
      collection: "true-colors",
    },
  ],
  [
    "2022blue-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022blue/",
      collection: "true-colors",
    },
  ],
  [
    "lush-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021lush/soft/",
      collection: "life-in-color",
    },
  ],
  [
    "lush-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021lush/balanced/",
      collection: "life-in-color",
    },
  ],
  [
    "lush-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021lush/bold/",
      collection: "life-in-color",
    },
  ],
  [
    "abstract-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021abstract/soft/",
      collection: "life-in-color",
    },
  ],
  [
    "abstract-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021abstract/balanced/",
      collection: "life-in-color",
    },
  ],
  [
    "abstract-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021abstract/bold/",
      collection: "life-in-color",
    },
  ],
  [
    "elemental-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021elemental/soft/",
      collection: "life-in-color",
    },
  ],
  [
    "elemental-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021elemental/balanced/",
      collection: "life-in-color",
    },
  ],
  [
    "elemental-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021elemental/bold/",
      collection: "life-in-color",
    },
  ],
  [
    "cheers-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021cheers/soft/",
      collection: "life-in-color",
    },
  ],
  [
    "cheers-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021cheers/balanced/",
      collection: "life-in-color",
    },
  ],
  [
    "cheers-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021cheers/bold/",
      collection: "life-in-color",
    },
  ],
  [
    "graffiti-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021graffiti/soft/",
      collection: "life-in-color",
    },
  ],
  [
    "graffiti-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021graffiti/balanced/",
      collection: "life-in-color",
    },
  ],
  [
    "graffiti-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021graffiti/bold/",
      collection: "life-in-color",
    },
  ],
  [
    "foto-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021foto/soft/",
      collection: "life-in-color",
    },
  ],
  [
    "foto-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021foto/balanced/",
      collection: "life-in-color",
    },
  ],
  [
    "foto-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2021foto/bold/",
      collection: "life-in-color",
    },
  ],
  [
    "playmaker-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022playmaker/soft/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-playmaker.avif",
      collection: "independent-voices",
    },
  ],
  [
    "playmaker-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022playmaker/balanced/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-playmaker.avif",
      collection: "independent-voices",
    },
  ],
  [
    "playmaker-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022playmaker/bold/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-playmaker.avif",
      collection: "independent-voices",
    },
  ],
  [
    "expressionist-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022expressionist/soft/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-expressionist.avif",
      collection: "independent-voices",
    },
  ],
  [
    "expressionist-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022expressionist/balanced/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-expressionist.avif",
      collection: "independent-voices",
    },
  ],
  [
    "expressionist-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022expressionist/bold/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-expressionist.avif",
      collection: "independent-voices",
    },
  ],
  [
    "visionary-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022visionary/soft/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-visionary.avif",
      collection: "independent-voices",
    },
  ],
  [
    "visionary-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022visionary/balanced/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-visionary.avif",
      collection: "independent-voices",
    },
  ],
  [
    "visionary-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022visionary/bold/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-visionary.avif",
      collection: "independent-voices",
    },
  ],
  [
    "activist-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022activist/soft/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-activist.avif",
      collection: "independent-voices",
    },
  ],
  [
    "activist-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022activist/balanced/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-activist.avif",
      collection: "independent-voices",
    },
  ],
  [
    "activist-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022activist/bold/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-activist.avif",
      collection: "independent-voices",
    },
  ],
  [
    "dreamer-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022dreamer/soft/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-dreamer.avif",
      collection: "independent-voices",
    },
  ],
  [
    "dreamer-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022dreamer/balanced/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-dreamer.avif",
      collection: "independent-voices",
    },
  ],
  [
    "dreamer-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022dreamer/bold/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-dreamer.avif",
      collection: "independent-voices",
    },
  ],
  [
    "innovator-soft-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022innovator/soft/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-innovator.avif",
      collection: "independent-voices",
    },
  ],
  [
    "innovator-balanced-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022innovator/balanced/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-innovator.avif",
      collection: "independent-voices",
    },
  ],
  [
    "innovator-bold-colorway@mozilla.org",
    {
      version: "1.1",
      path: "resource://builtin-themes/colorways/2022innovator/bold/",
      figureUrl:
        "chrome://browser/content/colorways/independent-voices-innovator.avif",
      collection: "independent-voices",
    },
  ],
]);

const colorwayClosetEnabled = Services.prefs.getBoolPref(
  "browser.theme.colorway-closet"
);

const ColorwayCollections = [
  {
    id: "life-in-color",
    expiry: "2022-02-08",
    l10nId: {
      title: "colorway-collection-life-in-color",
    },
  },
  {
    id: "true-colors",
    expiry: "2022-05-03",
    l10nId: {
      title: "colorway-collection-true-colors",
    },
  },
  {
    id: "independent-voices",
    expiry:
      colorwayClosetEnabled && AppConstants.NIGHTLY_BUILD
        ? "2022-12-31"
        : "1970-01-01",
    l10nId: {
      title: "colorway-collection-independent-voices",
      description: "colorway-collection-independent-voices-description",
    },
    cardImagePath:
      "chrome://browser/content/colorways/independent-voices-collection-banner.avif",
    iconUrl:
      "chrome://browser/content/colorways/independent-voices-collection.avif",
  },
];

function _applyColorwayConfig(collections) {
  const collectionsSorted = collections
    .map(({ expiry, ...rest }) => ({
      expiry: new Date(expiry),
      ...rest,
    }))
    .sort((a, b) => a.expiry - b.expiry);
  const collectionsMap = collectionsSorted.reduce((map, c) => {
    map.set(c.id, c);
    return map;
  }, new Map());
  for (let [key, value] of BuiltInThemeConfig.entries()) {
    if (value.collection) {
      const collectionConfig = collectionsMap.get(value.collection);
      BuiltInThemeConfig.set(key, {
        ...value,
        ...collectionConfig,
      });
    }
  }
  BuiltInThemeConfig.findActiveColorwayCollection = now => {
    let collection = null;
    let start = 0;
    let end = collectionsSorted.length - 1;
    while (start <= end) {
      const mid = Math.floor((start + end) / 2);
      const c = collectionsSorted[mid];
      const diff = c.expiry - now;
      if (diff < 0) {
        // collection expired, look for newer one
        start = mid + 1;
      } else {
        // collection not expired, check for older one
        collection = c;
        end = mid - 1;
      }
    }
    return collection;
  };
}

_applyColorwayConfig(ColorwayCollections);
