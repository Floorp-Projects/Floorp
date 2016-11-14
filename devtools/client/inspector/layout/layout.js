/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const App = createFactory(require("./components/app"));
const Store = require("./store");

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
      new LocalizationHelper("devtools/client/locales/inspector.properties");

function LayoutView(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;
  this.store = null;

  this.init();
}

LayoutView.prototype = {

  init() {
    let store = this.store = Store();
    let provider = createElement(Provider, {
      store,
      id: "layoutview",
      title: INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle"),
      key: "layoutview",
    }, App());

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    this.inspector.addSidebarTab(
      "layoutview",
      INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle"),
      provider,
      defaultTab == "layoutview"
    );
  },

  destroy() {
    this.inspector = null;
    this.document = null;
    this.store = null;
  },
};

exports.LayoutView = LayoutView;

