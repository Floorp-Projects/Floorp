/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");
const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    Reflect: "resource://gre/modules/reflect.sys.mjs",
  },
  { global: "contextual" }
);

/**
 * Gets a collection of parser methods for a specified source.
 *
 * @param string source
 *        The source text content.
 * @param boolean logExceptions
 */
function getSyntaxTrees(source, logExceptions) {
  // The source may not necessarily be JS, in which case we need to extract
  // all the scripts. Fastest/easiest way is with a regular expression.
  // Don't worry, the rules of using a <script> tag are really strict,
  // this will work.
  const regexp = /<script[^>]*?(?:>([^]*?)<\/script\s*>|\/>)/gim;
  const syntaxTrees = [];
  const scriptMatches = [];
  let scriptMatch;

  if (source.match(/^\s*</)) {
    // First non whitespace character is &lt, so most definitely HTML.
    while ((scriptMatch = regexp.exec(source))) {
      // Contents are captured at index 1 or nothing: Self-closing scripts
      // won't capture code content
      scriptMatches.push(scriptMatch[1] || "");
    }
  }

  // If there are no script matches, send the whole source directly to the
  // reflection API to generate the AST nodes.
  if (!scriptMatches.length) {
    // Reflect.parse throws when encounters a syntax error.
    try {
      syntaxTrees.push(lazy.Reflect.parse(source));
    } catch (e) {
      if (logExceptions) {
        DevToolsUtils.reportException("Parser:get", e);
      }
    }
  } else {
    // Generate the AST nodes for each script.
    for (const script of scriptMatches) {
      // Reflect.parse throws when encounters a syntax error.
      try {
        syntaxTrees.push(lazy.Reflect.parse(script));
      } catch (e) {
        if (logExceptions) {
          DevToolsUtils.reportException("Parser:get", e);
        }
      }
    }
  }

  return syntaxTrees;
}

exports.getSyntaxTrees = getSyntaxTrees;
