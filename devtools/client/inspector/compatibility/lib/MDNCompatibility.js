/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const _SUPPORT_STATE = {
  BROWSER_NOT_FOUND: "BROWSER_NOT_FOUND",
  DATA_NOT_FOUND: "DATA_NOT_FOUND",
  SUPPORTED: "SUPPORTED",
  UNSUPPORTED: "UNSUPPORTED",
};

const _ISSUE_TYPE = {
  CSS_PROPERTY: "CSS_PROPERTY",
  CSS_PROPERTY_ALIASES: "CSS_PROPERTY_ALIASES",
};

/**
 * A class with methods used to query the MDN compatibility data for CSS properties and
 * HTML nodes and attributes for specific browsers and versions.
 */
class MDNCompatibility {
  static get ISSUE_TYPE() {
    return _ISSUE_TYPE;
  }

  /**
   * Constructor.
   *
   * @param {JSON} cssPropertiesCompatData
   *        JSON of the compat data for CSS properties.
   *        https://github.com/mdn/browser-compat-data/tree/master/css/properties
   */
  constructor(cssPropertiesCompatData) {
    this._cssPropertiesCompatData = cssPropertiesCompatData;

    // Flatten all CSS properties aliases.
    this._flattenAliases(this._cssPropertiesCompatData);
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
    const {
      aliasSummaries,
      normalSummaries,
    } = this._classifyCSSCompatSummaries(summaries, browsers);

    // Finally, convert to CSS issues.
    return this._toCSSIssues(normalSummaries.concat(aliasSummaries));
  }

  _asFloatVersion(version = false) {
    if (version === true) {
      return 0;
    }

    if (version === false) {
      return Number.MAX_VALUE;
    }

    if (version.startsWith("\u2264")) {
      // MDN compatibility data started to use an expression like "≤66" for version.
      // We just ignore the character here.
      version = version.substring(1);
    }

    return parseFloat(version);
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
      const { database, invalid, terms, unsupportedBrowsers } = s;

      if (invalid) {
        return true;
      }

      const alias = this._getAlias(database, ...terms);
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
          })
        );
      }

      // Update alias summary.
      const terminal = terms.pop();
      const aliasSummary = aliasSummariesMap.get(alias);
      if (!aliasSummary.aliases.includes(terminal)) {
        aliasSummary.aliases.push(terminal);
      }
      aliasSummary.unsupportedBrowsers = aliasSummary.unsupportedBrowsers.filter(
        b => unsupportedBrowsers.includes(b)
      );
      return false;
    });

    return { aliasSummaries: [...aliasSummariesMap.values()], normalSummaries };
  }

  _findAliasesFrom(compatTable) {
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

  /**
   * Builds a list of aliases between CSS properties, like flex and -webkit-flex,
   * and mutates individual entries in the web compat data store for CSS properties to
   * add their corresponding aliases.
   */
  _flattenAliases(compatNode) {
    for (const term in compatNode) {
      if (term.startsWith("_")) {
        // Ignore exploring if the term is _aliasOf or __compat.
        continue;
      }

      const compatTable = this._getCompatTable(compatNode, [term]);
      if (compatTable) {
        const aliases = this._findAliasesFrom(compatTable);

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
      this._flattenAliases(compatNode[term]);
    }
  }

  _getAlias(compatNode, ...terms) {
    const targetNode = this._getCompatNode(compatNode, terms);
    return targetNode ? targetNode._aliasOf : null;
  }

  _getChildCompatNode(compatNode, term) {
    term = term.toLowerCase();

    let child = null;
    for (const field in compatNode) {
      if (field.toLowerCase() === term) {
        child = compatNode[field];
        break;
      }
    }

    if (!child) {
      return null;
    }

    if (child._aliasOf) {
      // If the node is an alias, returns the node the alias points.
      child = compatNode[child._aliasOf];
    }

    return child;
  }

  /**
   * Return a compatibility node which is target for `terms` parameter from `compatNode`
   * parameter. For example, when check `background-clip:  content-box;`, the `terms` will
   * be ["background-clip", "content-box"]. Then, follow the name of terms from the
   * compatNode node, return the target node. Although this function actually do more
   * complex a bit, if it says simply, returns a node of
   * compatNode["background-clip"]["content-box""] .
   */
  _getCompatNode(compatNode, terms) {
    for (const term of terms) {
      compatNode = this._getChildCompatNode(compatNode, term);
      if (!compatNode) {
        return null;
      }
    }

    return compatNode;
  }

  _getCompatTable(compatNode, terms) {
    let targetNode = this._getCompatNode(compatNode, terms);

    if (!targetNode) {
      return null;
    }

    if (!targetNode.__compat) {
      for (const field in targetNode) {
        // TODO: We don't have a way to know the context for now.
        // Thus, use first context node as the compat table.
        // e.g. flex_context of align-item
        // https://github.com/mdn/browser-compat-data/blob/master/css/properties/align-items.json#L5
        if (field.endsWith("_context")) {
          targetNode = targetNode[field];
          break;
        }
      }
    }

    return targetNode.__compat;
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
  _getCompatSummary(browsers, database, ...terms) {
    if (!this._hasTerm(database, ...terms)) {
      return { invalid: true, unsupportedBrowsers: [] };
    }

    const unsupportedBrowsers = browsers.filter(browser => {
      const state = this._getSupportState(browser, database, ...terms);
      return state === _SUPPORT_STATE.UNSUPPORTED;
    });
    const { deprecated, experimental } = this._getStatus(database, ...terms);
    const url = this._getMDNLink(database, ...terms);

    return {
      database,
      terms,
      url,
      deprecated,
      experimental,
      unsupportedBrowsers,
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
      property
    );
    return Object.assign(summary, { property });
  }

  _getMDNLink(compatNode, ...terms) {
    for (; terms.length > 0; terms.pop()) {
      const compatTable = this._getCompatTable(compatNode, terms);
      const url = compatTable ? compatTable.mdn_url : null;

      if (url) {
        return url;
      }
    }

    return null;
  }

  _getSupportState(browser, compatNode, ...terms) {
    const compatTable = this._getCompatTable(compatNode, terms);
    if (!compatTable) {
      return _SUPPORT_STATE.DATA_NOT_FOUND;
    }

    let supportList = compatTable.support[browser.id];
    if (!supportList) {
      return _SUPPORT_STATE.BROWSER_NOT_FOUND;
    }

    supportList = Array.isArray(supportList) ? supportList : [supportList];
    const version = parseFloat(browser.version);
    const terminal = terms[terms.length - 1];
    const match = terminal.match(/^-\w+-/);
    const prefix = match ? match[0] : null;

    for (const support of supportList) {
      if ((!support.prefix && !prefix) || support.prefix === prefix) {
        const { version_added: added, version_removed: removed } = support;
        const addedVersion = this._asFloatVersion(
          added === null ? true : added
        );
        const removedVersion = this._asFloatVersion(
          removed === null ? false : removed
        );

        if (addedVersion <= version && version < removedVersion) {
          return _SUPPORT_STATE.SUPPORTED;
        }
      }
    }

    return _SUPPORT_STATE.UNSUPPORTED;
  }

  _getStatus(compatNode, ...terms) {
    const compatTable = this._getCompatTable(compatNode, terms);
    return compatTable ? compatTable.status : {};
  }

  _hasIssue({ unsupportedBrowsers, deprecated, experimental, invalid }) {
    // Don't apply as issue the invalid term which was not in the database.
    return (
      !invalid && (unsupportedBrowsers.length || deprecated || experimental)
    );
  }

  _hasTerm(compatNode, ...terms) {
    return !!this._getCompatTable(compatNode, terms);
  }

  _toIssue(summary, type) {
    const issue = Object.assign({}, summary, { type });
    delete issue.database;
    delete issue.terms;
    return issue;
  }

  _toCSSIssues(summaries) {
    const issues = [];

    for (const summary of summaries) {
      if (!this._hasIssue(summary)) {
        continue;
      }

      const type = summary.aliases
        ? _ISSUE_TYPE.CSS_PROPERTY_ALIASES
        : _ISSUE_TYPE.CSS_PROPERTY;
      issues.push(this._toIssue(summary, type));
    }

    return issues;
  }
}

module.exports = MDNCompatibility;
