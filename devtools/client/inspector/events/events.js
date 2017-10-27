/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const EventsApp = createFactory(require("./components/EventsApp"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

class EventsView {

  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.store = inspector.store;

    this.init();
  }

  init() {
    if (!this.inspector) {
      return;
    }

    let eventsApp = EventsApp({});

    let provider = createElement(Provider, {
      id: "eventsview",
      key: "eventsview",
      store: this.store,
      title: INSPECTOR_L10N.getStr("inspector.sidebar.eventsViewTitle")
    }, eventsApp);

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = provider;
  }

  destroy() {
    this.document = null;
    this.inspector = null;
    this.store = null;
  }

}

module.exports = EventsView;
