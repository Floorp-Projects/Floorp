/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  compatibilitySpec,
} = require("resource://devtools/shared/specs/compatibility.js");

loader.lazyGetter(this, "mdnCompatibility", () => {
  const MDNCompatibility = require("resource://devtools/server/actors/compatibility/lib/MDNCompatibility.js");
  const cssPropertiesCompatData = require("resource://devtools/shared/compatibility/dataset/css-properties.json");
  return new MDNCompatibility(cssPropertiesCompatData);
});

class CompatibilityActor extends Actor {
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
  constructor(inspector) {
    super(inspector.conn, compatibilitySpec);
    this.inspector = inspector;
  }

  destroy() {
    super.destroy();
    this.inspector = null;
  }

  form() {
    return {
      actor: this.actorID,
    };
  }

  getTraits() {
    return {
      traits: {},
    };
  }

  /**
   * Responsible for computing the compatibility issues for a list of CSS declaration blocks
   *
   * @param {Array<Array<Object>>} domRulesDeclarations: An array of arrays of CSS declaration object
   * @param {string} domRulesDeclarations[][].name: Declaration name
   * @param {string} domRulesDeclarations[][].value: Declaration value
   * @param {Array<Object>} targetBrowsers: Array of target browsers () to be used to check CSS compatibility against
   * @param {string} targetBrowsers[].id: Browser id as specified in `devtools/shared/compatibility/datasets/browser.json`
   * @param {string} targetBrowsers[].name
   * @param {string} targetBrowsers[].version
   * @param {string} targetBrowsers[].status: Browser status - esr, current, beta, nightly
   * @returns {Array<Array<Object>>} An Array of arrays of JSON objects with compatibility
   *                                 information in following form:
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
  getCSSDeclarationBlockIssues(domRulesDeclarations, targetBrowsers) {
    return domRulesDeclarations.map(declarationBlock =>
      mdnCompatibility.getCSSDeclarationBlockIssues(
        declarationBlock,
        targetBrowsers
      )
    );
  }

  /**
   * Responsible for computing the compatibility issues in the
   * CSS declaration of the given node.
   * @param NodeActor node
   * @param targetBrowsers Array
   *   An Array of JSON object of target browser to check compatibility against in following form:
   *   {
   *     // Browser id as specified in `devtools/server/actors/compatibility/lib/datasets/browser.json`
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
  }
}

exports.CompatibilityActor = CompatibilityActor;
