/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
Cu.import("resource://gre/modules/EventEmitter.jsm");

const SectionsManager = {
  ACTIONS_TO_PROXY: ["SYSTEM_TICK", "NEW_TAB_LOAD"],
  initialized: false,
  sections: new Set(),
  addSection(id, options) {
    this.sections.add(id);
    this.emit(this.ADD_SECTION, id, options);
  },
  removeSection(id) {
    this.emit(this.REMOVE_SECTION, id);
    this.sections.delete(id);
  },
  updateRows(id, rows, shouldBroadcast) {
    if (this.sections.has(id)) {
      this.emit(this.UPDATE_ROWS, id, rows, shouldBroadcast);
    }
  }
};

for (const action of [
  "ACTION_DISPATCHED",
  "ADD_SECTION",
  "REMOVE_SECTION",
  "UPDATE_ROWS",
  "INIT",
  "UNINIT"
]) {
  SectionsManager[action] = action;
}

EventEmitter.decorate(SectionsManager);

class SectionsFeed {
  constructor() {
    this.onAddSection = this.onAddSection.bind(this);
    this.onRemoveSection = this.onRemoveSection.bind(this);
    this.onUpdateRows = this.onUpdateRows.bind(this);
  }

  init() {
    SectionsManager.on(SectionsManager.ADD_SECTION, this.onAddSection);
    SectionsManager.on(SectionsManager.REMOVE_SECTION, this.onRemoveSection);
    SectionsManager.on(SectionsManager.UPDATE_ROWS, this.onUpdateRows);
    SectionsManager.initialized = true;
    SectionsManager.emit(SectionsManager.INIT);
  }

  uninit() {
    SectionsManager.initialized = false;
    SectionsManager.emit(SectionsManager.UNINIT);
    SectionsManager.off(SectionsManager.ADD_SECTION, this.onAddSection);
    SectionsManager.off(SectionsManager.REMOVE_SECTION, this.onRemoveSection);
    SectionsManager.off(SectionsManager.UPDATE_ROWS, this.onUpdateRows);
  }

  onAddSection(event, id, options) {
    if (options) {
      this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_REGISTER, data: Object.assign({id}, options)}));
    }
  }

  onRemoveSection(event, id) {
    this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_DEREGISTER, data: id}));
  }

  onUpdateRows(event, id, rows, shouldBroadcast = false) {
    if (rows) {
      const action = {type: at.SECTION_ROWS_UPDATE, data: {id, rows}};
      this.store.dispatch(shouldBroadcast ? ac.BroadcastToContent(action) : action);
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
    }
    if (SectionsManager.ACTIONS_TO_PROXY.includes(action.type) && SectionsManager.sections.size > 0) {
      SectionsManager.emit(SectionsManager.ACTION_DISPATCHED, action.type, action.data);
    }
  }
}

this.SectionsFeed = SectionsFeed;
this.SectionsManager = SectionsManager;
this.EXPORTED_SYMBOLS = ["SectionsFeed", "SectionsManager"];
