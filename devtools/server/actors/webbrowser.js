/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci } = require("chrome");
var Services = require("Services");
var { DebuggerServer } = require("devtools/server/main");
var { ActorRegistry } = require("devtools/server/actors/utils/actor-registry");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(this, "RootActor", "devtools/server/actors/root", true);
loader.lazyRequireGetter(this, "FrameTargetActorProxy", "devtools/server/actors/targets/frame-proxy", true);
loader.lazyRequireGetter(this, "WebExtensionActor", "devtools/server/actors/addon/webextension", true);
loader.lazyRequireGetter(this, "WorkerTargetActorList", "devtools/server/actors/worker/worker-target-actor-list", true);
loader.lazyRequireGetter(this, "ServiceWorkerRegistrationActorList",
  "devtools/server/actors/worker/service-worker-registration-list", true);
loader.lazyRequireGetter(this, "ProcessActorList", "devtools/server/actors/process", true);
loader.lazyImporter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
loader.lazyImporter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm");

/**
 * Browser-specific actors.
 */

/**
 * Retrieve the window type of the top-level window |window|.
 */
function appShellDOMWindowType(window) {
  /* This is what nsIWindowMediator's enumerator checks. */
  return window.document.documentElement.getAttribute("windowtype");
}

/**
 * Send Debugger:Shutdown events to all "navigator:browser" windows.
 */
function sendShutdownEvent() {
  for (const win of Services.wm.getEnumerator(DebuggerServer.chromeWindowType)) {
    const evt = win.document.createEvent("Event");
    evt.initEvent("Debugger:Shutdown", true, false);
    win.document.documentElement.dispatchEvent(evt);
  }
}

exports.sendShutdownEvent = sendShutdownEvent;

/**
 * Construct a root actor appropriate for use in a server running in a
 * browser. The returned root actor:
 * - respects the factories registered with ActorRegistry.addGlobalActor,
 * - uses a BrowserTabList to supply target actors for tabs,
 * - sends all navigator:browser window documents a Debugger:Shutdown event
 *   when it exits.
 *
 * * @param connection DebuggerServerConnection
 *          The conection to the client.
 */
exports.createRootActor = function createRootActor(connection) {
  return new RootActor(connection, {
    tabList: new BrowserTabList(connection),
    addonList: new BrowserAddonList(connection),
    workerList: new WorkerTargetActorList(connection, {}),
    serviceWorkerRegistrationList:
      new ServiceWorkerRegistrationActorList(connection),
    processList: new ProcessActorList(),
    globalActorFactories: ActorRegistry.globalActorFactories,
    onShutdown: sendShutdownEvent,
  });
};

/**
 * A live list of FrameTargetActorProxys representing the current browser tabs,
 * to be provided to the root actor to answer 'listTabs' requests.
 *
 * This object also takes care of listening for TabClose events and
 * onCloseWindow notifications, and exiting the target actors concerned.
 *
 * (See the documentation for RootActor for the definition of the "live
 * list" interface.)
 *
 * @param connection DebuggerServerConnection
 *     The connection in which this list's target actors may participate.
 *
 * Some notes:
 *
 * This constructor is specific to the desktop browser environment; it
 * maintains the tab list by tracking XUL windows and their XUL documents'
 * "tabbrowser", "tab", and "browser" elements. What's entailed in maintaining
 * an accurate list of open tabs in this context?
 *
 * - Opening and closing XUL windows:
 *
 * An nsIWindowMediatorListener is notified when new XUL windows (i.e., desktop
 * windows) are opened and closed. It is not notified of individual content
 * browser tabs coming and going within such a XUL window. That seems
 * reasonable enough; it's concerned with XUL windows, not tab elements in the
 * window's XUL document.
 *
 * However, even if we attach TabOpen and TabClose event listeners to each XUL
 * window as soon as it is created:
 *
 * - we do not receive a TabOpen event for the initial empty tab of a new XUL
 *   window; and
 *
 * - we do not receive TabClose events for the tabs of a XUL window that has
 *   been closed.
 *
 * This means that TabOpen and TabClose events alone are not sufficient to
 * maintain an accurate list of live tabs and mark target actors as closed
 * promptly. Our nsIWindowMediatorListener onCloseWindow handler must find and
 * exit all actors for tabs that were in the closing window.
 *
 * Since this is a bit hairy, we don't make each individual attached target
 * actor responsible for noticing when it has been closed; we watch for that,
 * and promise to call each actor's 'exit' method when it's closed, regardless
 * of how we learn the news.
 *
 * - nsIWindowMediator locks
 *
 * nsIWindowMediator holds a lock protecting its list of top-level windows
 * while it calls nsIWindowMediatorListener methods. nsIWindowMediator's
 * GetEnumerator method also tries to acquire that lock. Thus, enumerating
 * windows from within a listener method deadlocks (bug 873589). Rah. One
 * can sometimes work around this by leaving the enumeration for a later
 * tick.
 *
 * - Dragging tabs between windows:
 *
 * When a tab is dragged from one desktop window to another, we receive a
 * TabOpen event for the new tab, and a TabClose event for the old tab; tab XUL
 * elements do not really move from one document to the other (although their
 * linked browser's content window objects do).
 *
 * However, while we could thus assume that each tab stays with the XUL window
 * it belonged to when it was created, I'm not sure this is behavior one should
 * rely upon. When a XUL window is closed, we take the less efficient, more
 * conservative approach of simply searching the entire table for actors that
 * belong to the closing XUL window, rather than trying to somehow track which
 * XUL window each tab belongs to.
 *
 * - Title changes:
 *
 * For tabs living in the child process, we listen for DOMTitleChange message
 * via the top-level window's message manager.
 * But as these messages aren't sent for tabs loaded in the parent process,
 * we also listen for TabAttrModified event, which is fired only on Firefox
 * desktop.
 * Also, we listen DOMTitleChange event on Android document.
 */
function BrowserTabList(connection) {
  this._connection = connection;

  /*
   * The XUL document of a tabbed browser window has "tab" elements, whose
   * 'linkedBrowser' JavaScript properties are "browser" elements; those
   * browsers' 'contentWindow' properties are wrappers on the tabs' content
   * window objects.
   *
   * This map's keys are "browser" XUL elements; it maps each browser element
   * to the target actor we've created for its content window, if we've created
   * one. This map serves several roles:
   *
   * - During iteration, we use it to find actors we've created previously.
   *
   * - On a TabClose event, we use it to find the tab's target actor and exit it.
   *
   * - When the onCloseWindow handler is called, we iterate over it to find all
   *   tabs belonging to the closing XUL window, and exit them.
   *
   * - When it's empty, and the onListChanged hook is null, we know we can
   *   stop listening for events and notifications.
   *
   * We listen for TabClose events and onCloseWindow notifications in order to
   * send onListChanged notifications, but also to tell actors when their
   * referent has gone away and remove entries for dead browsers from this map.
   * If that code is working properly, neither this map nor the actors in it
   * should ever hold dead tabs alive.
   */
  this._actorByBrowser = new Map();

  /* The current onListChanged handler, or null. */
  this._onListChanged = null;

  /*
   * True if we've been iterated over since we last called our onListChanged
   * hook.
   */
  this._mustNotify = false;

  /* True if we're testing, and should throw if consistency checks fail. */
  this._testing = false;

  this._onAndroidDocumentEvent = this._onAndroidDocumentEvent.bind(this);
}

BrowserTabList.prototype.constructor = BrowserTabList;

BrowserTabList.prototype.destroy = function() {
  this._actorByBrowser.clear();
  this.onListChanged = null;
};

/**
 * Get the selected browser for the given navigator:browser window.
 * @private
 * @param window nsIChromeWindow
 *        The navigator:browser window for which you want the selected browser.
 * @return Element|null
 *         The currently selected xul:browser element, if any. Note that the
 *         browser window might not be loaded yet - the function will return
 *         |null| in such cases.
 */
BrowserTabList.prototype._getSelectedBrowser = function(window) {
  return window.gBrowser ? window.gBrowser.selectedBrowser : null;
};

/**
 * Produces an iterable (in this case a generator) to enumerate all available
 * browser tabs.
 */
BrowserTabList.prototype._getBrowsers = function* () {
  // Iterate over all navigator:browser XUL windows.
  for (const win of Services.wm.getEnumerator(DebuggerServer.chromeWindowType)) {
    // For each tab in this XUL window, ensure that we have an actor for
    // it, reusing existing actors where possible.
    for (const browser of this._getChildren(win)) {
      yield browser;
    }
  }
};

BrowserTabList.prototype._getChildren = function(window) {
  if (!window.gBrowser) {
    return [];
  }
  const { gBrowser } = window;
  if (!gBrowser.browsers) {
    return [];
  }
  return gBrowser.browsers.filter(browser => {
    // Filter tabs that are closing. listTabs calls made right after TabClose
    // events still list tabs in process of being closed.
    const tab = gBrowser.getTabForBrowser(browser);
    return !tab.closing;
  });
};

BrowserTabList.prototype.getList = function(browserActorOptions) {
  const topXULWindow = Services.wm.getMostRecentWindow(
    DebuggerServer.chromeWindowType);
  let selectedBrowser = null;
  if (topXULWindow) {
    selectedBrowser = this._getSelectedBrowser(topXULWindow);
  }

  // As a sanity check, make sure all the actors presently in our map get
  // picked up when we iterate over all windows' tabs.
  const initialMapSize = this._actorByBrowser.size;
  this._foundCount = 0;

  // To avoid mysterious behavior if tabs are closed or opened mid-iteration,
  // we update the map first, and then make a second pass over it to yield
  // the actors. Thus, the sequence yielded is always a snapshot of the
  // actors that were live when we began the iteration.

  const actorPromises = [];

  for (const browser of this._getBrowsers()) {
    const selected = browser === selectedBrowser;
    actorPromises.push(
      this._getActorForBrowser(browser, browserActorOptions)
          .then(actor => {
            // Set the 'selected' properties on all actors correctly.
            actor.selected = selected;
            return actor;
          }, e => {
            if (e.error === "tabDestroyed") {
              // Return null if a tab was destroyed while retrieving the tab list.
              return null;
            }

            // Forward unexpected errors.
            throw e;
          })
    );
  }

  if (this._testing && initialMapSize !== this._foundCount) {
    throw new Error("_actorByBrowser map contained actors for dead tabs");
  }

  this._mustNotify = true;
  this._checkListening();

  return Promise.all(actorPromises).then(values => {
    // Filter out null values if we received a tabDestroyed error.
    return values.filter(value => value != null);
  });
};

/**
 * @param browserActorOptions see options argument of FrameTargetActorProxy constructor.
 */
BrowserTabList.prototype._getActorForBrowser = function(browser, browserActorOptions) {
  // Do we have an existing actor for this browser? If not, create one.
  let actor = this._actorByBrowser.get(browser);
  if (actor) {
    this._foundCount++;
    return actor.update(browserActorOptions);
  }

  actor = new FrameTargetActorProxy(this._connection, browser, browserActorOptions);
  this._actorByBrowser.set(browser, actor);
  this._checkListening();
  return actor.connect();
};

BrowserTabList.prototype.getTab = function({ outerWindowID, tabId },
                                           browserActorOptions) {
  if (typeof outerWindowID == "number") {
    // First look for in-process frames with this ID
    const window = Services.wm.getOuterWindowWithId(outerWindowID);
    // Safety check to prevent debugging top level window via getTab
    if (window && window.isChromeWindow) {
      return Promise.reject({
        error: "forbidden",
        message: "Window with outerWindowID '" + outerWindowID + "' is chrome",
      });
    }
    if (window) {
      const iframe = window.windowUtils.containerElement;
      if (iframe) {
        return this._getActorForBrowser(iframe, browserActorOptions);
      }
    }
    // Then also look on registered <xul:browsers> when using outerWindowID for
    // OOP tabs
    for (const browser of this._getBrowsers()) {
      if (browser.outerWindowID == outerWindowID) {
        return this._getActorForBrowser(browser, browserActorOptions);
      }
    }
    return Promise.reject({
      error: "noTab",
      message: "Unable to find tab with outerWindowID '" + outerWindowID + "'",
    });
  } else if (typeof tabId == "number") {
    // Tabs OOP
    for (const browser of this._getBrowsers()) {
      if (browser.frameLoader &&
          browser.frameLoader.remoteTab &&
          browser.frameLoader.remoteTab.tabId === tabId) {
        return this._getActorForBrowser(browser, browserActorOptions);
      }
    }
    return Promise.reject({
      error: "noTab",
      message: "Unable to find tab with tabId '" + tabId + "'",
    });
  }

  const topXULWindow = Services.wm.getMostRecentWindow(
    DebuggerServer.chromeWindowType);
  if (topXULWindow) {
    const selectedBrowser = this._getSelectedBrowser(topXULWindow);
    return this._getActorForBrowser(selectedBrowser, browserActorOptions);
  }
  return Promise.reject({
    error: "noTab",
    message: "Unable to find any selected browser",
  });
};

Object.defineProperty(BrowserTabList.prototype, "onListChanged", {
  enumerable: true,
  configurable: true,
  get() {
    return this._onListChanged;
  },
  set(v) {
    if (v !== null && typeof v !== "function") {
      throw new Error(
        "onListChanged property may only be set to 'null' or a function");
    }
    this._onListChanged = v;
    this._checkListening();
  },
});

/**
 * The set of tabs has changed somehow. Call our onListChanged handler, if
 * one is set, and if we haven't already called it since the last iteration.
 */
BrowserTabList.prototype._notifyListChanged = function() {
  if (!this._onListChanged) {
    return;
  }
  if (this._mustNotify) {
    this._onListChanged();
    this._mustNotify = false;
  }
};

/**
 * Exit |actor|, belonging to |browser|, and notify the onListChanged
 * handle if needed.
 */
BrowserTabList.prototype._handleActorClose = function(actor, browser) {
  if (this._testing) {
    if (this._actorByBrowser.get(browser) !== actor) {
      throw new Error("FrameTargetActorProxy not stored in map under given browser");
    }
    if (actor.browser !== browser) {
      throw new Error("actor's browser and map key don't match");
    }
  }

  this._actorByBrowser.delete(browser);
  actor.exit();

  this._notifyListChanged();
  this._checkListening();
};

/**
 * Make sure we are listening or not listening for activity elsewhere in
 * the browser, as appropriate. Other than setting up newly created XUL
 * windows, all listener / observer management should happen here.
 */
BrowserTabList.prototype._checkListening = function() {
  /*
   * If we have an onListChanged handler that we haven't sent an announcement
   * to since the last iteration, we need to watch for tab creation as well as
   * change of the currently selected tab and tab title changes of tabs in
   * parent process via TabAttrModified (tabs oop uses DOMTitleChanges).
   *
   * Oddly, we don't need to watch for 'close' events here. If our actor list
   * is empty, then either it was empty the last time we iterated, and no
   * close events are possible, or it was not empty the last time we
   * iterated, but all the actors have since been closed, and we must have
   * sent a notification already when they closed.
   */
  this._listenForEventsIf(this._onListChanged && this._mustNotify,
                          "_listeningForTabOpen",
                          ["TabOpen", "TabSelect", "TabAttrModified"]);

  /* If we have live actors, we need to be ready to mark them dead. */
  this._listenForEventsIf(this._actorByBrowser.size > 0,
                          "_listeningForTabClose",
                          ["TabClose", "TabRemotenessChange"]);

  /*
   * We must listen to the window mediator in either case, since that's the
   * only way to find out about tabs that come and go when top-level windows
   * are opened and closed.
   */
  this._listenToMediatorIf((this._onListChanged && this._mustNotify) ||
                           (this._actorByBrowser.size > 0));

  /*
   * We also listen for title changed from the child process.
   * This allows listening for title changes from OOP tabs.
   * OOP tabs are running browser-child.js frame script which sends DOMTitleChanged
   * events through the message manager.
   */
  this._listenForMessagesIf(this._onListChanged && this._mustNotify,
                            "_listeningForTitleChange",
                            ["DOMTitleChanged"]);

  /*
   * We also listen for title changed event on Android document.
   * Android document events are used for single process Gecko View and Firefox for
   * Android. They do no execute browser-child.js because of single process, instead
   * DOMTitleChanged events are emitted on the top level document.
   * Also, Multi process Gecko View is not covered by here since that receives title
   * updates via DOMTitleChanged messages.
   */
  if (AppConstants.platform === "android") {
    this._listenForEventsIf(this._onListChanged && this._mustNotify,
                            "_listeningForAndroidDocument",
                            ["DOMTitleChanged"],
                            this._onAndroidDocumentEvent);
  }
};

/*
 * Add or remove event listeners for all XUL windows.
 *
 * @param shouldListen boolean
 *    True if we should add event handlers; false if we should remove them.
 * @param guard string
 *    The name of a guard property of 'this', indicating whether we're
 *    already listening for those events.
 * @param eventNames array of strings
 *    An array of event names.
 */
BrowserTabList.prototype._listenForEventsIf =
  function(shouldListen, guard, eventNames, listener = this) {
    if (!shouldListen !== !this[guard]) {
      const op = shouldListen ? "addEventListener" : "removeEventListener";
      for (const win of Services.wm.getEnumerator(DebuggerServer.chromeWindowType)) {
        for (const name of eventNames) {
          win[op](name, listener, false);
        }
      }
      this[guard] = shouldListen;
    }
  };

/*
 * Add or remove message listeners for all XUL windows.
 *
 * @param shouldListen boolean
 *    True if we should add message listeners; false if we should remove them.
 * @param guard string
 *    The name of a guard property of 'this', indicating whether we're
 *    already listening for those messages.
 * @param messageNames array of strings
 *    An array of message names.
 */
BrowserTabList.prototype._listenForMessagesIf =
  function(shouldListen, guard, messageNames) {
    if (!shouldListen !== !this[guard]) {
      const op = shouldListen ? "addMessageListener" : "removeMessageListener";
      for (const win of Services.wm.getEnumerator(DebuggerServer.chromeWindowType)) {
        for (const name of messageNames) {
          win.messageManager[op](name, this);
        }
      }
      this[guard] = shouldListen;
    }
  };

/*
 * This function assumes to be used as a event listener for Android document.
 */
BrowserTabList.prototype._onAndroidDocumentEvent = function(event) {
  switch (event.type) {
    case "DOMTitleChanged": {
      const win = event.currentTarget.ownerGlobal;
      const browser = win.BrowserApp.getBrowserForDocument(event.target);
      this._onDOMTitleChanged(browser);
      break;
    }
  }
};

/**
 * Implement nsIMessageListener.
 */
BrowserTabList.prototype.receiveMessage = DevToolsUtils.makeInfallible(
  function(message) {
    const browser = message.target;
    switch (message.name) {
      case "DOMTitleChanged": {
        this._onDOMTitleChanged(browser);
        break;
      }
    }
  });

/**
 * Handle "DOMTitleChanged" event.
 */
BrowserTabList.prototype._onDOMTitleChanged = DevToolsUtils.makeInfallible(
  function(browser) {
    const actor = this._actorByBrowser.get(browser);
    if (actor) {
      this._notifyListChanged();
      this._checkListening();
    }
  });

/**
 * Implement nsIDOMEventListener.
 */
BrowserTabList.prototype.handleEvent =
DevToolsUtils.makeInfallible(function(event) {
  // If event target has `linkedBrowser`, the event target can be assumed <tab> element.
  // Else (in Android case), because event target is assumed <browser> element,
  // use the target as it is.
  const browser = event.target.linkedBrowser || event.target;
  switch (event.type) {
    case "TabOpen":
    case "TabSelect": {
      /* Don't create a new actor; iterate will take care of that. Just notify. */
      this._notifyListChanged();
      this._checkListening();
      break;
    }
    case "TabClose": {
      const actor = this._actorByBrowser.get(browser);
      if (actor) {
        this._handleActorClose(actor, browser);
      }
      break;
    }
    case "TabRemotenessChange": {
      // We have to remove the cached actor as we have to create a new instance.
      const actor = this._actorByBrowser.get(browser);
      if (actor) {
        this._actorByBrowser.delete(browser);
        // Don't create a new actor; iterate will take care of that. Just notify.
        this._notifyListChanged();
        this._checkListening();
      }
      break;
    }
    case "TabAttrModified": {
      // Remote <browser> title changes are handled via DOMTitleChange message
      // TabAttrModified is only here for browsers in parent process which
      // don't send this message.
      if (browser.isRemoteBrowser) {
        break;
      }
      const actor = this._actorByBrowser.get(browser);
      if (actor) {
        // TabAttrModified is fired in various cases, here only care about title
        // changes
        if (event.detail.changed.includes("label")) {
          this._notifyListChanged();
          this._checkListening();
        }
      }
      break;
    }
  }
}, "BrowserTabList.prototype.handleEvent");

/*
 * If |shouldListen| is true, ensure we've registered a listener with the
 * window mediator. Otherwise, ensure we haven't registered a listener.
 */
BrowserTabList.prototype._listenToMediatorIf = function(shouldListen) {
  if (!shouldListen !== !this._listeningToMediator) {
    const op = shouldListen ? "addListener" : "removeListener";
    Services.wm[op](this);
    this._listeningToMediator = shouldListen;
  }
};

/**
 * nsIWindowMediatorListener implementation.
 *
 * See _onTabClosed for explanation of why we needn't actually tweak any
 * actors or tables here.
 *
 * An nsIWindowMediatorListener's methods get passed all sorts of windows; we
 * only care about the tab containers. Those have 'gBrowser' members.
 */
BrowserTabList.prototype.onOpenWindow =
DevToolsUtils.makeInfallible(function(window) {
  const handleLoad = DevToolsUtils.makeInfallible(() => {
    /* We don't want any further load events from this window. */
    window.removeEventListener("load", handleLoad);

    if (appShellDOMWindowType(window) !== DebuggerServer.chromeWindowType) {
      return;
    }

    // Listen for future tab activity.
    if (this._listeningForTabOpen) {
      window.addEventListener("TabOpen", this);
      window.addEventListener("TabSelect", this);
      window.addEventListener("TabAttrModified", this);
    }
    if (this._listeningForTabClose) {
      window.addEventListener("TabClose", this);
      window.addEventListener("TabRemotenessChange", this);
    }
    if (this._listeningForTitleChange) {
      window.messageManager.addMessageListener("DOMTitleChanged", this);
    }

    // As explained above, we will not receive a TabOpen event for this
    // document's initial tab, so we must notify our client of the new tab
    // this will have.
    this._notifyListChanged();
  });

  /*
   * You can hardly do anything at all with a XUL window at this point; it
   * doesn't even have its document yet. Wait until its document has
   * loaded, and then see what we've got. This also avoids
   * nsIWindowMediator enumeration from within listeners (bug 873589).
   */
  window = window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindow);

  window.addEventListener("load", handleLoad);
}, "BrowserTabList.prototype.onOpenWindow");

BrowserTabList.prototype.onCloseWindow =
DevToolsUtils.makeInfallible(function(window) {
  if (window instanceof Ci.nsIXULWindow) {
    window = window.docShell.domWindow;
  }

  if (appShellDOMWindowType(window) !== DebuggerServer.chromeWindowType) {
    return;
  }

  /*
   * nsIWindowMediator deadlocks if you call its GetEnumerator method from
   * a nsIWindowMediatorListener's onCloseWindow hook (bug 873589), so
   * handle the close in a different tick.
   */
  Services.tm.dispatchToMainThread(DevToolsUtils.makeInfallible(() => {
    /*
     * Scan the entire map for actors representing tabs that were in this
     * top-level window, and exit them.
     */
    for (const [browser, actor] of this._actorByBrowser) {
      /* The browser document of a closed window has no default view. */
      if (!browser.ownerGlobal) {
        this._handleActorClose(actor, browser);
      }
    }
  }, "BrowserTabList.prototype.onCloseWindow's delayed body"));
}, "BrowserTabList.prototype.onCloseWindow");

exports.BrowserTabList = BrowserTabList;

function BrowserAddonList(connection) {
  this._connection = connection;
  this._actorByAddonId = new Map();
  this._onListChanged = null;
}

BrowserAddonList.prototype.getList = async function() {
  const addons = await AddonManager.getAllAddons();
  for (const addon of addons) {
    let actor = this._actorByAddonId.get(addon.id);
    if (!actor) {
      actor = new WebExtensionActor(this._connection, addon);
      this._actorByAddonId.set(addon.id, actor);
    }
  }

  return Array.from(this._actorByAddonId, ([_, actor]) => actor);
};

Object.defineProperty(BrowserAddonList.prototype, "onListChanged", {
  enumerable: true,
  configurable: true,
  get() {
    return this._onListChanged;
  },
  set(v) {
    if (v !== null && typeof v != "function") {
      throw new Error(
        "onListChanged property may only be set to 'null' or a function");
    }
    this._onListChanged = v;
    this._adjustListener();
  },
});

/**
 * AddonManager listener must implement onDisabled.
 */
BrowserAddonList.prototype.onDisabled = function(addon) {
  this._onAddonManagerUpdated();
};

/**
 * AddonManager listener must implement onEnabled.
 */
BrowserAddonList.prototype.onEnabled = function(addon) {
  this._onAddonManagerUpdated();
};

/**
 * AddonManager listener must implement onInstalled.
 */
BrowserAddonList.prototype.onInstalled = function(addon) {
  this._onAddonManagerUpdated();
};

/**
 * AddonManager listener must implement onOperationCancelled.
 */
BrowserAddonList.prototype.onOperationCancelled = function(addon) {
  this._onAddonManagerUpdated();
};

/**
 * AddonManager listener must implement onUninstalling.
 */
BrowserAddonList.prototype.onUninstalling = function(addon) {
  this._onAddonManagerUpdated();
};

/**
 * AddonManager listener must implement onUninstalled.
 */
BrowserAddonList.prototype.onUninstalled = function(addon) {
  this._actorByAddonId.delete(addon.id);
  this._onAddonManagerUpdated();
};

BrowserAddonList.prototype._onAddonManagerUpdated = function(addon) {
  this._notifyListChanged();
  this._adjustListener();
};

BrowserAddonList.prototype._notifyListChanged = function() {
  if (this._onListChanged) {
    this._onListChanged();
  }
};

BrowserAddonList.prototype._adjustListener = function() {
  if (this._onListChanged) {
    // As long as the callback exists, we need to listen for changes
    // so we can notify about add-on changes.
    AddonManager.addAddonListener(this);
  } else if (this._actorByAddonId.size === 0) {
    // When the callback does not exist, we only need to keep listening
    // if the actor cache will need adjusting when add-ons change.
    AddonManager.removeAddonListener(this);
  }
};

exports.BrowserAddonList = BrowserAddonList;
