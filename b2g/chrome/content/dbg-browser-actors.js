/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';
/**
 * B2G-specific actors.
 */

/**
 * Construct a root actor appropriate for use in a server running in B2G. The
 * returned root actor:
 * - respects the factories registered with DebuggerServer.addGlobalActor,
 * - uses a ContentTabList to supply tab actors,
 * - sends all navigator:browser window documents a Debugger:Shutdown event
 *   when it exits.
 *
 * * @param connection DebuggerServerConnection
 *        The conection to the client.
 */
function createRootActor(connection)
{
  let parameters = {
#ifndef MOZ_WIDGET_GONK
    tabList: new ContentTabList(connection),
#else
    tabList: [],
#endif
    globalActorFactories: DebuggerServer.globalActorFactories,
    onShutdown: sendShutdownEvent
  };
  let root = new RootActor(connection, parameters);
  root.applicationType = "operating-system";
  return root;
}

/**
 * A live list of BrowserTabActors representing the current browser tabs,
 * to be provided to the root actor to answer 'listTabs' requests. In B2G,
 * only a single tab is ever present.
 *
 * @param connection DebuggerServerConnection
 *     The connection in which this list's tab actors may participate.
 *
 * @see BrowserTabList for more a extensive description of how tab list objects
 *      work.
 */
function ContentTabList(connection)
{
  BrowserTabList.call(this, connection);
}

ContentTabList.prototype = Object.create(BrowserTabList.prototype);

ContentTabList.prototype.constructor = ContentTabList;

ContentTabList.prototype.iterator = function() {
  let browser = Services.wm.getMostRecentWindow('navigator:browser');
  // Do we have an existing actor for this browser? If not, create one.
  let actor = this._actorByBrowser.get(browser);
  if (!actor) {
    actor = new ContentTabActor(this._connection, browser);
    this._actorByBrowser.set(browser, actor);
    actor.selected = true;
  }

  yield actor;
};

ContentTabList.prototype.onCloseWindow = makeInfallible(function(aWindow) {
  /*
   * nsIWindowMediator deadlocks if you call its GetEnumerator method from
   * a nsIWindowMediatorListener's onCloseWindow hook (bug 873589), so
   * handle the close in a different tick.
   */
  Services.tm.currentThread.dispatch(makeInfallible(() => {
    /*
     * Scan the entire map for actors representing tabs that were in this
     * top-level window, and exit them.
     */
    for (let [browser, actor] of this._actorByBrowser) {
      this._handleActorClose(actor, browser);
    }
  }, "ContentTabList.prototype.onCloseWindow's delayed body"), 0);
}, "ContentTabList.prototype.onCloseWindow");

/**
 * Creates a tab actor for handling requests to the single tab, like
 * attaching and detaching. ContentTabActor respects the actor factories
 * registered with DebuggerServer.addTabActor.
 *
 * @param connection DebuggerServerConnection
 *        The conection to the client.
 * @param browser browser
 *        The browser instance that contains this tab.
 */
function ContentTabActor(connection, browser)
{
  BrowserTabActor.call(this, connection, browser);
}

ContentTabActor.prototype.constructor = ContentTabActor;

ContentTabActor.prototype = Object.create(BrowserTabActor.prototype);

Object.defineProperty(ContentTabActor.prototype, "title", {
  get: function() {
    return this.browser.title;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(ContentTabActor.prototype, "url", {
  get: function() {
    return this.browser.document.documentURI;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(ContentTabActor.prototype, "window", {
  get: function() {
    return this.browser;
  },
  enumerable: true,
  configurable: false
});
