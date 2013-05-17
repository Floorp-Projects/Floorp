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
  // Message handlers for this page.
  // We can have only one handler per message type.
  this._handlers = {};

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

  _dispatchMessage: function sysMessMgr_dispatchMessage(aType, aHandler, aMessage) {
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

    aHandler.handleMessage(wrapped ? aMessage
                                   : ObjectWrapper.wrap(aMessage, this._window));
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

    let handlers = this._handlers;
    if (!aHandler) {
      // Setting the handler to null means we don't want to receive messages
      // of this type anymore.
      delete handlers[aType];
      return;
    }

    // Last registered handler wins.
    handlers[aType] = aHandler;

    // Ask for the list of currently pending messages.
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
    if (aType in this._handlers) {
      return false;
    }

    return cpmm.sendSyncMessage("SystemMessageManager:HasPendingMessages",
                                { type: aType,
                                  uri: this._uri,
                                  manifest: this._manifest })[0];
  },

  uninit: function sysMessMgr_uninit()  {
    this._handlers = null;
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
    debug("receiveMessage " + aMessage.name + " for [" + aMessage.data.type + "] " +
          "with manifest = " + this._manifest + " and uri = " + this._uri);

    let msg = aMessage.data;

    if (aMessage.name == "SystemMessageManager:Message") {
      // Send an acknowledgement to parent to clean up the pending message,
      // so a re-launched app won't handle it again, which is redundant.
      cpmm.sendAsyncMessage("SystemMessageManager:Message:Return:OK",
                            { type: msg.type,
                              manifest: this._manifest,
                              uri: this._uri,
                              msgID: msg.msgID });
    }

    let messages = (aMessage.name == "SystemMessageManager:Message")
                   ? [msg.msg]
                   : msg.msgQueue;

    // We only dispatch messages when a handler is registered.
    let handler = this._handlers[msg.type];
    if (handler) {
      messages.forEach(function(aMsg) {
        this._dispatchMessage(msg.type, handler, aMsg);
      }, this);
    }

    // We need to notify the parent the system messages have been handled,
    // even if there are no handlers registered for them, so the parent can
    // release the CPU wake lock it took on our behalf.
    cpmm.sendAsyncMessage("SystemMessageManager:HandleMessagesDone",
                          { type: msg.type,
                            manifest: this._manifest,
                            uri: this._uri,
                            handledCount: messages.length });

    Services.obs.notifyObservers(/* aSubject */ null,
                                 "handle-system-messages-done",
                                 /* aData */ null);
  },

  // nsIDOMGlobalPropertyInitializer implementation.
  init: function sysMessMgr_init(aWindow) {
    debug("init");
    this.initHelper(aWindow, ["SystemMessageManager:Message",
                              "SystemMessageManager:GetPendingMessages:Return"]);

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
                                         Ci.nsIObserver]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{bc076ea0-609b-4d8f-83d7-5af7cbdc3bb2}"),
                                    contractID: "@mozilla.org/system-message-manager;1",
                                    interfaces: [Ci.nsIDOMNavigatorSystemMessages],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "System Messages"})
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageManager]);
