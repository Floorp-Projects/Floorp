/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Console.jsm");
const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

// Url to fetch snippets, in the urlFormatter service format.
const SNIPPETS_URL_PREF = "browser.aboutHomeSnippets.updateUrl";

// Should be bumped up if the snippets content format changes.
const STARTPAGE_VERSION = 4;

this.SnippetsFeed = class SnippetsFeed {
  constructor() {
    this._onUrlChange = this._onUrlChange.bind(this);
  }
  get snippetsURL() {
    const updateURL = Services
      .prefs.getStringPref(SNIPPETS_URL_PREF)
      .replace("%STARTPAGE_VERSION%", STARTPAGE_VERSION);
    return Services.urlFormatter.formatURL(updateURL);
  }
  init() {
    const data = {
      snippetsURL: this.snippetsURL,
      version: STARTPAGE_VERSION
    };
    this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPETS_DATA, data}));
    Services.prefs.addObserver(SNIPPETS_URL_PREF, this._onUrlChange);
  }
  uninit() {
    this.store.dispatch({type: at.SNIPPETS_RESET});
    Services.prefs.removeObserver(SNIPPETS_URL_PREF, this._onUrlChange);
  }
  _onUrlChange() {
    this.store.dispatch(ac.BroadcastToContent({
      type: at.SNIPPETS_DATA,
      data: {snippetsURL: this.snippetsURL}
    }));
  }
  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.FEED_INIT:
        if (action.data === "feeds.snippets") { this.init(); }
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["SnippetsFeed"];
