/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// The Compatibility panel detects issues by comparing against official MDN compatibility data
// at https://github.com/mdn/browser-compat-data). It uses a local snapshot of the dataset.
// This dataset needs to be manually synchronized periodically

// The subsets from the dataset required by the Compatibility panel are:
// * css.properties: https://github.com/mdn/browser-compat-data/tree/master/css

// The MDN compatibility data is available as a node package ("@mdn/browser-compat-data"),
// which is used here to update `../dataset/css-properties.json`.

/* global __dirname */

"use strict";

const compatData = require("@mdn/browser-compat-data");
const { properties } = compatData.css;

globalThis.loader = { lazyRequireGetter: () => {} };
// eslint-disable-next-line mozilla/reject-relative-requires
const MDNCompatibility = require("../../../server/actors/compatibility/lib/MDNCompatibility");

const mdnCompatibility = new MDNCompatibility(properties);
// Flatten all CSS properties aliases here so we don't have to do it at runtime,
// which is costly.
flattenAliases(mdnCompatibility._cssPropertiesCompatData);
parseBrowserVersion(mdnCompatibility._cssPropertiesCompatData);
exportData(mdnCompatibility._cssPropertiesCompatData, "css-properties.json");

/**
 * Builds a list of aliases between CSS properties, like flex and -webkit-flex,
 * and mutates individual entries in the web compat data store for CSS properties to
 * add their corresponding aliases.
 */
function flattenAliases(compatNode) {
  for (const term in compatNode) {
    if (term.startsWith("_")) {
      // Ignore exploring if the term is _aliasOf or __compat.
      continue;
    }

    const compatTable = mdnCompatibility._getCompatTable(compatNode, [term]);
    if (compatTable) {
      const aliases = findAliasesFrom(compatTable);

      for (const { alternative_name: name, prefix } of aliases) {
        const alias = name || prefix + term;
        compatNode[alias] = { _aliasOf: term };
      }

      if (aliases.length) {
        // Make the term accessible as the alias.
        compatNode[term]._aliasOf = term;
      }
    }

    // Flatten deeper node as well.
    flattenAliases(compatNode[term]);
  }
}

function findAliasesFrom(compatTable) {
  const aliases = [];

  for (const browser in compatTable.support) {
    let supportStates = compatTable.support[browser] || [];
    supportStates = Array.isArray(supportStates)
      ? supportStates
      : [supportStates];

    for (const { alternative_name: name, prefix } of supportStates) {
      if (!prefix && !name) {
        continue;
      }

      aliases.push({ alternative_name: name, prefix });
    }
  }

  return aliases;
}

function parseBrowserVersion(compatNode) {
  for (const term in compatNode) {
    if (term.startsWith("_")) {
      // Ignore exploring if the term is _aliasOf or __compat.
      continue;
    }

    const compatTable = mdnCompatibility._getCompatTable(compatNode, [term]);
    if (compatTable?.support) {
      for (const [browserId, supportItem] of Object.entries(
        compatTable.support
      )) {
        // supportItem is an array when there are info for both prefixed and non-prefixed
        // versions. If it's not an array in the original data, transform it into one
        // since we'd have to do it in MDNCompatibility at runtime otherwise.
        if (!Array.isArray(supportItem)) {
          compatTable.support[browserId] = [supportItem];
        }
        for (const item of compatTable.support[browserId]) {
          replaceVersionsInBrowserSupport(item);
        }
      }
    }

    // Handle deeper node as well.
    parseBrowserVersion(compatNode[term]);
  }
}

function replaceVersionsInBrowserSupport(browserSupport) {
  browserSupport.added = asFloatVersion(browserSupport.version_added);
  browserSupport.removed = asFloatVersion(browserSupport.version_removed);
}

function asFloatVersion(version) {
  // `version` is not always a string (can be null, or a boolean) and in such case,
  // we want to keep it that way.
  if (typeof version !== "string") {
    return version;
  }

  if (version.startsWith("\u2264")) {
    // MDN compatibility data started to use an expression like "≤66" for version.
    // We just ignore the character here.
    version = version.substring(1);
  }

  return parseFloat(version);
}

function exportData(data, fileName) {
  const fs = require("fs");
  const path = require("path");

  const content = `${JSON.stringify(data)}`;

  fs.writeFile(
    path.resolve(__dirname, "../dataset", fileName),
    content,
    err => {
      if (err) {
        console.error(err);
      } else {
        console.log(`${fileName} downloaded`);
      }
    }
  );
}
