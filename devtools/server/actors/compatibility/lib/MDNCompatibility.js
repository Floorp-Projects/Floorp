/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const _SUPPORT_STATE_BROWSER_NOT_FOUND = "BROWSER_NOT_FOUND";
const _SUPPORT_STATE_SUPPORTED = "SUPPORTED";
const _SUPPORT_STATE_UNSUPPORTED = "UNSUPPORTED";
const _SUPPORT_STATE_UNSUPPORTED_PREFIX_NEEDED = "UNSUPPORTED_PREFIX_NEEDED";

loader.lazyRequireGetter(
  this,
  "COMPATIBILITY_ISSUE_TYPE",
  "resource://devtools/shared/constants.js",
  true
);

loader.lazyRequireGetter(
  this,
  ["getCompatNode", "getCompatTable"],
  "resource://devtools/shared/compatibility/helpers.js",
  true
);

const PREFIX_REGEX = /^-\w+-/;

/**
 * A class with methods used to query the MDN compatibility data for CSS properties and
 * HTML nodes and attributes for specific browsers and versions.
 */
class MDNCompatibility {
  /**
   * Constructor.
   *
   * @param {JSON} cssPropertiesCompatData
   *        JSON of the compat data for CSS properties.
   *        https://github.com/mdn/browser-compat-data/tree/master/css/properties
   */
  constructor(cssPropertiesCompatData) {
    this._cssPropertiesCompatData = cssPropertiesCompatData;
  }

  /**
   * Return the CSS related compatibility issues from given CSS declaration blocks.
   *
   * @param {Array} declarations
   *                CSS declarations to check.
   *                e.g. [{ name: "background-color", value: "lime" }, ...]
   * @param {Array} browsers
   *                Restrict compatibility checks to these browsers and versions.
   *                e.g. [{ id: "firefox", name: "Firefox", version: "68" }, ...]
   * @return {Array} issues
   */
  getCSSDeclarationBlockIssues(declarations, browsers) {
    const summaries = [];
    for (const { name: property } of declarations) {
      // Ignore CSS custom properties as any name is valid.
      if (property.startsWith("--")) {
        continue;
      }

      summaries.push(this._getCSSPropertyCompatSummary(browsers, property));
    }

    // Classify to aliases summaries and normal summaries.
    const { aliasSummaries, normalSummaries } =
      this._classifyCSSCompatSummaries(summaries, browsers);

    // Finally, convert to CSS issues.
    return this._toCSSIssues(normalSummaries.concat(aliasSummaries));
  }

  /**
   * Classify the compatibility summaries that are able to get from
   * `getCSSPropertyCompatSummary`.
   * There are CSS properties that can specify the style with plural aliases such as
   * `user-select`, aggregates those as the aliases summaries.
   *
   * @param {Array} summaries
   *                Assume the result of _getCSSPropertyCompatSummary().
   * @param {Array} browsers
   *                All browsers that to check
   *                e.g. [{ id: "firefox", name: "Firefox", version: "68" }, ...]
   * @return Object
   *                {
   *                  aliasSummaries: Array of alias summary,
   *                  normalSummaries: Array of normal summary
   *                }
   */
  _classifyCSSCompatSummaries(summaries, browsers) {
    const aliasSummariesMap = new Map();
    const normalSummaries = summaries.filter(s => {
      const {
        database,
        invalid,
        terms,
        unsupportedBrowsers,
        prefixNeededBrowsers,
      } = s;

      if (invalid) {
        return true;
      }

      const alias = this._getAlias(database, terms);
      if (!alias) {
        return true;
      }

      if (!aliasSummariesMap.has(alias)) {
        aliasSummariesMap.set(
          alias,
          Object.assign(s, {
            property: alias,
            aliases: [],
            unsupportedBrowsers: browsers,
            prefixNeededBrowsers: browsers,
          })
        );
      }

      // Update alias summary.
      const terminal = terms.pop();
      const aliasSummary = aliasSummariesMap.get(alias);
      if (!aliasSummary.aliases.includes(terminal)) {
        aliasSummary.aliases.push(terminal);
      }
      aliasSummary.unsupportedBrowsers =
        aliasSummary.unsupportedBrowsers.filter(b =>
          unsupportedBrowsers.includes(b)
        );
      aliasSummary.prefixNeededBrowsers =
        aliasSummary.prefixNeededBrowsers.filter(b =>
          prefixNeededBrowsers.includes(b)
        );
      return false;
    });

    const aliasSummaries = [...aliasSummariesMap.values()].map(s => {
      s.prefixNeeded = s.prefixNeededBrowsers.length !== 0;
      return s;
    });

    return { aliasSummaries, normalSummaries };
  }

  _getAlias(compatNode, terms) {
    const targetNode = getCompatNode(compatNode, terms);
    return targetNode ? targetNode._aliasOf : null;
  }

  /**
   * Return the compatibility summary of the terms.
   *
   * @param {Array} browsers
   *                All browsers that to check
   *                e.g. [{ id: "firefox", name: "Firefox", version: "68" }, ...]
   * @param {Array} database
   *                MDN compatibility dataset where finds from
   * @param {Array} terms
   *                The terms which is checked the compatibility summary from the
   *                database. The paremeters are passed as `rest parameters`.
   *                e.g. _getCompatSummary(browsers, database, "user-select", ...)
   * @return {Object}
   *                {
   *                  database: The passed database as a parameter,
   *                  terms: The passed terms as a parameter,
   *                  url: The link which indicates the spec in MDN,
   *                  deprecated: true if the spec of terms is deprecated,
   *                  experimental: true if the spec of terms is experimental,
   *                  unsupportedBrowsers: Array of unsupported browsers,
   *                }
   */
  _getCompatSummary(browsers, database, terms) {
    const compatTable = getCompatTable(database, terms);

    if (!compatTable) {
      return { invalid: true, unsupportedBrowsers: [] };
    }

    const unsupportedBrowsers = [];
    const prefixNeededBrowsers = [];

    for (const browser of browsers) {
      const state = this._getSupportState(
        compatTable,
        browser,
        database,
        terms
      );

      switch (state) {
        case _SUPPORT_STATE_UNSUPPORTED_PREFIX_NEEDED: {
          prefixNeededBrowsers.push(browser);
          unsupportedBrowsers.push(browser);
          break;
        }
        case _SUPPORT_STATE_UNSUPPORTED: {
          unsupportedBrowsers.push(browser);
          break;
        }
      }
    }

    const { deprecated, experimental } = compatTable.status || {};

    return {
      database,
      terms,
      url: compatTable.mdn_url,
      specUrl: compatTable.spec_url,
      deprecated,
      experimental,
      unsupportedBrowsers,
      prefixNeededBrowsers,
    };
  }

  /**
   * Return the compatibility summary of the CSS property.
   * This function just adds `property` filed to the result of `_getCompatSummary`.
   *
   * @param {Array} browsers
   *                All browsers that to check
   *                e.g. [{ id: "firefox", name: "Firefox", version: "68" }, ...]
   * @return {Object} compatibility summary
   */
  _getCSSPropertyCompatSummary(browsers, property) {
    const summary = this._getCompatSummary(
      browsers,
      this._cssPropertiesCompatData,
      [property]
    );
    return Object.assign(summary, { property });
  }

  _getSupportState(compatTable, browser, compatNode, terms) {
    const supportList = compatTable.support[browser.id];
    if (!supportList) {
      return _SUPPORT_STATE_BROWSER_NOT_FOUND;
    }

    const version = parseFloat(browser.version);
    const terminal = terms.at(-1);
    const prefix = terminal.match(PREFIX_REGEX)?.[0];

    let prefixNeeded = false;
    for (const support of supportList) {
      const { alternative_name: alternativeName, added, removed } = support;

      if (
        // added id true when feature is supported, but we don't know the version
        (added === true ||
          // `null` and `undefined` is when we don't know if it's supported.
          // Since we don't want to have false negative, we consider it as supported
          added === null ||
          added === undefined ||
          // It was added on a previous version number
          added <= version) &&
        // `added` is false when the property isn't supported
        added !== false &&
        // `removed` is false when the feature wasn't removevd
        (removed === false ||
          // `null` and `undefined` is when we don't know if it was removed.
          // Since we don't want to have false negative, we consider it as supported
          removed === null ||
          removed === undefined ||
          // It was removed, but on a later version, so it's still supported
          version <= removed)
      ) {
        if (alternativeName) {
          if (alternativeName === terminal) {
            return _SUPPORT_STATE_SUPPORTED;
          }
        } else if (
          support.prefix === prefix ||
          // There are compat data that are defined with prefix like "-moz-binding".
          // In this case, we don't have to check the prefix.
          (prefix && !this._getAlias(compatNode, terms))
        ) {
          return _SUPPORT_STATE_SUPPORTED;
        }

        prefixNeeded = true;
      }
    }

    return prefixNeeded
      ? _SUPPORT_STATE_UNSUPPORTED_PREFIX_NEEDED
      : _SUPPORT_STATE_UNSUPPORTED;
  }

  _hasIssue({ unsupportedBrowsers, deprecated, experimental, invalid }) {
    // Don't apply as issue the invalid term which was not in the database.
    return (
      !invalid && (unsupportedBrowsers.length || deprecated || experimental)
    );
  }

  _toIssue(summary, type) {
    const issue = Object.assign({}, summary, { type });
    delete issue.database;
    delete issue.terms;
    delete issue.prefixNeededBrowsers;
    return issue;
  }

  _toCSSIssues(summaries) {
    const issues = [];

    for (const summary of summaries) {
      if (!this._hasIssue(summary)) {
        continue;
      }

      const type = summary.aliases
        ? COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES
        : COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY;
      issues.push(this._toIssue(summary, type));
    }

    return issues;
  }
}

module.exports = MDNCompatibility;
