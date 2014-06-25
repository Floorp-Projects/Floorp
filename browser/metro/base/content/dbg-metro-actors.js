/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const error = Components.utils.reportError;

/* Metrofx specific actors, modelled from the android fennec actors */

/** aConnection DebuggerServerConnection, the conection to the client.
 */
function createRootActor(aConnection) {
  let parameters = {
    tabList: new MetroTabList(aConnection),
    globalActorFactories: DebuggerServer.globalActorFactories,
    onShutdown: sendShutdownEvent
  };
  return new RootActor(aConnection, parameters);
}

/** aConnection DebuggerServerConnection, the conection to the client.
 */
function MetroTabList(aConnection) {
  BrowserTabList.call(this, aConnection);
}

MetroTabList.prototype = Object.create(BrowserTabList.prototype);
MetroTabList.prototype.constructor = MetroTabList;

MetroTabList.prototype._getSelectedBrowser = function(aWindow) {
  return aWindow.Browser.selectedBrowser;
};

MetroTabList.prototype._getChildren = function(aWindow) {
  return aWindow.Browser.browsers;
};
