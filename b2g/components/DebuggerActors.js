/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils.js");
const promise = require("promise");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const { BrowserTabList } = require("devtools/server/actors/webbrowser");

XPCOMUtils.defineLazyGetter(this, "Frames", function() {
  const { Frames } =
    Cu.import("resource://gre/modules/Frames.jsm", {});
  return Frames;
});

/**
 * Unlike the original BrowserTabList which iterates over XUL windows, we
 * override many portions to refer to Frames for the info needed here.
 */
function B2GTabList(connection) {
  BrowserTabList.call(this, connection);
  this._listening = false;
}

B2GTabList.prototype = Object.create(BrowserTabList.prototype);

B2GTabList.prototype._getBrowsers = function() {
  return Frames.list().filter(frame => {
    // Ignore app frames
    return !frame.getAttribute("mozapp");
  });
};

B2GTabList.prototype._getSelectedBrowser = function() {
  return this._getBrowsers().find(frame => {
    // Find the one visible browser (if any)
    return !frame.classList.contains("hidden");
  });
};

B2GTabList.prototype._checkListening = function() {
  // The conditions from BrowserTabList are merged here, since we must listen to
  // all events with our observer design.
  this._listenForEventsIf(this._onListChanged && this._mustNotify ||
                          this._actorByBrowser.size > 0);
};

B2GTabList.prototype._listenForEventsIf = function(shouldListen) {
  if (this._listening != shouldListen) {
    let op = shouldListen ? "addObserver" : "removeObserver";
    Frames[op](this);
    this._listening = shouldListen;
  }
};

B2GTabList.prototype.onFrameCreated = function(frame) {
  let mozapp = frame.getAttribute("mozapp");
  if (mozapp) {
    // Ignore app frames
    return;
  }
  this._notifyListChanged();
  this._checkListening();
};

B2GTabList.prototype.onFrameDestroyed = function(frame) {
  let mozapp = frame.getAttribute("mozapp");
  if (mozapp) {
    // Ignore app frames
    return;
  }
  let actor = this._actorByBrowser.get(frame);
  if (actor) {
    this._handleActorClose(actor, frame);
  }
};

exports.B2GTabList = B2GTabList;
