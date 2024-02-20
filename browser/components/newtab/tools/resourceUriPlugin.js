/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This plugin supports finding files with particular resource:// URIs
// and translating the uri into a relative filesytem path where the file may be
// found when running within the Karma / Mocha test framework.

const path = require("path");

module.exports = {
  ResourceUriPlugin: class ResourceUriPlugin {
    /**
     * @typedef {RegEx} ResourceReplacement0
     *   A regular expression matching a resource:// URI substring to be
     *   replaced.
     * @typedef {string} ResourceReplacement1
     *   A string to replace the matched substring with.
     * @typedef {[ResourceReplacement0, ResourceReplacement1]} ResourceReplacement
     */

    /**
     * Maps regular expressions representing resource URIs to strings that
     * should replace matches for those regular expressions.
     *
     * @type {ResourceReplacement[]}
     */
    #resourcePathRegExes;

    /**
     * @param {object} options
     * @param {ResourceReplacement[]} options.resourcePathRegExes
     *   An array of regex/string tuples to perform replacements on for
     *   imports involving resource:// URIs.
     */
    constructor({ resourcePathRegExes }) {
      this.#resourcePathRegExes = resourcePathRegExes;
    }

    apply(compiler) {
      compiler.hooks.compilation.tap(
        "ResourceUriPlugin",
        (compilation, { normalModuleFactory }) => {
          normalModuleFactory.hooks.resolveForScheme
            .for("resource")
            .tap("ResourceUriPlugin", resourceData => {
              const url = new URL(resourceData.resource);

              for (let [regex, replacement] of this.#resourcePathRegExes) {
                if (!url.href.match(regex)) {
                  continue;
                }
                // path.join() is necessary to normalize the path on Windows.
                // Without it, the path may contain backslashes, resulting in
                // different build output on Windows than on Unix systems.
                const pathname = path.join(
                  url.href.replace(regex, replacement)
                );
                resourceData.path = pathname;
                resourceData.query = url.search;
                resourceData.fragment = url.hash;
                resourceData.resource = pathname + url.search + url.hash;
                return true;
              }

              return true;
            });
        }
      );
    }
  },
};
