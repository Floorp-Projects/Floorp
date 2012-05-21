/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SubscribeHandler = {
  /**
   * The nsIFeedWriter object that produces the UI
   */
  _feedWriter: null,
  
  init: function SH_init() {
    this._feedWriter = new BrowserFeedWriter();
    this._feedWriter.init(window);
  },

  writeContent: function SH_writeContent() {
    this._feedWriter.writeContent();
  },

  uninit: function SH_uninit() {
    this._feedWriter.close();
  },
  
  subscribe: function SH_subscribe() {
    this._feedWriter.subscribe();
  }
};
