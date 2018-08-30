/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } =
  ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", {});
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/aboutdebugging-new/",
  window,
});
const Services = require("Services");

const { bindActionCreators } = require("devtools/client/shared/vendor/redux");
const { createFactory } =
  require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } =
  require("devtools/client/shared/vendor/react-dom");
const Provider =
  createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const { L10nRegistry, FileSource } =
  require("resource://gre/modules/L10nRegistry.jsm");

const actions = require("./src/actions/index");
const { configureStore } = require("./src/create-store");
const {
  addNetworkLocationsObserver,
  getNetworkLocations,
} = require("./src/modules/network-locations");

const App = createFactory(require("./src/components/App"));

const { PAGES } = require("./src/constants");

const AboutDebugging = {
  async init() {
    if (!Services.prefs.getBoolPref("devtools.enabled", true)) {
      // If DevTools are disabled, navigate to about:devtools.
      window.location = "about:devtools?reason=AboutDebugging";
      return;
    }

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);

    const messageContexts = await this.createMessageContexts();

    render(
      Provider({ store: this.store }, App({ messageContexts })),
      this.mount
    );

    this.actions.selectPage(PAGES.THIS_FIREFOX);
    addNetworkLocationsObserver(() => {
      this.actions.updateNetworkLocations(getNetworkLocations());
    });
  },

  async createMessageContexts() {
    // XXX Until the strings for the updated about:debugging stabilize, we
    // locate them outside the regular directory for locale resources so that
    // they don't get picked up by localization tools.
    const temporarySource = new FileSource(
      "aboutdebugging",
      ["en-US"],
      "chrome://devtools/content/aboutdebugging-new/tmp-locale/{locale}/"
    );
    L10nRegistry.registerSource(temporarySource);

    const locales = Services.locale.getAppLocalesAsBCP47();
    const generator =
      L10nRegistry.generateContexts(locales, ["aboutdebugging.ftl"]);

    const contexts = [];
    for await (const context of generator) {
      contexts.push(context);
    }

    return contexts;
  },

  destroy() {
    unmountComponentAtNode(this.mount);
  },

  get mount() {
    return document.getElementById("mount");
  },
};

window.addEventListener("DOMContentLoaded", () => {
  AboutDebugging.init();
}, { once: true });

window.addEventListener("unload", () => {
  AboutDebugging.destroy();
}, {once: true});
