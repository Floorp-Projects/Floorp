/* global
   NewTabPrefsProvider,
   Services,
   EventEmitter,
   Preferences,
   XPCOMUtils,
   WebChannel,
   NewTabRemoteResources
*/
/* exported NewTabWebChannel */

"use strict";

this.EXPORTED_SYMBOLS = ["NewTabWebChannel"];

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabPrefsProvider",
                                  "resource:///modules/NewTabPrefsProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabRemoteResources",
                                  "resource:///modules/NewTabRemoteResources.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebChannel",
                                  "resource://gre/modules/WebChannel.jsm");
XPCOMUtils.defineLazyGetter(this, "EventEmitter", function() {
  const {EventEmitter} = Cu.import("resource://devtools/shared/event-emitter.js", {});
  return EventEmitter;
});

const CHAN_ID = "newtab";
const PREF_ENABLED = "browser.newtabpage.remote";
const PREF_MODE = "browser.newtabpage.remote.mode";

/**
 * NewTabWebChannel is the conduit for all communication with unprivileged newtab instances.
 *
 * It allows for the ability to broadcast to all newtab browsers.
 * If the browser.newtab.remote pref is false, the object will be in an uninitialized state.
 *
 * Mode choices:
 * 'production': pages from our production CDN
 * 'staging': pages from our staging CDN
 * 'test': intended for tests
 * 'test2': intended for tests
 * 'dev': intended for development
 *
 *  An unknown mode will result in 'production' mode, which is the default
 *
 *  Incoming messages are expected to be JSON-serialized and in the format:
 *
 *  {
 *    type: "REQUEST_SCREENSHOT",
 *    data: {
 *      url: "https://example.com"
 *    }
 *  }
 *
 *  Or:
 *
 *  {
 *    type: "REQUEST_SCREENSHOT",
 *  }
 *
 *  Outgoing messages are expected to be objects serializable by structured cloning, in a similar format:
 *  {
 *    type: "RECEIVE_SCREENSHOT",
 *    data: {
 *      "url": "https://example.com",
 *      "image": "dataURi:....."
 *    }
 *  }
 */
let NewTabWebChannelImpl = function NewTabWebChannelImpl() {
  EventEmitter.decorate(this);
  this._handlePrefChange = this._handlePrefChange.bind(this);
  this._incomingMessage = this._incomingMessage.bind(this);
};

NewTabWebChannelImpl.prototype = {
  _prefs: {},
  _channel: null,

  // a WeakMap containing browsers as keys and a weak ref to their principal
  // as value
  _principals: null,

  // a Set containing weak refs to browsers
  _browsers: null,

  /*
   * Returns current channel's ID
   */
  get chanId() {
    return CHAN_ID;
  },

  /*
   * Returns the number of browsers currently tracking
   */
  get numBrowsers() {
    return this._getBrowserRefs().length;
  },

  /*
   * Returns current channel's origin
   */
  get origin() {
    if (!(this._prefs.mode in NewTabRemoteResources.MODE_CHANNEL_MAP)) {
      this._prefs.mode = "production";
    }
    return NewTabRemoteResources.MODE_CHANNEL_MAP[this._prefs.mode].origin;
  },

  /*
   * Unloads all browsers and principals
   */
  _unloadAll() {
    if (this._principals != null) {
      this._principals = new WeakMap();
    }
    this._browsers = new Set();
    this.emit("targetUnloadAll");
  },

  /*
   * Checks if a browser is known
   *
   * This will cause an iteration through all known browsers.
   * That's ok, we don't expect a lot of browsers
   */
  _isBrowserKnown(browser) {
    for (let bRef of this._getBrowserRefs()) {
      let b = bRef.get();
      if (b && b.permanentKey === browser.permanentKey) {
        return true;
      }
    }

    return false;
  },

  /*
   * Obtains all known browser refs
   */
  _getBrowserRefs() {
    // Some code may try to emit messages after teardown.
    if (!this._browsers) {
      return [];
    }
    let refs = [];
    for (let bRef of this._browsers) {
      /*
       * even though we hold a weak ref to browser, it seems that browser
       * objects aren't gc'd immediately after a tab closes. They stick around
       * in memory, but thankfully they don't have a documentURI in that case
       */
      let browser = bRef.get();
      if (browser && browser.documentURI) {
        refs.push(bRef);
      } else {
        // need to clean up principals because the browser object is not gc'ed
        // immediately
        this._principals.delete(browser);
        this._browsers.delete(bRef);
        this.emit("targetUnload");
      }
    }
    return refs;
  },

  /*
   * Receives a message from content.
   *
   * Keeps track of browsers for broadcast, relays messages to listeners.
   */
  _incomingMessage(id, message, target) {
    if (this.chanId !== id) {
      Cu.reportError(new Error("NewTabWebChannel unexpected message destination"));
    }

    /*
     * need to differentiate by browser, because event targets are created each
     * time a message is sent.
     */
    if (!this._isBrowserKnown(target.browser)) {
      this._browsers.add(Cu.getWeakReference(target.browser));
      this._principals.set(target.browser, Cu.getWeakReference(target.principal));
      this.emit("targetAdd");
    }

    try {
      let msg = JSON.parse(message);
      this.emit(msg.type, {data: msg.data, target});
    } catch (err) {
      Cu.reportError(err);
    }
  },

  /*
   * Sends a message to all known browsers
   */
  broadcast(actionType, message) {
    for (let bRef of this._getBrowserRefs()) {
      let browser = bRef.get();
      try {
        let principal = this._principals.get(browser).get();
        if (principal && browser && browser.documentURI) {
          this._channel.send({type: actionType, data: message}, {browser, principal});
        }
      } catch (e) {
        Cu.reportError(new Error("NewTabWebChannel WeakRef is dead"));
        this._principals.delete(browser);
      }
    }
  },

  /*
   * Sends a message to a specific target
   */
  send(actionType, message, target) {
    try {
      this._channel.send({type: actionType, data: message}, target);
    } catch (e) {
      // Web Channel might be dead
      Cu.reportError(e);
    }
  },

  /*
   * Pref change observer callback
   */
  _handlePrefChange(prefName, newState, forceState) { // eslint-disable-line no-unused-vars
    switch (prefName) {
      case PREF_ENABLED:
        if (!this._prefs.enabled && newState) {
          // changing state from disabled to enabled
          this.setupState();
        } else if (this._prefs.enabled && !newState) {
          // changing state from enabled to disabled
          this.tearDownState();
        }
        break;
      case PREF_MODE:
        if (this._prefs.mode !== newState) {
          // changing modes
          this.tearDownState();
          this.setupState();
        }
        break;
    }
  },

  /*
   * Sets up the internal state
   */
  setupState() {
    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    let mode = Preferences.get(PREF_MODE, "production");
    if (!(mode in NewTabRemoteResources.MODE_CHANNEL_MAP)) {
      mode = "production";
    }
    this._prefs.mode = mode;
    this._principals = new WeakMap();
    this._browsers = new Set();

    if (this._prefs.enabled) {
      this._channel = new WebChannel(this.chanId, Services.io.newURI(this.origin));
      this._channel.listen(this._incomingMessage);
    }
  },

  tearDownState() {
    if (this._channel) {
      this._channel.stopListening();
    }
    this._prefs = {};
    this._unloadAll();
    this._channel = null;
    this._principals = null;
    this._browsers = null;
  },

  init() {
    this.setupState();
    NewTabPrefsProvider.prefs.on(PREF_ENABLED, this._handlePrefChange);
    NewTabPrefsProvider.prefs.on(PREF_MODE, this._handlePrefChange);
  },

  uninit() {
    this.tearDownState();
    NewTabPrefsProvider.prefs.off(PREF_ENABLED, this._handlePrefChange);
    NewTabPrefsProvider.prefs.off(PREF_MODE, this._handlePrefChange);
  }
};

let NewTabWebChannel = new NewTabWebChannelImpl();
