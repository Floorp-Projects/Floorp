/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

const DUMMY_DATA = [
  "http://cdn.attackofthecute.com/February-13-2013-20-34-03-ii.jpg",
  "https://c1.staticflickr.com/1/202/509388231_d774116746_b.jpg",
  "https://c1.staticflickr.com/1/204/509363724_1f5d8813d0_b.jpg"
];

/**
 * DummySectionFeed - Included to test the Sections API, prefed off by default.
 * Dispatches three image urls as rows data on init.
 */
this.DummySectionFeed = class DummySectionFeed {
  constructor() {
    this.options = {id: "dummy_section", title: "Dummy Section", rows: DUMMY_DATA};
  }
  init() {
    this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_REGISTER, data: this.options}));
  }
  uninit() {
    this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_DEREGISTER, data: this.options.id}));
  }
  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.FEED_INIT:
        if (action.data === "feeds.section.dummy_section") { this.init(); }
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["DummySectionFeed"];
