/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for a single chrome window, like a browser window.
 *
 * This actor extends BrowsingContextTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { Ci } = require("chrome");
const Services = require("Services");
const {
  BrowsingContextTargetActor,
  browsingContextTargetPrototype
} = require("devtools/server/actors/targets/browsing-context");

const { extend } = require("devtools/shared/extend");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const { chromeWindowTargetSpec } = require("devtools/shared/specs/targets/chrome-window");

/**
 * Protocol.js expects only the prototype object, and does not maintain the
 * prototype chain when it constructs the ActorClass. For this reason we are using
 * `extend` to maintain the properties of BrowsingContextTargetActor.prototype
 */
const chromeWindowTargetPrototype = extend({}, browsingContextTargetPrototype);

/**
 * Creates a ChromeWindowTargetActor for debugging a single window, like a browser window
 * in Firefox, but it can be used to reach any window in the process.
 *
 * Currently this is parent process only because the root actor's `onGetWindow` doesn't
 * try to cross process boundaries.  This actor technically would work for both chrome and
 * content windows, but it can't reach (most) content windows since it's parent process
 * only.  Since these restrictions mean that chrome windows are the main use case for
 * this at the moment, it's named to match.
 *
 * Most of the implementation is inherited from BrowsingContextTargetActor.
 * ChromeWindowTargetActor exposes all target-scoped actors via its form() request, like
 * BrowsingContextTargetActor.
 *
 * You can request a specific window's actor via RootActor.getWindow().
 *
 * @param connection DebuggerServerConnection
 *        The connection to the client.
 * @param window DOMWindow
 *        The window.
 */
chromeWindowTargetPrototype.initialize = function(connection, window) {
  BrowsingContextTargetActor.prototype.initialize.call(this, connection);

  const docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);
  Object.defineProperty(this, "docShell", {
    value: docShell,
    configurable: true
  });
};

// Bug 1266561: This setting is mysteriously named, we should split up the
// functionality that is triggered by it.
chromeWindowTargetPrototype.isRootActor = true;

chromeWindowTargetPrototype.observe = function(subject, topic, data) {
  BrowsingContextTargetActor.prototype.observe.call(this, subject, topic, data);
  if (!this.attached) {
    return;
  }
  if (topic == "chrome-webnavigation-destroy") {
    this._onDocShellDestroy(subject);
  }
};

chromeWindowTargetPrototype._attach = function() {
  if (this.attached) {
    return false;
  }

  BrowsingContextTargetActor.prototype._attach.call(this);

  // Listen for chrome docshells in addition to content docshells
  if (this.docShell.itemType == Ci.nsIDocShellTreeItem.typeChrome) {
    Services.obs.addObserver(this, "chrome-webnavigation-destroy");
  }

  return true;
};

chromeWindowTargetPrototype._detach = function() {
  if (!this.attached) {
    return false;
  }

  if (this.docShell.itemType == Ci.nsIDocShellTreeItem.typeChrome) {
    Services.obs.removeObserver(this, "chrome-webnavigation-destroy");
  }

  BrowsingContextTargetActor.prototype._detach.call(this);

  return true;
};

exports.ChromeWindowTargetActor =
  ActorClassWithSpec(chromeWindowTargetSpec, chromeWindowTargetPrototype);
