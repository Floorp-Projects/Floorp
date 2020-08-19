/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
var protocol = require("devtools/shared/protocol");
const { compatibilitySpec } = require("devtools/shared/specs/compatibility");

loader.lazyRequireGetter(
  this,
  "browsersDataset",
  "devtools/shared/compatibility/dataset/browsers.json"
);

loader.lazyGetter(this, "mdnCompatibility", () => {
  const MDNCompatibility = require("devtools/shared/compatibility/MDNCompatibility");
  const cssPropertiesCompatData = require("devtools/shared/compatibility/dataset/css-properties.json");
  return new MDNCompatibility(cssPropertiesCompatData);
});

const TARGET_BROWSER_ID = [
  "firefox",
  "firefox_android",
  "chrome",
  "chrome_android",
  "safari",
  "safari_ios",
  "edge",
];
const TARGET_BROWSER_STATUS = ["esr", "current", "beta", "nightly"];
const TARGET_BROWSER_PREF = "devtools.inspector.compatibility.target-browsers";

const CompatibilityActor = protocol.ActorClassWithSpec(compatibilitySpec, {
  /**
   * Create a CompatibilityActor.
   * CompatibilityActor is responsible for providing the compatibility information
   * for the web page using the data from the Inspector and the `MDNCompatibility`
   * and conveys them to the compatibility panel in the DevTool Inspector. Currently,
   * the `CompatibilityActor` only detects compatibility issues in the CSS declarations
   * but plans are in motion to extend it to evaluate compatibility information for
   * HTML and JavaScript.
   * The design below has the InspectorActor own the CompatibilityActor, but it's
   * possible we will want to move it into it's own panel in the future.
   *
   * @param inspector
   *    The InspectorActor that owns this CompatibilityActor.
   *
   * @constructor
   */
  initialize: function(inspector) {
    protocol.Actor.prototype.initialize.call(this, inspector.conn);
    this.inspector = inspector;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this.inspector = null;
  },

  _getDefaultTargetBrowsers() {
    // Retrieve the information that matches to the browser id and the status
    // from the browsersDataset.
    // For the structure of then browsersDataset,
    // see https://github.com/mdn/browser-compat-data/blob/master/browsers/firefox.json
    const targets = [];

    for (const id of TARGET_BROWSER_ID) {
      const { name, releases } = browsersDataset[id];

      for (const version in releases) {
        const { status } = releases[version];

        if (!TARGET_BROWSER_STATUS.includes(status)) {
          continue;
        }

        // MDN compat data might have browser data that have the same id and status.
        // e.g. https://github.com/mdn/browser-compat-data/commit/53453400ecb2a85e7750d99e2e0a1611648d1d56#diff-31a16f09157f13354db27821261604aa
        // In this case, replace to the browser that has newer version to keep uniqueness
        // by id and status.
        const target = { id, name, version, status };
        const index = targets.findIndex(
          t => target.id === t.id && target.status === t.status
        );

        if (index < 0) {
          targets.push(target);
          continue;
        }

        const existingTarget = targets[index];
        if (parseFloat(existingTarget.version) < parseFloat(target.version)) {
          targets[index] = target;
        }
      }
    }

    return targets;
  },

  _getTargetBrowsers() {
    const targetsString = Services.prefs.getCharPref(TARGET_BROWSER_PREF, "");
    return targetsString
      ? JSON.parse(targetsString)
      : this._getDefaultTargetBrowsers();
  },

  /**
   * Responsible for computing the compatibility issues for a given CSS declaration block
   * @param Array
   *  Array of CSS declaration object of the form:
   *    {
   *      // Declaration name
   *      name: <string>,
   *      // Declaration value
   *      value: <string>,
   *    }
   * @param object options
   *  `targetBrowsers`: Array of target browsers to be used to check CSS compatibility against.
   *     It is an Array of the following form
   *     {
   *       // Browser id as specified in `devtools/shared/compatibility/datasets/browser.json`
   *       id: <string>,
   *       name: <string>,
   *       version: <string>,
   *       // Browser status - esr, current, beta, nightly
   *       status: <string>,
   *     }
   * @returns An Array of JSON objects with compatibility information in following form:
   *    {
   *      // Type of compatibility issue
   *      type: <string>,
   *      // The CSS declaration that has compatibility issues
   *      property: <string>,
   *      // Alias to the given CSS property
   *      alias: <Array>,
   *      // Link to MDN documentation for the particular CSS rule
   *      url: <string>,
   *      deprecated: <boolean>,
   *      experimental: <boolean>,
   *      // An array of all the browsers that don't support the given CSS rule
   *      unsupportedBrowsers: <Array>,
   *    }
   */
  getCSSDeclarationBlockIssues: function(declarationBlock, options) {
    const targetBrowsers =
      (options && options.targetBrowsers) || this._getTargetBrowsers();
    return mdnCompatibility.getCSSDeclarationBlockIssues(
      declarationBlock,
      targetBrowsers
    );
  },

  /**
   * Responsible for computing the compatibility issues in the
   * CSS declaration of the given node.
   * @param NodeActor node
   * @param targetBrowsers Array
   *   An Array of JSON object of target browser to check compatibility against in following form:
   *   {
   *     // Browser id as specified in `devtools/shared/compatibility/datasets/browser.json`
   *     id: <string>,
   *     name: <string>,
   *     version: <string>,
   *     // Browser status - esr, current, beta, nightly
   *     status: <string>,
   *   }
   * @returns An Array of JSON objects with compatibility information in following form:
   *    {
   *      // Type of compatibility issue
   *      type: <string>,
   *      // The CSS declaration that has compatibility issues
   *      property: <string>,
   *      // Alias to the given CSS property
   *      alias: <Array>,
   *      // Link to MDN documentation for the particular CSS rule
   *      url: <string>,
   *      deprecated: <boolean>,
   *      experimental: <boolean>,
   *      // An array of all the browsers that don't support the given CSS rule
   *      unsupportedBrowsers: <Array>,
   *    }
   */
  async getNodeCssIssues(node, targetBrowsers) {
    const pageStyle = await this.inspector.getPageStyle();
    const styles = await pageStyle.getApplied(node, {
      skipPseudo: false,
    });

    const declarationBlocks = styles.entries
      .map(({ rule }) => {
        // Replace form() with a function to get minimal subset
        // of declarations from StyleRuleActor when such a
        // function lands in the StyleRuleActor
        const declarations = rule.form().declarations;
        if (!declarations) {
          return null;
        }
        return declarations.filter(d => !d.commentOffsets);
      })
      .filter(declarations => declarations && declarations.length);

    return declarationBlocks
      .map(declarationBlock =>
        mdnCompatibility.getCSSDeclarationBlockIssues(
          declarationBlock,
          targetBrowsers
        )
      )
      .flat()
      .reduce((issues, issue) => {
        // Get rid of duplicate issue
        return issues.find(
          i => i.type === issue.type && i.property === issue.property
        )
          ? issues
          : [...issues, issue];
      }, []);
  },
});

module.exports = {
  CompatibilityActor,
};
