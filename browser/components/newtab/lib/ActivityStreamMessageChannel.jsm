/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// TODO delete this?

ChromeUtils.defineModuleGetter(
  this,
  "AboutNewTab",
  "resource:///modules/AboutNewTab.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AboutHomeStartupCache",
  "resource:///modules/BrowserGlue.jsm"
);

const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);

const {
  actionCreators: ac,
  actionTypes: at,
  actionUtils: au,
} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm");

const ABOUT_NEW_TAB_URL = "about:newtab";
const ABOUT_HOME_URL = "about:home";

const DEFAULT_OPTIONS = {
  dispatch(action) {
    throw new Error(
      `\nMessageChannel: Received action ${action.type}, but no dispatcher was defined.\n`
    );
  },
  pageURL: ABOUT_NEW_TAB_URL,
  outgoingMessageName: "ActivityStream:MainToContent",
  incomingMessageName: "ActivityStream:ContentToMain",
};

this.ActivityStreamMessageChannel = class ActivityStreamMessageChannel {
  /**
   * ActivityStreamMessageChannel - This module connects a Redux store to a RemotePageManager in Firefox.
   *                  Call .createChannel to start the connection, and .destroyChannel to destroy it.
   *                  You should use the BroadcastToContent, AlsoToOneContent, and AlsoToMain action creators
   *                  in common/Actions.jsm to help you create actions that will be automatically routed
   *                  to the correct location.
   *
   * @param  {object} options
   * @param  {function} options.dispatch The dispatch method from a Redux store
   * @param  {string} options.pageURL The URL to which a RemotePageManager should be attached.
   *                                  Note that if it is about:newtab, the existing RemotePageManager
   *                                  for about:newtab will also be disabled
   * @param  {string} options.outgoingMessageName The name of the message sent to child processes
   * @param  {string} options.incomingMessageName The name of the message received from child processes
   * @return {ActivityStreamMessageChannel}
   */
  constructor(options = {}) {
    Object.assign(this, DEFAULT_OPTIONS, options);
    this.channel = null;

    this.middleware = this.middleware.bind(this);
    this.onMessage = this.onMessage.bind(this);
    this.onNewTabLoad = this.onNewTabLoad.bind(this);
    this.onNewTabUnload = this.onNewTabUnload.bind(this);
    this.onNewTabInit = this.onNewTabInit.bind(this);
  }

  /**
   * middleware - Redux middleware that looks for AlsoToOneContent and BroadcastToContent type
   *              actions, and sends them out.
   *
   * @param  {object} store A redux store
   * @return {function} Redux middleware
   */
  middleware(store) {
    return next => action => {
      const skipMain = action.meta && action.meta.skipMain;
      if (!this.channel && !skipMain) {
        next(action);
        return;
      }
      if (au.isSendToOneContent(action)) {
        this.send(action);
      } else if (au.isBroadcastToContent(action)) {
        this.broadcast(action);
      } else if (au.isSendToPreloaded(action)) {
        this.sendToPreloaded(action);
      }

      if (!skipMain) {
        next(action);
      }
    };
  }

  /**
   * onActionFromContent - Handler for actions from a content processes
   *
   * @param  {object} action  A Redux action
   * @param  {string} targetId The portID of the port that sent the message
   */
  onActionFromContent(action, targetId) {
    this.dispatch(ac.AlsoToMain(action, this.validatePortID(targetId)));
  }

  /**
   * broadcast - Sends an action to all ports
   *
   * @param  {object} action A Redux action
   */
  broadcast(action) {
    // We're trying to update all tabs, so signal the AboutHomeStartupCache
    // that its likely time to refresh the cache.
    AboutHomeStartupCache.onPreloadedNewTabMessage();

    this.channel.sendAsyncMessage(this.outgoingMessageName, action);
  }

  /**
   * send - Sends an action to a specific port
   *
   * @param  {obj} action A redux action; it should contain a portID in the meta.toTarget property
   */
  send(action) {
    const targetId = action.meta && action.meta.toTarget;
    const target = this.getTargetById(targetId);
    try {
      target.sendAsyncMessage(this.outgoingMessageName, action);
    } catch (e) {
      // The target page is closed/closing by the user or test, so just ignore.
    }
  }

  /**
   * A valid portID is a combination of process id and port
   * https://searchfox.org/mozilla-central/rev/196560b95f191b48ff7cba7c2ba9237bba6b5b6a/toolkit/components/remotepagemanager/RemotePageManagerChild.jsm#14
   */
  validatePortID(id) {
    if (typeof id !== "string" || !id.includes(":")) {
      Cu.reportError("Invalid portID");
    }

    return id;
  }

  /**
   * getIdByTarget - Retrieve the id of a message target, if it exists in this.targets
   *
   * @param  {obj} targetObj A message target
   * @return {string|null} The unique id of the target, if it exists.
   */
  getTargetById(id) {
    this.validatePortID(id);
    for (let port of this.channel.messagePorts) {
      if (port.portID === id) {
        return port;
      }
    }
    return null;
  }

  /**
   * sendToPreloaded - Sends an action to each preloaded browser, if any
   *
   * @param  {obj} action A redux action
   */
  sendToPreloaded(action) {
    // We're trying to update the preloaded about:newtab, so signal
    // the AboutHomeStartupCache that its likely time to refresh
    // the cache.
    AboutHomeStartupCache.onPreloadedNewTabMessage();

    const preloadedBrowsers = this.getPreloadedBrowser();
    if (preloadedBrowsers && action.data) {
      for (let preloadedBrowser of preloadedBrowsers) {
        try {
          preloadedBrowser.sendAsyncMessage(this.outgoingMessageName, action);
        } catch (e) {
          // The preloaded page is no longer available, so just ignore.
        }
      }
    }
  }

  /**
   * getPreloadedBrowser - Retrieve the port of any preloaded browsers
   *
   * @return {Array|null} An array of ports belonging to the preloaded browsers, or null
   *                      if there aren't any preloaded browsers
   */
  getPreloadedBrowser() {
    let preloadedPorts = [];
    for (let port of this.channel.messagePorts) {
      if (this.isPreloadedBrowser(port.browser)) {
        preloadedPorts.push(port);
      }
    }
    return preloadedPorts.length ? preloadedPorts : null;
  }

  /**
   * isPreloadedBrowser - Returns true if the passed browser has been preloaded
   *                      for faster rendering of new tabs.
   *
   * @param {<browser>} A <browser> to check.
   * @return {bool} True if the browser is preloaded.
   *                      if there aren't any preloaded browsers
   */
  isPreloadedBrowser(browser) {
    return browser.getAttribute("preloadedState") === "preloaded";
  }

  /**
   * createChannel - Create RemotePages channel to establishing message passing
   *                 between the main process and child pages
   */
  createChannel() {
    //  Receive AboutNewTab's Remote Pages instance, if it exists, on override
    const channel =
      this.pageURL === ABOUT_NEW_TAB_URL &&
      AboutNewTab.overridePageListener(true);
    this.channel =
      channel || new RemotePages([ABOUT_HOME_URL, ABOUT_NEW_TAB_URL]);
    this.channel.addMessageListener("RemotePage:Init", this.onNewTabInit);
    this.channel.addMessageListener("RemotePage:Load", this.onNewTabLoad);
    this.channel.addMessageListener("RemotePage:Unload", this.onNewTabUnload);
    this.channel.addMessageListener(this.incomingMessageName, this.onMessage);
  }

  simulateMessagesForExistingTabs() {
    // Some pages might have already loaded, so we won't get the usual message
    for (const target of this.channel.messagePorts) {
      const simulatedMsg = {
        target: Object.assign({ simulated: true }, target),
      };
      this.onNewTabInit(simulatedMsg);
      if (target.loaded) {
        this.onNewTabLoad(simulatedMsg);
      }
    }
  }

  /**
   * destroyChannel - Destroys the RemotePages channel
   */
  destroyChannel() {
    this.channel.removeMessageListener("RemotePage:Init", this.onNewTabInit);
    this.channel.removeMessageListener("RemotePage:Load", this.onNewTabLoad);
    this.channel.removeMessageListener(
      "RemotePage:Unload",
      this.onNewTabUnload
    );
    this.channel.removeMessageListener(
      this.incomingMessageName,
      this.onMessage
    );
    if (this.pageURL === ABOUT_NEW_TAB_URL) {
      AboutNewTab.reset(this.channel);
    } else {
      this.channel.destroy();
    }
    this.channel = null;
  }

  /**
   * onNewTabInit - Handler for special RemotePage:Init message fired
   * by RemotePages
   *
   * @param  {obj} msg The messsage from a page that was just initialized
   */
  onNewTabInit(msg) {
    this.onActionFromContent(
      {
        type: at.NEW_TAB_INIT,
        data: msg.target,
      },
      msg.target.portID
    );
  }

  /**
   * onNewTabLoad - Handler for special RemotePage:Load message fired by RemotePages
   *
   * @param  {obj} msg The messsage from a page that was just loaded
   */
  onNewTabLoad(msg) {
    let { browser } = msg.target;
    if (
      this.isPreloadedBrowser(browser) &&
      browser.ownerGlobal.windowState !== browser.ownerGlobal.STATE_MINIMIZED &&
      !browser.ownerGlobal.isFullyOccluded
    ) {
      // As a perceived performance optimization, if this loaded Activity Stream
      // happens to be a preloaded browser in a window that is not minimized or
      // occluded, have it render its layers to the compositor now to increase
      // the odds that by the time we switch to the tab, the layers are already
      // ready to present to the user.
      browser.renderLayers = true;
    }

    this.onActionFromContent({ type: at.NEW_TAB_LOAD }, msg.target.portID);
  }

  /**
   * onNewTabUnloadLoad - Handler for special RemotePage:Unload message fired by RemotePages
   *
   * @param  {obj} msg The messsage from a page that was just unloaded
   */
  onNewTabUnload(msg) {
    this.onActionFromContent({ type: at.NEW_TAB_UNLOAD }, msg.target.portID);
  }

  /**
   * onMessage - Handles custom messages from content. It expects all messages to
   *             be formatted as Redux actions, and dispatches them to this.store
   *
   * @param  {obj} msg A custom message from content
   * @param  {obj} msg.action A Redux action (e.g. {type: "HELLO_WORLD"})
   * @param  {obj} msg.target A message target
   */
  onMessage(msg) {
    const { portID } = msg.target;
    if (!msg.data || !msg.data.type) {
      Cu.reportError(
        new Error(`Received an improperly formatted message from ${portID}`)
      );
      return;
    }
    let action = {};
    Object.assign(action, msg.data);
    // target is used to access a browser reference that came from the content
    // and should only be used in feeds (not reducers)
    action._target = msg.target;
    this.onActionFromContent(action, portID);
  }
};

this.DEFAULT_OPTIONS = DEFAULT_OPTIONS;
const EXPORTED_SYMBOLS = ["ActivityStreamMessageChannel", "DEFAULT_OPTIONS"];
