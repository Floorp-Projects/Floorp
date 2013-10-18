/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ObjectWrapper.jsm");

const kSystemMessageInternalReady = "system-message-internal-ready";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function debug(aMsg) {
   // dump("-- SystemMessageManager " + Date.now() + " : " + aMsg + "\n");
}

// Implementation of the DOM API for system messages

function SystemMessageManager() {
  // If we have a system message handler registered for messages of type
  // |type|, this._dispatchers[type] equals {handler, messages, isHandling},
  // where
  //  - |handler| is the message handler that the page registered,
  //  - |messages| is a list of messages which we've received while
  //    dispatching messages to the handler, but haven't yet sent, and
  //  - |isHandling| indicates whether we're currently dispatching messages
  //    to this handler.
  this._dispatchers = {};

  // Pending messages for this page, keyed by message type.
  this._pendings = {};

  // Flag to specify if this process has already registered manifest.
  this._registerManifestReady = false;

  // Flag to determine this process is a parent or child process.
  let appInfo = Cc["@mozilla.org/xre/app-info;1"];
  this._isParentProcess =
    !appInfo || appInfo.getService(Ci.nsIXULRuntime)
                  .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

  // An oberver to listen to whether the |SystemMessageInternal| is ready.
  if (this._isParentProcess) {
    Services.obs.addObserver(this, kSystemMessageInternalReady, false);
  }
}

SystemMessageManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  _dispatchMessage: function sysMessMgr_dispatchMessage(aType, aDispatcher, aMessage) {
    if (aDispatcher.isHandling) {
      // Queue up the incomming message if we're currently dispatching a
      // message; we'll send the message once we finish with the current one.
      //
      // _dispatchMethod is reentrant because a page can spin up a nested
      // event loop from within a system message handler (e.g. via alert()),
      // and we can then try to send the page another message while it's
      // inside this nested event loop.
      aDispatcher.messages.push(aMessage);
      return;
    }

    aDispatcher.isHandling = true;

    // We get a json blob, but in some cases we want another kind of object
    // to be dispatched.
    // To do so, we check if we have a with a contract ID of
    // "@mozilla.org/dom/system-messages/wrapper/TYPE;1" component implementing
    // nsISystemMessageWrapper.
    debug("Dispatching " + JSON.stringify(aMessage) + "\n");
    let contractID = "@mozilla.org/dom/system-messages/wrapper/" + aType + ";1";
    let wrapped = false;

    if (contractID in Cc) {
      debug(contractID + " is registered, creating an instance");
      let wrapper = Cc[contractID].createInstance(Ci.nsISystemMessagesWrapper);
      if (wrapper) {
        aMessage = wrapper.wrapMessage(aMessage, this._window);
        wrapped = true;
        debug("wrapped = " + aMessage);
      }
    }

    aDispatcher.handler
      .handleMessage(wrapped ? aMessage
                             : ObjectWrapper.wrap(aMessage, this._window));

    // We need to notify the parent one of the system messages has been handled,
    // so the parent can release the CPU wake lock it took on our behalf.
    cpmm.sendAsyncMessage("SystemMessageManager:HandleMessagesDone",
                          { type: aType,
                            manifest: this._manifest,
                            uri: this._uri,
                            handledCount: 1 });

    aDispatcher.isHandling = false;
    if (aDispatcher.messages.length > 0) {
      this._dispatchMessage(aType, aDispatcher, aDispatcher.messages.shift());
    }
  },

  mozSetMessageHandler: function sysMessMgr_setMessageHandler(aType, aHandler) {
    debug("set message handler for [" + aType + "] " + aHandler);

    if (this._isInBrowserElement) {
      debug("the app loaded in the browser cannot set message handler");
      // Don't throw there, but ignore the registration.
      return;
    }

    if (!aType) {
      // Just bail out if we have no type.
      return;
    }

    let dispatchers = this._dispatchers;
    if (!aHandler) {
      // Setting the dispatcher to null means we don't want to handle messages
      // for this type anymore.
      delete dispatchers[aType];
      return;
    }

    // Last registered handler wins.
    dispatchers[aType] = { handler: aHandler, messages: [], isHandling: false };

    // Ask for the list of currently pending messages.
    this.addMessageListeners("SystemMessageManager:GetPendingMessages:Return");
    cpmm.sendAsyncMessage("SystemMessageManager:GetPendingMessages",
                          { type: aType,
                            uri: this._uri,
                            manifest: this._manifest });
  },

  mozHasPendingMessage: function sysMessMgr_hasPendingMessage(aType) {
    debug("asking pending message for [" + aType + "]");

    if (this._isInBrowserElement) {
      debug("the app loaded in the browser cannot ask pending message");
      // Don't throw there, but pretend to have no messages available.
      return false;
    }

    // If we have a handler for this type, we can't have any pending message.
    if (aType in this._dispatchers) {
      return false;
    }

    return cpmm.sendSyncMessage("SystemMessageManager:HasPendingMessages",
                                { type: aType,
                                  uri: this._uri,
                                  manifest: this._manifest })[0];
  },

  uninit: function sysMessMgr_uninit()  {
    this._dispatchers = null;
    this._pendings = null;

    if (this._isParentProcess) {
      Services.obs.removeObserver(this, kSystemMessageInternalReady);
    }

    if (this._isInBrowserElement) {
      debug("the app loaded in the browser doesn't need to unregister " +
            "the manifest for listening to the system messages");
      return;
    }

    cpmm.sendAsyncMessage("SystemMessageManager:Unregister",
                          { manifest: this._manifest,
                            uri: this._uri,
                            innerWindowID: this.innerWindowID });
  },

  // Possible messages:
  //
  //   - SystemMessageManager:Message
  //     This one will only be received when the child process is alive when
  //     the message is initially sent.
  //
  //   - SystemMessageManager:GetPendingMessages:Return
  //     This one will be received when the starting child process wants to
  //     retrieve the pending system messages from the parent (i.e. after
  //     sending SystemMessageManager:GetPendingMessages).
  receiveMessage: function sysMessMgr_receiveMessage(aMessage) {
    let msg = aMessage.data;
    debug("receiveMessage " + aMessage.name + " for [" + msg.type + "] " +
          "with manifest = " + msg.manifest + " and uri = " + msg.uri);

    // Multiple windows can share the same target (process), the content
    // window needs to check if the manifest/page URL is matched. Only
    // *one* window should handle the system message.
    if (msg.manifest !== this._manifest || msg.uri !== this._uri) {
      debug("This page shouldn't handle the messages because its " +
            "manifest = " + this._manifest + " and uri = " + this._uri);
      return;
    }

    if (aMessage.name == "SystemMessageManager:Message") {
      // Send an acknowledgement to parent to clean up the pending message,
      // so a re-launched app won't handle it again, which is redundant.
      cpmm.sendAsyncMessage("SystemMessageManager:Message:Return:OK",
                            { type: msg.type,
                              manifest: this._manifest,
                              uri: this._uri,
                              msgID: msg.msgID });
    } else if (aMessage.name == "SystemMessageManager:GetPendingMessages:Return") {
      this.removeMessageListeners(aMessage.name);
    }

    let messages = (aMessage.name == "SystemMessageManager:Message")
                   ? [msg.msg]
                   : msg.msgQueue;

    // We only dispatch messages when a handler is registered.
    let dispatcher = this._dispatchers[msg.type];
    if (dispatcher) {
      messages.forEach(function(aMsg) {
        this._dispatchMessage(msg.type, dispatcher, aMsg);
      }, this);
    } else {
      // We need to notify the parent that all the queued system messages have
      // been handled (notice |handledCount: messages.length|), so the parent
      // can release the CPU wake lock it took on our behalf.
      cpmm.sendAsyncMessage("SystemMessageManager:HandleMessagesDone",
                            { type: msg.type,
                              manifest: this._manifest,
                              uri: this._uri,
                              handledCount: messages.length });
    }

    if (!dispatcher || !dispatcher.isHandling) {
      // TODO: Bug 874353 - Remove SystemMessageHandledListener in ContentParent
      Services.obs.notifyObservers(/* aSubject */ null,
                                   "handle-system-messages-done",
                                   /* aData */ null);
    }
  },

  // nsIDOMGlobalPropertyInitializer implementation.
  init: function sysMessMgr_init(aWindow) {
    debug("init");
    this.initDOMRequestHelper(aWindow, ["SystemMessageManager:Message"]);

    let principal = aWindow.document.nodePrincipal;
    this._isInBrowserElement = principal.isInBrowserElement;
    this._uri = principal.URI.spec;

    let appsService = Cc["@mozilla.org/AppsService;1"]
                        .getService(Ci.nsIAppsService);
    this._manifest = appsService.getManifestURLByLocalId(principal.appId);

    // Two cases are valid to register the manifest for the current process:
    // 1. This is asked by a child process (parent process must be ready).
    // 2. Parent process has already constructed the |SystemMessageInternal|.
    // Otherwise, delay to do it when the |SystemMessageInternal| is ready.
    let readyToRegister = true;
    if (this._isParentProcess) {
      let ready = cpmm.sendSyncMessage(
        "SystemMessageManager:AskReadyToRegister", null);
      if (ready.length == 0 || !ready[0]) {
        readyToRegister = false;
      }
    }
    if (readyToRegister) {
      this._registerManifest();
    }

    debug("done");
  },

  observe: function sysMessMgr_observe(aSubject, aTopic, aData) {
    if (aTopic === kSystemMessageInternalReady) {
      this._registerManifest();
    }

    // Call the DOMRequestIpcHelper.observe method.
    this.__proto__.__proto__.observe.call(this, aSubject, aTopic, aData);
  },

  _registerManifest: function sysMessMgr_registerManifest() {
    if (this._isInBrowserElement) {
      debug("the app loaded in the browser doesn't need to register " +
            "the manifest for listening to the system messages");
      return;
    }

    if (!this._registerManifestReady) {
      cpmm.sendAsyncMessage("SystemMessageManager:Register",
                            { manifest: this._manifest,
                              uri: this._uri,
                              innerWindowID: this.innerWindowID });

      this._registerManifestReady = true;
    }
  },

  classID: Components.ID("{bc076ea0-609b-4d8f-83d7-5af7cbdc3bb2}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMNavigatorSystemMessages,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  classInfo: XPCOMUtils.generateCI({
    classID: Components.ID("{bc076ea0-609b-4d8f-83d7-5af7cbdc3bb2}"),
    contractID: "@mozilla.org/system-message-manager;1",
    interfaces: [Ci.nsIDOMNavigatorSystemMessages],
    flags: Ci.nsIClassInfo.DOM_OBJECT,
    classDescription: "System Messages"})
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageManager]);
