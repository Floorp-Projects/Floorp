/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const path = require("path");

// This plugin supports finding files for resource:/activity-stream/ uris,
// translating the uri into a path relative to the browser/components/newtab/
// directory where the file may be found.

module.exports = {
  ResourceUriPlugin: class ResourceUriPlugin {
    #resourcePathRegEx;

    constructor({ resourcePathRegEx }) {
      this.#resourcePathRegEx = resourcePathRegEx;
    }

    apply(compiler) {
      compiler.hooks.compilation.tap(
        "ResourceUriPlugin",
        (compilation, { normalModuleFactory }) => {
          normalModuleFactory.hooks.resolveForScheme
            .for("resource")
            .tap("ResourceUriPlugin", resourceData => {
              const url = new URL(resourceData.resource);
              if (!url.href.match(this.#resourcePathRegEx)) {
                return true;
              }
              const pathname = path.join(__dirname, "..", url.pathname);
              resourceData.path = pathname;
              resourceData.query = url.search;
              resourceData.fragment = url.hash;
              resourceData.resource = pathname + url.search + url.hash;
              return true;
            });
        }
      );
    }
  },
};
