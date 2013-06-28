/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
/**
 * We want to avoid mysterious behavior if tabs are closed or opened mid-iteration.
 * We want at the end, a list of the actors that were live when we began the iteration.
 * So, we update the map first, iterate over it again to yield the actors.
 */
MetroTabList.prototype.iterator = function() {

  let initialMapSize = this._actorByBrowser.size;
  let foundCount = 0;
  for (let win of allAppShellDOMWindows("navigator:browser")) {
    let selectedTab = win.Browser.selectedBrowser;

    for (let browser of win.Browser.browsers) {
      let actor = this._actorByBrowser.get(browser);
      if (actor) {
        foundCount++;
      } else {
        actor = new BrowserTabActor(this._connection, browser);
        this._actorByBrowser.set(browser, actor);
      }

      // Set the 'selected' properties on all actors correctly.
      actor.selected = (browser === selectedTab);
    }
  }

  if (this._testing && initialMapSize !== foundCount) {
    throw error("_actorByBrowser map contained actors for dead tabs");
  }

  this._mustNotify = true;
  this._checkListening();

  for (let [browser, actor] of this._actorByBrowser) {
    yield actor;
  }
};
