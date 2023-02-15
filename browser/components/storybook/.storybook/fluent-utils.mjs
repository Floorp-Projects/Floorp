/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DOMLocalization } from "@fluent/dom";
import { FluentBundle, FluentResource } from "@fluent/bundle";
import { addons } from "@storybook/addons";
import { PSEUDO_STRATEGY_TRANSFORMS } from "./l10n-pseudo.mjs";
import {
  UPDATE_STRATEGY_EVENT,
  STRATEGY_DEFAULT,
  PSEUDO_STRATEGIES,
} from "./addon-pseudo-localization/constants.mjs";

let loadedResources = new Map();
let currentStrategy;
let storybookBundle = new FluentBundle("en-US", {
  transform(str) {
    if (currentStrategy in PSEUDO_STRATEGY_TRANSFORMS) {
      return PSEUDO_STRATEGY_TRANSFORMS[currentStrategy](str);
    }
    return str;
  },
});

// Listen for update events from addon-pseudo-localization.
const channel = addons.getChannel();
channel.on(UPDATE_STRATEGY_EVENT, updatePseudoStrategy);

/**
 * Updates "currentStrategy" when the selected pseudo localization strategy
 * changes, which in turn changes the transform used by the Fluent bundle.
 *
 * @param {string} strategy
 *  Pseudo localization strategy. Can be "default", "accented", or "bidi".
 */
function updatePseudoStrategy(strategy = STRATEGY_DEFAULT) {
  if (strategy !== currentStrategy && PSEUDO_STRATEGIES.includes(strategy)) {
    currentStrategy = strategy;
    document.l10n.translateRoots();
  }
}

export function connectFluent() {
  document.l10n = new DOMLocalization([], generateBundles);
  document.l10n.connectRoot(document.documentElement);
  document.l10n.translateRoots();
}

function* generateBundles() {
  yield* [storybookBundle];
}

export async function insertFTLIfNeeded(name) {
  if (loadedResources.has(name)) {
    return;
  }

  // This should be browser, locales-preview or toolkit.
  let [root, ...rest] = name.split("/");
  let ftlContents;

  // TODO(mstriemer): These seem like they could be combined but I don't want
  // to fight with webpack anymore.
  if (root == "toolkit") {
    // eslint-disable-next-line no-unsanitized/method
    let imported = await import(
      /* webpackInclude: /.*[\/\\].*\.ftl$/ */
      `toolkit/locales/en-US/${name}`
    );
    ftlContents = imported.default;
  } else if (root == "browser") {
    // eslint-disable-next-line no-unsanitized/method
    let imported = await import(
      /* webpackInclude: /.*[\/\\].*\.ftl$/ */
      `browser/locales/en-US/${name}`
    );
    ftlContents = imported.default;
  } else if (root == "locales-preview") {
    // eslint-disable-next-line no-unsanitized/method
    let imported = await import(
      /* webpackInclude: /\.ftl$/ */
      `browser/locales-preview/${rest}`
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
  loadedResources.set(name, ftlResource);
  document.l10n.translateRoots();
}
