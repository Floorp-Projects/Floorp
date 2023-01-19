/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DOMLocalization } from "@fluent/dom";
import { FluentBundle, FluentResource } from "@fluent/bundle";
import { css, html } from "lit.all.mjs";
import { MozLitElement } from "toolkit/content/widgets/lit-utils.mjs";

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
    loadedResources.add(name);
    document.l10n.translateRoots();
  },

  // For some reason Storybook doesn't watch the static folder. By creating a
  // method with a dynamic import we can pull the desired files into the bundle.
  async importCss(name) {
    // eslint-disable-next-line no-unsanitized/method
    let file = await import(
      /* webpackInclude: /.*[\/\\].*\.css$/ */
      `browser/themes/shared/${name}`
    );
    return file;
  },
};

/**
 * Wrapper component used to decorate all of our stories by providing access to
 * `in-content/common.css` without leaking styles that conflict Storybook's CSS.
 *
 * More information on decorators can be found at:
 * https://storybook.js.org/docs/web-components/writing-stories/decorators
 *
 * @property {Function} story
 *  Storybook uses this internally to render stories. We call `story` in our
 *  render function so that the story contents have the same shadow root as
 *  `with-common-styles` and styles from `in-content/common` get applied.
 * @property {Object} context
 *  Another Storybook provided property containing additional data stories use
 *  to render. If we don't make this a reactive property Lit seems to optimize
 *  away any re-rendering of components inside `with-common-styles`.
 */
class WithCommonStyles extends MozLitElement {
  static styles = css`
    :host {
      display: block;
      height: 100%;
      padding: 1rem;
      box-sizing: border-box;
    }

    :host,
    :root {
      font: message-box;
      appearance: none;
      background-color: var(--in-content-page-background);
      color: var(--in-content-page-color);
      -moz-box-layout: flex;
    }

    :host,
    :root:not(.system-font-size) {
      font-size: 15px;
    }
  `;

  static properties = {
    story: { type: Function },
    context: { type: Object },
  };

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      ${this.story()}
    `;
  }
}
customElements.define("with-common-styles", WithCommonStyles);

// Wrap all stories in `with-common-styles`.
export const decorators = [
  (story, context) =>
    html`
      <with-common-styles
        .story=${story}
        .context=${context}
      ></with-common-styles>
    `,
];
