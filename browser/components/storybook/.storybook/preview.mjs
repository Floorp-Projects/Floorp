/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { css, html } from "lit.all.mjs";
import { MozLitElement } from "toolkit/content/widgets/lit-utils.mjs";
import { setCustomElementsManifest } from "@storybook/web-components";
import customElementsManifest from "../custom-elements.json";
import { insertFTLIfNeeded, connectFluent } from "./fluent-utils.mjs";

// Base Fluent set up.
connectFluent();

// Any fluent imports should go through MozXULElement.insertFTLIfNeeded.
window.MozXULElement = {
  insertFTLIfNeeded,
};

// Used to set prefs in unprivileged contexts.
window.RPMSetPref = () => {
  /* NOOP */
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
      font-size: var(--font-size-root);
      appearance: none;
      background-color: var(--color-canvas);
      color: var(--text-color);
      -moz-box-layout: flex;
    }

    :host {
      font-size: var(--font-size-root);
    }
  `;

  static properties = {
    story: { type: Function },
    context: { type: Object },
  };

  connectedCallback() {
    super.connectedCallback();
    this.classList.add("anonymous-content-host");
  }

  storyContent() {
    if (this.story) {
      return this.story();
    }
    return html` <slot></slot> `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      ${this.storyContent()}
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

// Enable props tables documentation.
setCustomElementsManifest(customElementsManifest);
