/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const { TabActor } = require("./tab");

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { tabSpec } = require("devtools/shared/specs/tab");

/**
 * Creates a WindowActor for debugging a single window, like a browser window in Firefox,
 * but it can be used to reach any window in the process.  (Currently this is parent
 * process only because the root actor's `onGetWindow` doesn't try to cross process
 * boundaries.)  Both chrome and content windows are supported.
 *
 * Most of the implementation is inherited from TabActor.  WindowActor exposes all tab
 * actors via its form() request, like TabActor.
 *
 * You can request a specific window's actor via RootActor.getWindow().
 *
 * Caveat: Protocol.js expects only the prototype object, and does
 * not maintain the prototype chain when it constructs the
 * ActorClass. For this reason we are using `extend` to
 * maintain the properties of TabActor.prototype
 *
 * @param connection DebuggerServerConnection
 *        The connection to the client.
 * @param window DOMWindow
 *        The window.
 */

const windowPrototype = extend({}, TabActor.prototype);

windowPrototype.initialize = function(connection, window) {
  Actor.prototype.initialize.call(this, connection);
  TabActor.call(this, connection);

  let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);
  Object.defineProperty(this, "docShell", {
    value: docShell,
    configurable: true
  });
};

// Bug 1266561: This setting is mysteriously named, we should split up the
// functionality that is triggered by it.
windowPrototype.isRootActor = true;

windowPrototype.observe = function(subject, topic, data) {
  TabActor.prototype.observe.call(this, subject, topic, data);
  if (!this.attached) {
    return;
  }
  if (topic == "chrome-webnavigation-destroy") {
    this._onDocShellDestroy(subject);
  }
};

windowPrototype._attach = function() {
  if (this.attached) {
    return false;
  }

  TabActor.prototype._attach.call(this);

  // Listen for chrome docshells in addition to content docshells
  if (this.docShell.itemType == Ci.nsIDocShellTreeItem.typeChrome) {
    Services.obs.addObserver(this, "chrome-webnavigation-destroy");
  }

  return true;
};

windowPrototype._detach = function() {
  if (!this.attached) {
    return false;
  }

  if (this.docShell.itemType == Ci.nsIDocShellTreeItem.typeChrome) {
    Services.obs.removeObserver(this, "chrome-webnavigation-destroy");
  }

  TabActor.prototype._detach.call(this);

  return true;
};

exports.WindowActor = ActorClassWithSpec(tabSpec, windowPrototype);
