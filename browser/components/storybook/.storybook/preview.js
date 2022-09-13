/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DOMLocalization } from "@fluent/dom";
import { FluentBundle, FluentResource } from "@fluent/bundle";

// Base Fluent set up.
let storybookBundle = new FluentBundle("en-US");
let loadedResources = new Set();
function* generateBundles() {
  yield* [storybookBundle];
}
document.l10n = new DOMLocalization([], generateBundles);
document.l10n.connectRoot(document.documentElement);

// Any fluent imports should go through MozXULElement.insertFTLIfNeeded.
window.MozXULElement = {
  async insertFTLIfNeeded(name) {
    if (loadedResources.has(name)) {
      return;
    }

    // This should be browser or toolkit.
    let root = name.split("/")[0];
    let ftlContents;
    //
    // TODO(mstriemer): These seem like they could be combined but I don't want
    // to fight with webpack anymore.
    if (root == "toolkit") {
      // eslint-disable-next-line no-unsanitized/method
      let imported = await import(
        /* webpackInclude: /.*[\/\\].*\.ftl/ */
        `toolkit/locales/en-US/${name}`
      );
      ftlContents = imported.default;
    } else if (root == "browser") {
      // eslint-disable-next-line no-unsanitized/method
      let imported = await import(
        /* webpackInclude: /.*[\/\\].*\.ftl/ */
        `browser/locales/en-US/${name}`
      );
      ftlContents = imported.default;
    }

    if (loadedResources.has(name)) {
      // Seems possible we've attempted to load this twice before the first call
      // resolves, so once the first load is complete we can abandon the others.
      return;
    }

    let ftlResource = new FluentResource(ftlContents);
    storybookBundle.addResource(ftlResource);
    loadedResources.add(name);
    document.l10n.translateRoots();
  },
};
