/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SystemMessagePermissionsChecker.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyServiceGetter(this, "powerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

// Limit the number of pending messages for a given page.
let kMaxPendingMessages;
try {
  kMaxPendingMessages = Services.prefs.getIntPref("dom.messages.maxPendingMessages");
} catch(e) {
  // getIntPref throws when the pref is not set.
  kMaxPendingMessages = 5;
}

const kMessages =["SystemMessageManager:GetPendingMessages",
                  "SystemMessageManager:HasPendingMessages",
                  "SystemMessageManager:Register",
                  "SystemMessageManager:Unregister",
                  "SystemMessageManager:Message:Return:OK",
                  "SystemMessageManager:AskReadyToRegister",
                  "SystemMessageManager:HandleMessagesDone",
                  "child-process-shutdown"]

function debug(aMsg) {
  // dump("-- SystemMessageInternal " + Date.now() + " : " + aMsg + "\n");
}


let defaultMessageConfigurator = {
  get mustShowRunningApp() {
    return false;
  }
};

const MSG_SENT_SUCCESS = 0;
const MSG_SENT_FAILURE_PERM_DENIED = 1;
const MSG_SENT_FAILURE_APP_NOT_RUNNING = 2;

// Implementation of the component used by internal users.

function SystemMessageInternal() {
  // The set of pages registered by installed apps. We keep the
  // list of pending messages for each page here also.
  this._pages = [];

  // The set of listeners. This is a multi-dimensional object. The _listeners
  // object itself is a map from manifest ID -> an array mapping proccesses to
  // windows. We do this so that we can track both what processes we have to
  // send system messages to as well as supporting the single-process case
  // where we track windows instead.
  this._listeners = {};

  this._webappsRegistryReady = false;
  this._bufferedSysMsgs = [];

  this._cpuWakeLocks = {};

  this._configurators = {};

  Services.obs.addObserver(this, "xpcom-shutdown", false);
  Services.obs.addObserver(this, "webapps-registry-start", false);
  Services.obs.addObserver(this, "webapps-registry-ready", false);
  kMessages.forEach(function(aMsg) {
    ppmm.addMessageListener(aMsg, this);
  }, this);

  Services.obs.notifyObservers(this, "system-message-internal-ready", null);
}

SystemMessageInternal.prototype = {

  _getMessageConfigurator: function _getMessageConfigurator(aType) {
    debug("_getMessageConfigurator for type: " + aType);
    if (this._configurators[aType] === undefined) {
      let contractID = "@mozilla.org/dom/system-messages/configurator/" + aType + ";1";
      if (contractID in Cc) {
        debug(contractID + " is registered, creating an instance");
        this._configurators[aType] =
          Cc[contractID].createInstance(Ci.nsISystemMessagesConfigurator);
      } else {
        debug(contractID + "is not registered, caching the answer");
        this._configurators[aType] = null;
      }
    }
    return this._configurators[aType] || defaultMessageConfigurator;
  },

  _cancelCpuWakeLock: function _cancelCpuWakeLock(aPageKey) {
    let cpuWakeLock = this._cpuWakeLocks[aPageKey];
    if (cpuWakeLock) {
      debug("Releasing the CPU wake lock for page key = " + aPageKey);
      cpuWakeLock.wakeLock.unlock();
      cpuWakeLock.timer.cancel();
      delete this._cpuWakeLocks[aPageKey];
    }
  },

  _acquireCpuWakeLock: function _acquireCpuWakeLock(aPageKey) {
    let cpuWakeLock = this._cpuWakeLocks[aPageKey];
    if (!cpuWakeLock) {
      // We have to ensure the CPU doesn't sleep during the process of the page
      // handling the system messages, so that they can be handled on time.
      debug("Acquiring a CPU wake lock for page key = " + aPageKey);
      cpuWakeLock = this._cpuWakeLocks[aPageKey] = {
        wakeLock: powerManagerService.newWakeLock("cpu"),
        timer: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),
        lockCount: 1
      };
    } else {
      // We've already acquired the CPU wake lock for this page,
      // so just add to the lock count and extend the timeout.
      cpuWakeLock.lockCount++;
    }

    // Set a watchdog to avoid locking the CPU wake lock too long,
    // because it'd exhaust the battery quickly which is very bad.
    // This could probably happen if the app failed to launch or
    // handle the system messages due to any unexpected reasons.
    cpuWakeLock.timer.initWithCallback(function timerCb() {
      debug("Releasing the CPU wake lock because the system messages " +
            "were not handled by its registered page before time out.");
      this._cancelCpuWakeLock(aPageKey);
    }.bind(this), 30000, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _releaseCpuWakeLock: function _releaseCpuWakeLock(aPageKey, aHandledCount) {
    let cpuWakeLock = this._cpuWakeLocks[aPageKey];
    if (cpuWakeLock) {
      cpuWakeLock.lockCount -= aHandledCount;
      if (cpuWakeLock.lockCount <= 0) {
        debug("Unlocking the CPU wake lock now that the system messages " +
              "have been successfully handled by its registered page.");
        this._cancelCpuWakeLock(aPageKey);
      }
    }
  },

  _findPage: function _findPage(aType, aPageURL, aManifestURL) {
    let page = null;
    this._pages.some(function(aPage) {
      if (this._isPageMatched(aPage, aType, aPageURL, aManifestURL)) {
        page = aPage;
      }
      return page !== null;
    }, this);
    return page;
  },

  sendMessage: function sendMessage(aType, aMessage, aPageURI, aManifestURI, aExtra) {
    // Buffer system messages until the webapps' registration is ready,
    // so that we can know the correct pages registered to be sent.
    if (!this._webappsRegistryReady) {
      this._bufferedSysMsgs.push({ how: "send",
                                   type: aType,
                                   msg: aMessage,
                                   pageURI: aPageURI,
                                   manifestURI: aManifestURI,
                                   extra: aExtra });
      return;
    }

    // Give this message an ID so that we can identify the message and
    // clean it up from the pending message queue when apps receive it.
    let messageID = gUUIDGenerator.generateUUID().toString();

    debug("Sending " + aType + " " + JSON.stringify(aMessage) +
      " for " + aPageURI.spec + " @ " + aManifestURI.spec +
      '; extra: ' + JSON.stringify(aExtra));

    let result = this._sendMessageCommon(aType,
                                         aMessage,
                                         messageID,
                                         aPageURI.spec,
                                         aManifestURI.spec,
                                         aExtra);
    debug("Returned status of sending message: " + result);

    // Don't need to open the pages and queue the system message
    // which was not allowed to be sent.
    if (result === MSG_SENT_FAILURE_PERM_DENIED) {
      return;
    }

    let page = this._findPage(aType, aPageURI.spec, aManifestURI.spec);
    if (page) {
      // Queue this message in the corresponding pages.
      this._queueMessage(page, aMessage, messageID);

      this._openAppPage(page, aMessage, aExtra, result);
    }
  },

  broadcastMessage: function broadcastMessage(aType, aMessage, aExtra) {
    // Buffer system messages until the webapps' registration is ready,
    // so that we can know the correct pages registered to be broadcasted.
    if (!this._webappsRegistryReady) {
      this._bufferedSysMsgs.push({ how: "broadcast",
                                   type: aType,
                                   msg: aMessage,
                                   extra: aExtra });
      return;
    }

    // Give this message an ID so that we can identify the message and
    // clean it up from the pending message queue when apps receive it.
    let messageID = gUUIDGenerator.generateUUID().toString();

    debug("Broadcasting " + aType + " " + JSON.stringify(aMessage) +
      '; extra = ' + JSON.stringify(aExtra));
    // Find pages that registered an handler for this type.
    this._pages.forEach(function(aPage) {
      if (aPage.type == aType) {
        let result = this._sendMessageCommon(aType,
                                             aMessage,
                                             messageID,
                                             aPage.uri,
                                             aPage.manifest,
                                             aExtra);
        debug("Returned status of sending message: " + result);


        // Don't need to open the pages and queue the system message
        // which was not allowed to be sent.
        if (result === MSG_SENT_FAILURE_PERM_DENIED) {
          return;
        }

        // Queue this message in the corresponding pages.
        this._queueMessage(aPage, aMessage, messageID);

        this._openAppPage(aPage, aMessage, aExtra, result);
      }
    }, this);
  },

  registerPage: function registerPage(aType, aPageURI, aManifestURI) {
    if (!aPageURI || !aManifestURI) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let pageURL = aPageURI.spec;
    let manifestURL = aManifestURI.spec;

    // Don't register duplicates for this tuple.
    let page = this._findPage(aType, pageURL, manifestURL);
    if (page) {
      debug("Ignoring duplicate registration of " +
            [aType, pageURL, manifestURL]);
      return;
    }

    this._pages.push({ type: aType,
                       uri: pageURL,
                       manifest: manifestURL,
                       pendingMessages: [] });
  },

  _findTargetIndex: function _findTargetIndex(aTargets, aTarget) {
    if (!aTargets || !aTarget) {
      return -1;
    }
    for (let index = 0; index < aTargets.length; ++index) {
      let target = aTargets[index];
      if (target.target === aTarget) {
        return index;
      }
    }
    return -1;
  },

  _isEmptyObject: function _isEmptyObject(aObj) {
    for (let name in aObj) {
      return false;
    }
    return true;
  },

  _removeTargetFromListener: function _removeTargetFromListener(aTarget,
                                                                aManifest,
                                                                aRemoveListener,
                                                                aUri) {
    let targets = this._listeners[aManifest];
    if (!targets) {
      return false;
    }

    let index = this._findTargetIndex(targets, aTarget);
    if (index === -1) {
      return false;
    }

    if (aRemoveListener) {
      debug("remove the listener for " + aManifest);
      delete this._listeners[aManifest];
      return true;
    }

    let target = targets[index];
    if (aUri && target.winCounts[aUri] !== undefined &&
        --target.winCounts[aUri] === 0) {
      delete target.winCounts[aUri];
    }

    if (this._isEmptyObject(target.winCounts)) {
      if (targets.length === 1) {
        // If it's the only one, get rid of this manifest entirely.
        debug("remove the listener for " + aManifest);
        delete this._listeners[aManifest];
      } else {
        // If more than one left, remove this one and leave the rest.
        targets.splice(index, 1);
      }
    }
    return true;
  },

  receiveMessage: function receiveMessage(aMessage) {
    let msg = aMessage.json;

    // To prevent the hacked child process from sending commands to parent
    // to manage system messages, we need to check its manifest URL.
    if (["SystemMessageManager:Register",
         "SystemMessageManager:Unregister",
         "SystemMessageManager:GetPendingMessages",
         "SystemMessageManager:HasPendingMessages",
         "SystemMessageManager:Message:Return:OK",
         "SystemMessageManager:HandleMessagesDone"].indexOf(aMessage.name) != -1) {
      if (!aMessage.target.assertContainApp(msg.manifest)) {
        debug("Got message from a child process containing illegal manifest URL.");
        return null;
      }
    }

    switch(aMessage.name) {
      case "SystemMessageManager:AskReadyToRegister":
        return true;
        break;
      case "SystemMessageManager:Register":
      {
        debug("Got Register from " + msg.uri + " @ " + msg.manifest);
        let uri = msg.uri;
        let targets, index;
        if (!(targets = this._listeners[msg.manifest])) {
          let winCounts = {};
          winCounts[uri] = 1;
          this._listeners[msg.manifest] = [{ target: aMessage.target,
                                             winCounts: winCounts }];
        } else if ((index = this._findTargetIndex(targets, aMessage.target)) === -1) {
          let winCounts = {};
          winCounts[uri] = 1;
          targets.push({ target: aMessage.target,
                         winCounts: winCounts });
        } else {
          let winCounts = targets[index].winCounts;
          if (winCounts[uri] === undefined) {
            winCounts[uri] = 1;
          } else {
            winCounts[uri]++;
          }
        }

        debug("listeners for " + msg.manifest + " innerWinID " + msg.innerWindowID);
        break;
      }
      case "child-process-shutdown":
      {
        debug("Got child-process-shutdown from " + aMessage.target);
        for (let manifest in this._listeners) {
          // See if any processes in this manifest have this target.
          if (this._removeTargetFromListener(aMessage.target, manifest, true, null)) {
            break;
          }
        }
        break;
      }
      case "SystemMessageManager:Unregister":
      {
        debug("Got Unregister from " + aMessage.target + "innerWinID " + msg.innerWindowID);
        this._removeTargetFromListener(aMessage.target, msg.manifest, false, msg.uri);
        break;
      }
      case "SystemMessageManager:GetPendingMessages":
      {
        debug("received SystemMessageManager:GetPendingMessages " + msg.type +
          " for " + msg.uri + " @ " + msg.manifest);

        // This is a sync call used to return the pending messages for a page.
        // Find the right page to get its corresponding pending messages.
        let page = this._findPage(msg.type, msg.uri, msg.manifest);
        if (!page) {
          return;
        }

        // Return the |msg| of each pending message (drop the |msgID|).
        let pendingMessages = [];
        page.pendingMessages.forEach(function(aMessage) {
          pendingMessages.push(aMessage.msg);
        });

        // Clear the pending queue for this page. This is OK since we'll store
        // pending messages in the content process (|SystemMessageManager|).
        page.pendingMessages.length = 0;

        // Send the array of pending messages.
        aMessage.target
                .sendAsyncMessage("SystemMessageManager:GetPendingMessages:Return",
                                  { type: msg.type,
                                    manifest: msg.manifest,
                                    uri: msg.uri,
                                    msgQueue: pendingMessages });
        break;
      }
      case "SystemMessageManager:HasPendingMessages":
      {
        debug("received SystemMessageManager:HasPendingMessages " + msg.type +
          " for " + msg.uri + " @ " + msg.manifest);

        // This is a sync call used to return if a page has pending messages.
        // Find the right page to get its corresponding pending messages.
        let page = this._findPage(msg.type, msg.uri, msg.manifest);
        if (!page) {
          return false;
        }

        return page.pendingMessages.length != 0;
        break;
      }
      case "SystemMessageManager:Message:Return:OK":
      {
        debug("received SystemMessageManager:Message:Return:OK " + msg.type +
          " for " + msg.uri + " @ " + msg.manifest);

        // We need to clean up the pending message since the app has already
        // received it, thus avoiding the re-lanunched app handling it again.
        let page = this._findPage(msg.type, msg.uri, msg.manifest);
        if (page) {
          let pendingMessages = page.pendingMessages;
          for (let i = 0; i < pendingMessages.length; i++) {
            if (pendingMessages[i].msgID === msg.msgID) {
              pendingMessages.splice(i, 1);
              break;
            }
          }
        }
        break;
      }
      case "SystemMessageManager:HandleMessagesDone":
      {
        debug("received SystemMessageManager:HandleMessagesDone " + msg.type +
          " with " + msg.handledCount + " for " + msg.uri + " @ " + msg.manifest);

        // A page has finished handling some of its system messages, so we try
        // to release the CPU wake lock we acquired on behalf of that page.
        this._releaseCpuWakeLock(this._createKeyForPage(msg), msg.handledCount);
        break;
      }
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        kMessages.forEach(function(aMsg) {
          ppmm.removeMessageListener(aMsg, this);
        }, this);
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "webapps-registry-start");
        Services.obs.removeObserver(this, "webapps-registry-ready");
        ppmm = null;
        this._pages = null;
        this._bufferedSysMsgs = null;
        break;
      case "webapps-registry-start":
        this._webappsRegistryReady = false;
        break;
      case "webapps-registry-ready":
        // After the webapps' registration has been done for sure,
        // re-fire the buffered system messages if there is any.
        this._webappsRegistryReady = true;
        this._bufferedSysMsgs.forEach(function(aSysMsg) {
          switch (aSysMsg.how) {
            case "send":
              this.sendMessage(
                aSysMsg.type, aSysMsg.msg, aSysMsg.pageURI, aSysMsg.manifestURI, aSysMsg.extra);
              break;
            case "broadcast":
              this.broadcastMessage(aSysMsg.type, aSysMsg.msg, aSysMsg.extra);
              break;
          }
        }, this);
        this._bufferedSysMsgs.length = 0;
        break;
    }
  },

  _queueMessage: function _queueMessage(aPage, aMessage, aMessageID) {
    // Queue the message for this page because we've never known if an app is
    // opened or not. We'll clean it up when the app has already received it.
    aPage.pendingMessages.push({ msg: aMessage, msgID: aMessageID });
    if (aPage.pendingMessages.length > kMaxPendingMessages) {
      aPage.pendingMessages.splice(0, 1);
    }
  },

  _openAppPage: function _openAppPage(aPage, aMessage, aExtra, aMsgSentStatus) {
    // This means the app must be brought to the foreground.
    let showApp = this._getMessageConfigurator(aPage.type).mustShowRunningApp;

    // We should send the open-app message if the system message was
    // not sent, or if it was sent but we should show the app anyway.
    if ((aMsgSentStatus === MSG_SENT_SUCCESS) && !showApp) {
      return;
    }

    // This flag means the app must *only* be brought to the foreground
    // and we don't need to load the app to handle messages.
    let onlyShowApp = (aMsgSentStatus === MSG_SENT_SUCCESS) && showApp;

    // We don't need to send the full object to observers.
    let page = { uri: aPage.uri,
                 manifest: aPage.manifest,
                 type: aPage.type,
                 extra: aExtra,
                 target: aMessage.target,
                 onlyShowApp: onlyShowApp,
                 showApp: showApp };
    debug("Asking to open " + JSON.stringify(page));
    Services.obs.notifyObservers(this, "system-messages-open-app", JSON.stringify(page));
  },

  _isPageMatched: function _isPageMatched(aPage, aType, aPageURI, aManifestURI) {
    return (aPage.type === aType &&
            aPage.manifest === aManifestURI &&
            aPage.uri === aPageURI)
  },

  _createKeyForPage: function _createKeyForPage(aPage) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let hasher = Cc["@mozilla.org/security/hash;1"]
                   .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);

    // add uri and action to the hash
    ["type", "manifest", "uri"].forEach(function(aProp) {
      let data = converter.convertToByteArray(aPage[aProp], {});
      hasher.update(data, data.length);
    });

    return hasher.finish(true);
  },

  _sendMessageCommon:
    function _sendMessageCommon(aType, aMessage, aMessageID, aPageURI, aManifestURI, aExtra) {
    // Don't send the system message not granted by the app's permissions.
    if (!SystemMessagePermissionsChecker
          .isSystemMessagePermittedToSend(aType,
                                          aPageURI,
                                          aManifestURI)) {
      return MSG_SENT_FAILURE_PERM_DENIED;
    }

    let appPageIsRunning = false;
    let pageKey = this._createKeyForPage({ type: aType,
                                           manifest: aManifestURI,
                                           uri: aPageURI });

    let targets = this._listeners[aManifestURI];
    if (targets) {
      for (let index = 0; index < targets.length; ++index) {
        let target = targets[index];
        // We only need to send the system message to the targets (processes)
        // which contain the window page that matches the manifest/page URL of
        // the destination of system message.
        if (target.winCounts[aPageURI] === undefined) {
          continue;
        }

        appPageIsRunning = true;
        // We need to acquire a CPU wake lock for that page and expect that
        // we'll receive a "SystemMessageManager:HandleMessagesDone" message
        // when the page finishes handling the system message. At that point,
        // we'll release the lock we acquired.
        this._acquireCpuWakeLock(pageKey);

        // Multiple windows can share the same target (process), the content
        // window needs to check if the manifest/page URL is matched. Only
        // *one* window should handle the system message.
        let manager = target.target;
        manager.sendAsyncMessage("SystemMessageManager:Message",
                                 { type: aType,
                                   msg: aMessage,
                                   manifest: aManifestURI,
                                   uri: aPageURI,
                                   msgID: aMessageID });
      }
    }

    if (!appPageIsRunning) {
      // The app page isn't running and relies on the 'open-app' chrome event to
      // wake it up. We still need to acquire a CPU wake lock for that page and
      // expect that we will receive a "SystemMessageManager:HandleMessagesDone"
      // message when the page finishes handling the system message with other
      // pending messages. At that point, we'll release the lock we acquired.
      this._acquireCpuWakeLock(pageKey);
      return MSG_SENT_FAILURE_APP_NOT_RUNNING;
    } else {
      return MSG_SENT_SUCCESS;
    }

  },

  classID: Components.ID("{70589ca5-91ac-4b9e-b839-d6a88167d714}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesInternal, Ci.nsIObserver])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageInternal]);
