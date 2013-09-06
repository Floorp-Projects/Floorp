/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

// Observer topics.
const kPrefenceChangedObserverTopic     = "nsPref:changed";
const kSysMsgListenerReadyObserverTopic = "system-message-listener-ready";
const kXpcomShutdownObserverTopic       = "xpcom-shutdown";

// Preference keys.
const kPrefKeyRilDebuggingEnabled = "ril.debugging.enabled";

// Frame message names.
const kMsgNameChildProcessShutdown = "child-process-shutdown";

let DEBUG;
function debug(s) {
  dump("RilMessageManager: " + s + "\n");
}

this.RilMessageManager = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                         Ci.nsIObserver]),

  topicRegistrationNames: {
    cellbroadcast:    "RIL:RegisterCellBroadcastMsg",
    icc:              "RIL:RegisterIccMsg",
    mobileconnection: "RIL:RegisterMobileConnectionMsg",
    voicemail:        "RIL:RegisterVoicemailMsg",
  },

  /**
   * this.callbacksByName[< A string message name>] = {
   *   topic:    <A string topic>,
   *   callback: <A callback that accepts two parameters -- topic and msg>,
   * }
   */
  callbacksByName: {},

  // Manage message targets in terms of topic. Only the authorized and
  // registered contents can receive related messages.
  targetsByTopic: {},
  topics: [],

  targetMessageQueue: [],
  ready: false,

  _init: function _init() {
    this._updateDebugFlag();

    Services.obs.addObserver(this, kPrefenceChangedObserverTopic, false);
    Services.obs.addObserver(this, kSysMsgListenerReadyObserverTopic, false);
    Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);

    ppmm.addMessageListener(kMsgNameChildProcessShutdown, this);

    let callback = this._registerMessageTarget.bind(this);
    for (let topic in this.topicRegistrationNames) {
      let name = this.topicRegistrationNames[topic];
      this.registerMessageListeners(topic, [name], callback);
    }
  },

  _shutdown: function _shutdown() {
    if (!this.ready) {
      Services.obs.removeObserver(this, kSysMsgListenerReadyObserverTopic);
    }
    Services.obs.removeObserver(this, kPrefenceChangedObserverTopic);
    Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);

    for (let name in this.callbacksByName) {
      ppmm.removeMessageListener(name, this);
    }
    this.callbacksByName = null;

    ppmm.removeMessageListener(kMsgNameChildProcessShutdown, this);
    ppmm = null;

    this.targetsByTopic = null;
    this.targetMessageQueue = null;
  },

  _registerMessageTarget: function _registerMessageTarget(topic, msg) {
    let targets = this.targetsByTopic[topic];
    if (!targets) {
      targets = this.targetsByTopic[topic] = [];
      let list = this.topics;
      if (list.indexOf(topic) == -1) {
        list.push(topic);
      }
    }

    let target = msg.target;
    if (targets.indexOf(target) != -1) {
      if (DEBUG) debug("Already registered this target!");
      return;
    }

    targets.push(target);
    if (DEBUG) debug("Registered " + topic + " target: " + target);
  },

  _unregisterMessageTarget: function _unregisterMessageTarget(topic, target) {
    if (topic == null) {
      // Unregister the target for every topic when no topic is specified.
      for (let type of this.topics) {
        this._unregisterMessageTarget(type, target);
      }
      return;
    }

    // Unregister the target for a specified topic.
    let targets = this.targetsByTopic[topic];
    if (!targets) {
      return;
    }

    let index = targets.indexOf(target);
    if (index != -1) {
      targets.splice(index, 1);
      if (DEBUG) debug("Unregistered " + topic + " target: " + target);
    }
  },

  _enqueueTargetMessage: function _enqueueTargetMessage(topic, name, options) {
    let msg = { topic : topic,
                name : name,
                options : options };
    // Remove previous queued message of same message name, only one message
    // per message name is allowed in queue.
    let messageQueue = this.targetMessageQueue;
    for (let i = 0; i < messageQueue.length; i++) {
      if (messageQueue[i].name === name) {
        messageQueue.splice(i, 1);
        break;
      }
    }

    messageQueue.push(msg);
  },

  _sendTargetMessage: function _sendTargetMessage(topic, name, options) {
    if (!this.ready) {
      this._enqueueTargetMessage(topic, name, options);
      return;
    }

    let targets = this.targetsByTopic[topic];
    if (!targets) {
      return;
    }

    for (let target of targets) {
      target.sendAsyncMessage(name, options);
    }
  },

  _resendQueuedTargetMessage: function _resendQueuedTargetMessage() {
    // Here uses this._sendTargetMessage() to resend message, which will
    // enqueue message if listener is not ready.  So only resend after listener
    // is ready, or it will cause infinate loop and hang the system.

    // Dequeue and resend messages.
    for (let msg of this.targetMessageQueue) {
      this._sendTargetMessage(msg.topic, msg.name, msg.options);
    }
    this.targetMessageQueue = null;
  },

  _updateDebugFlag: function _updateDebugFlag() {
    try {
      DEBUG = RIL.DEBUG_RIL ||
              Services.prefs.getBoolPref(kPrefKeyRilDebuggingEnabled);
    } catch(e) {}
  },

  /**
   * nsIMessageListener interface methods.
   */

  receiveMessage: function receiveMessage(msg) {
    if (DEBUG) {
      debug("Received '" + msg.name + "' message from content process");
    }

    if (msg.name == kMsgNameChildProcessShutdown) {
      // By the time we receive child-process-shutdown, the child process has
      // already forgotten its permissions so we need to unregister the target
      // for every permission.
      this._unregisterMessageTarget(null, msg.target);
      return;
    }

    let entry = this.callbacksByName[msg.name];
    if (!entry) {
      if (DEBUG) debug("Ignoring unknown message type: " + msg.name);
      return null;
    }

    if (entry.topic && !msg.target.assertPermission(entry.topic)) {
      if (DEBUG) {
        debug("Message " + msg.name + " from a content process with no '" +
              entry.topic + "' privileges.");
      }
      return null;
    }

    return entry.callback(entry.topic, msg);
  },

  /**
   * nsIObserver interface methods.
   */

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case kSysMsgListenerReadyObserverTopic:
        this.ready = true;
        Services.obs.removeObserver(this, kSysMsgListenerReadyObserverTopic);

        this._resendQueuedTargetMessage();
        break;

      case kPrefenceChangedObserverTopic:
        if (data === kPrefKeyRilDebuggingEnabled) {
          this._updateDebugFlag();
        }
        break;

      case kXpcomShutdownObserverTopic:
        this._shutdown();
        break;
    }
  },

  /**
   * Public methods.
   */

  /**
   * @param topic
   *        A string for the topic of the registrating names. Also the
   *        permission to listen messages of these names.
   * @param names
   *        An array of string message names to listen.
   * @param callback
   *        A callback that accepts two parameters -- topic and msg.
   */
  registerMessageListeners: function registerMessageListeners(topic, names,
                                                              callback) {
    for (let name of names) {
      if (this.callbacksByName[name]) {
        if (DEBUG) {
          debug("Message name '" + name + "' was already registered. Ignored.");
        }
        continue;
      }

      this.callbacksByName[name] = { topic: topic, callback: callback };
      ppmm.addMessageListener(name, this);
    }
  },

  /**
   * Remove all listening names with specified callback.
   *
   * @param callback
   *        The callback previously registered for messages.
   */
  unregisterMessageListeners: function unregisterMessageListeners(callback) {
    let remains = {};
    for (let name in this.callbacksByName) {
      let entry = this.callbacksByName[name];
      if (entry.callback != callback) {
        remains[name] = entry;
      } else {
        ppmm.removeMessageListener(name, this);
      }
    }
    this.callbacksByName = remains;
  },

  sendMobileConnectionMessage: function sendMobileConnectionMessage(name,
                                                                    clientId,
                                                                    data) {
    this._sendTargetMessage("mobileconnection", name, {
      clientId: clientId,
      data: data
    });
  },

  sendVoicemailMessage: function sendVoicemailMessage(name, clientId, data) {
    this._sendTargetMessage("voicemail", name, {
      clientId: clientId,
      data: data
    });
  },

  sendCellBroadcastMessage: function sendCellBroadcastMessage(name, clientId,
                                                              data) {
    this._sendTargetMessage("cellbroadcast", name, {
      clientId: clientId,
      data: data
    });
  },

  sendIccMessage: function sendIccMessage(name, clientId, data) {
    this._sendTargetMessage("icc", name, {
      clientId: clientId,
      data: data
    });
  }
};

RilMessageManager._init();

this.EXPORTED_SYMBOLS = ["RilMessageManager"];
