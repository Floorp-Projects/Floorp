/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global setConsoleEventHandler, retrieveConsoleEvents */

"use strict";

// This file is loaded on the server side for worker debugging.
// Since the server is running in the worker thread, it doesn't
// have access to Services / Components but the listeners defined here
// are imported by webconsole-utils and used for the webconsole actor.
class ConsoleAPIListener {
  constructor(window, listener, consoleID) {
    this.window = window;
    this.listener = listener;
    this.consoleID = consoleID;
    this.observe = this.observe.bind(this);
  }

  init() {
    setConsoleEventHandler(this.observe);
  }
  destroy() {
    setConsoleEventHandler(null);
  }
  observe(message) {
    this.listener(message.wrappedJSObject);
  }
  getCachedMessages() {
    return retrieveConsoleEvents();
  }
}

exports.ConsoleAPIListener = ConsoleAPIListener;
