/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 /* globals ContentSearch, XPCOMUtils, Services */
"use strict";

const {utils: Cu} = Components;
const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ContentSearch",
  "resource:///modules/ContentSearch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

this.SearchFeed = class SearchFeed {
  addObservers() {
    Services.obs.addObserver(this, SEARCH_ENGINE_TOPIC);
  }
  removeObservers() {
    Services.obs.removeObserver(this, SEARCH_ENGINE_TOPIC);
  }
  observe(subject, topic, data) {
    switch (topic) {
      case SEARCH_ENGINE_TOPIC:
        if (data !== "engine-default") {
          this.getState();
        }
        break;
    }
  }
  async getState() {
    const state = await ContentSearch.currentStateObj(true);
    const engines = state.engines.map(engine => ({
      name: engine.name,
      icon: engine.iconBuffer
    }));
    const currentEngine = {
      name: state.currentEngine.name,
      icon: state.currentEngine.iconBuffer
    };
    const action = {type: at.SEARCH_STATE_UPDATED, data: {engines, currentEngine}};
    this.store.dispatch(ac.BroadcastToContent(action));
  }
  performSearch(browser, data) {
    ContentSearch.performSearch({target: browser}, data);
  }
  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.addObservers();
        this.getState();
        break;
      case at.PERFORM_SEARCH:
        this.performSearch(action._target.browser, action.data);
        break;
      case at.UNINIT:
        this.removeObservers();
        break;
    }
  }
};
this.EXPORTED_SYMBOLS = ["SearchFeed"];
