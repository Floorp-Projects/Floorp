/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

this.EXPORTED_SYMBOLS = ["InterAppCommService"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

const DEBUG = false;
function debug(aMsg) {
  dump("-- InterAppCommService: " + Date.now() + ": " + aMsg + "\n");
}

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "UUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyServiceGetter(this, "messenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

const kMessages =["Webapps:Connect",
                  "Webapps:GetConnections",
                  "InterAppConnection:Cancel",
                  "InterAppMessagePort:PostMessage",
                  "InterAppMessagePort:Register",
                  "InterAppMessagePort:Unregister",
                  "child-process-shutdown"];

/**
 * This module contains helpers for Inter-App Communication API [1] related
 * purposes, which plays the role of the central service receiving messages
 * from and interacting with the content processes.
 *
 * [1] https://wiki.mozilla.org/WebAPI/Inter_App_Communication_Alt_proposal
 */

this.InterAppCommService = {
  init: function() {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "webapps-clear-data", false);

    kMessages.forEach(function(aMsg) {
      ppmm.addMessageListener(aMsg, this);
    }, this);

    // This matrix is used for saving the inter-app connection info registered in
    // the app manifest. The object literal is defined as below:
    //
    // {
    //   "keyword1": {
    //     "subAppManifestURL1": {
    //       /* subscribed info */
    //     },
    //     "subAppManifestURL2": {
    //       /* subscribed info */
    //     },
    //     ...
    //   },
    //   "keyword2": {
    //     "subAppManifestURL3": {
    //       /* subscribed info */
    //     },
    //     ...
    //   },
    //   ...
    // }
    //
    // For example:
    //
    // {
    //   "foo": {
    //     "app://subApp1.gaiamobile.org/manifest.webapp": {
    //       pageURL: "app://subApp1.gaiamobile.org/handler.html",
    //       description: "blah blah",
    //       rules: { ... }
    //     },
    //     "app://subApp2.gaiamobile.org/manifest.webapp": {
    //       pageURL: "app://subApp2.gaiamobile.org/handler.html",
    //       description: "blah blah",
    //       rules: { ... }
    //     }
    //   },
    //   "bar": {
    //     "app://subApp3.gaiamobile.org/manifest.webapp": {
    //       pageURL: "app://subApp3.gaiamobile.org/handler.html",
    //       description: "blah blah",
    //       rules: { ... }
    //     }
    //   }
    // }
    //
    // TODO Bug 908999 - Update registered connections when app gets uninstalled.
    this._registeredConnections = {};

    // This matrix is used for saving the permitted connections, which allows
    // the messaging between publishers and subscribers. The object literal is
    // defined as below:
    //
    // {
    //   "keyword1": {
    //     "pubAppManifestURL1": [
    //       "subAppManifestURL1",
    //       "subAppManifestURL2",
    //       ...
    //     ],
    //     "pubAppManifestURL2": [
    //       "subAppManifestURL3",
    //       "subAppManifestURL4",
    //       ...
    //     ],
    //     ...
    //   },
    //   "keyword2": {
    //     "pubAppManifestURL3": [
    //       "subAppManifestURL5",
    //       ...
    //     ],
    //     ...
    //   },
    //   ...
    // }
    //
    // For example:
    //
    // {
    //   "foo": {
    //     "app://pubApp1.gaiamobile.org/manifest.webapp": [
    //       "app://subApp1.gaiamobile.org/manifest.webapp",
    //       "app://subApp2.gaiamobile.org/manifest.webapp"
    //     ],
    //     "app://pubApp2.gaiamobile.org/manifest.webapp": [
    //       "app://subApp3.gaiamobile.org/manifest.webapp",
    //       "app://subApp4.gaiamobile.org/manifest.webapp"
    //     ]
    //   },
    //   "bar": {
    //     "app://pubApp3.gaiamobile.org/manifest.webapp": [
    //       "app://subApp5.gaiamobile.org/manifest.webapp",
    //     ]
    //   }
    // }
    //
    // TODO Bug 908999 - Update allowed connections when app gets uninstalled.
    this._allowedConnections = {};

    // This matrix is used for saving the caller info from the content process,
    // which is indexed by a random UUID, to know where to return the promise
    // resolvser's callback when the prompt UI for allowing connections returns.
    // An example of the object literal is shown as below:
    //
    // {
    //   "fooID": {
    //     outerWindowID: 12,
    //     requestID: 34,
    //     target: pubAppTarget1
    //   },
    //   "barID": {
    //     outerWindowID: 56,
    //     requestID: 78,
    //     target: pubAppTarget2
    //   }
    // }
    //
    // where |outerWindowID| is the ID of the window requesting the connection,
    //       |requestID| is the ID specifying the promise resolver to return,
    //       |target| is the target of the process requesting the connection.
    this._promptUICallers = {};

    // This matrix is used for saving the pair of message ports, which is indexed
    // by a random UUID, so that each port can know whom it should talk to.
    // An example of the object literal is shown as below:
    //
    // {
    //   "UUID1": {
    //     keyword: "keyword1",
    //     publisher: {
    //       manifestURL: "app://pubApp1.gaiamobile.org/manifest.webapp",
    //       target: pubAppTarget1,
    //       pageURL: "app://pubApp1.gaiamobile.org/caller.html",
    //       messageQueue: [...]
    //     },
    //     subscriber: {
    //       manifestURL: "app://subApp1.gaiamobile.org/manifest.webapp",
    //       target: subAppTarget1,
    //       pageURL: "app://pubApp1.gaiamobile.org/handler.html",
    //       messageQueue: [...]
    //     }
    //   },
    //   "UUID2": {
    //     keyword: "keyword2",
    //     publisher: {
    //       manifestURL: "app://pubApp2.gaiamobile.org/manifest.webapp",
    //       target: pubAppTarget2,
    //       pageURL: "app://pubApp2.gaiamobile.org/caller.html",
    //       messageQueue: [...]
    //     },
    //     subscriber: {
    //       manifestURL: "app://subApp2.gaiamobile.org/manifest.webapp",
    //       target: subAppTarget2,
    //       pageURL: "app://pubApp2.gaiamobile.org/handler.html",
    //       messageQueue: [...]
    //     }
    //   }
    // }
    this._messagePortPairs = {};
  },

  /**
   * Registration of a page that wants to be connected to other apps through
   * the Inter-App Communication API.
   *
   * @param aKeyword        The connection's keyword.
   * @param aHandlerPageURI The URI of the handler's page.
   * @param aManifestURI    The webapp's manifest URI.
   * @param aDescription    The connection's description.
   * @param aRules          The connection's rules.
   */
  registerConnection: function(aKeyword, aHandlerPageURI, aManifestURI,
                               aDescription, aRules) {
    let manifestURL = aManifestURI.spec;
    let pageURL = aHandlerPageURI.spec;

    if (DEBUG) {
      debug("registerConnection: aKeyword: " + aKeyword +
            " manifestURL: " + manifestURL + " pageURL: " + pageURL +
            " aDescription: " + aDescription +
            " aRules.minimumAccessLevel: " + aRules.minimumAccessLevel +
            " aRules.manifestURLs: " + aRules.manifestURLs +
            " aRules.installOrigins: " + aRules.installOrigins);
    }

    let subAppManifestURLs = this._registeredConnections[aKeyword];
    if (!subAppManifestURLs) {
      subAppManifestURLs = this._registeredConnections[aKeyword] = {};
    }

    subAppManifestURLs[manifestURL] = {
      pageURL: pageURL,
      description: aDescription,
      rules: aRules,
      manifestURL: manifestURL
    };
  },

  _matchMinimumAccessLevel: function(aRules, aAppStatus) {
    if (!aRules || !aRules.minimumAccessLevel) {
      if (DEBUG) {
        debug("rules.minimumAccessLevel is not available. No need to match.");
      }
      return true;
    }

    let minAccessLevel = aRules.minimumAccessLevel;
    switch (minAccessLevel) {
      case "web":
        if (aAppStatus == Ci.nsIPrincipal.APP_STATUS_INSTALLED ||
            aAppStatus == Ci.nsIPrincipal.APP_STATUS_PRIVILEGED ||
            aAppStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED) {
          return true;
        }
        break;
      case "privileged":
        if (aAppStatus == Ci.nsIPrincipal.APP_STATUS_PRIVILEGED ||
            aAppStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED) {
          return true;
        }
        break;
      case "certified":
        if (aAppStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED) {
          return true;
        }
        break;
    }

    if (DEBUG) {
      debug("rules.minimumAccessLevel is not matched!" +
            " minAccessLevel: " + minAccessLevel +
            " aAppStatus : " + aAppStatus);
    }
    return false;
  },

  _matchManifestURLs: function(aRules, aManifestURL) {
    if (!aRules || !Array.isArray(aRules.manifestURLs)) {
      if (DEBUG) {
        debug("rules.manifestURLs is not available. No need to match.");
      }
      return true;
    }

    let manifestURLs = aRules.manifestURLs;
    if (manifestURLs.indexOf(aManifestURL) != -1) {
      return true;
    }

    if (DEBUG) {
      debug("rules.manifestURLs is not matched!" +
            " manifestURLs: " + manifestURLs +
            " aManifestURL : " + aManifestURL);
    }
    return false;
  },

  _matchInstallOrigins: function(aRules, aInstallOrigin) {
    if (!aRules || !Array.isArray(aRules.installOrigins)) {
      if (DEBUG) {
        debug("rules.installOrigins is not available. No need to match.");
      }
      return true;
    }

    let installOrigins = aRules.installOrigins;
    if (installOrigins.indexOf(aInstallOrigin) != -1) {
      return true;
    }

    if (DEBUG) {
      debug("rules.installOrigins is not matched!" +
            " installOrigins: " + installOrigins +
            " installOrigin : " + aInstallOrigin);
    }
    return false;
  },

  _matchRules: function(aPubAppManifestURL, aPubRules,
                        aSubAppManifestURL, aSubRules) {
    let pubApp = appsService.getAppByManifestURL(aPubAppManifestURL);
    let subApp = appsService.getAppByManifestURL(aSubAppManifestURL);

    let isPubAppCertified =
      (pubApp.appStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED);

    let isSubAppCertified =
      (subApp.appStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED);

    // TODO Bug 907068 In the initiative step, we only expose this API to
    // certified apps to meet the time line. Eventually, we need to make
    // it available for the non-certified apps as well. For now, only the
    // certified apps can match the rules.
    if (!isPubAppCertified || !isSubAppCertified) {
      if (DEBUG) {
        debug("Only certified apps are allowed to do connections.");
      }
      return false;
    }

    if (!aPubRules && !aSubRules) {
      if (DEBUG) {
        debug("No rules for publisher and subscriber. No need to match.");
      }
      return true;
    }

    // Check minimumAccessLevel.
    if (!this._matchMinimumAccessLevel(aPubRules, subApp.appStatus) ||
        !this._matchMinimumAccessLevel(aSubRules, pubApp.appStatus)) {
      return false;
    }

    // Check manifestURLs.
    if (!this._matchManifestURLs(aPubRules, aSubAppManifestURL) ||
        !this._matchManifestURLs(aSubRules, aPubAppManifestURL)) {
      return false;
    }

    // Check installOrigins. Note that we only check the install origin for the
    // non-certified app, because the certified app doesn't have install origin.
    if ((!isSubAppCertified &&
         !this._matchInstallOrigins(aPubRules, subApp.installOrigin)) ||
        (!isPubAppCertified &&
         !this._matchInstallOrigins(aSubRules, pubApp.installOrigin))) {
      return false;
    }

    if (DEBUG) debug("All rules are matched.");
    return true;
  },

  _dispatchMessagePorts: function(aKeyword, aPubAppManifestURL,
                                  aAllowedSubAppManifestURLs,
                                  aTarget, aOuterWindowID, aRequestID) {
    if (DEBUG) {
      debug("_dispatchMessagePorts: aKeyword: " + aKeyword +
            " aPubAppManifestURL: " + aPubAppManifestURL +
            " aAllowedSubAppManifestURLs: " + aAllowedSubAppManifestURLs);
    }

    if (aAllowedSubAppManifestURLs.length == 0) {
      if (DEBUG) debug("No apps are allowed to connect. Returning.");
      aTarget.sendAsyncMessage("Webapps:Connect:Return:KO",
                               { oid: aOuterWindowID, requestID: aRequestID });
      return;
    }

    let subAppManifestURLs = this._registeredConnections[aKeyword];
    if (!subAppManifestURLs) {
      if (DEBUG) debug("No apps are subscribed to connect. Returning.");
      aTarget.sendAsyncMessage("Webapps:Connect:Return:KO",
                               { oid: aOuterWindowID, requestID: aRequestID });
      return;
    }

    let messagePortIDs = [];
    aAllowedSubAppManifestURLs.forEach(function(aAllowedSubAppManifestURL) {
      let subscribedInfo = subAppManifestURLs[aAllowedSubAppManifestURL];
      if (!subscribedInfo) {
        if (DEBUG) {
          debug("The sunscribed info is not available. Skipping: " +
                aAllowedSubAppManifestURL);
        }
        return;
      }

      // The message port ID is aimed for identifying the coupling targets
      // to deliver messages with each other. This ID is centrally generated
      // by the parent and dispatched to both the sender and receiver ends
      // for creating their own message ports respectively.
      let messagePortID = UUIDGenerator.generateUUID().toString();
      this._messagePortPairs[messagePortID] = {
        keyword: aKeyword,
        publisher: {
          manifestURL: aPubAppManifestURL
        },
        subscriber: {
          manifestURL: aAllowedSubAppManifestURL
        }
      };

      // Fire system message to deliver the message port to the subscriber.
      messenger.sendMessage("connection",
        { keyword: aKeyword,
          messagePortID: messagePortID },
        Services.io.newURI(subscribedInfo.pageURL, null, null),
        Services.io.newURI(subscribedInfo.manifestURL, null, null));

      messagePortIDs.push(messagePortID);
    }, this);

    if (messagePortIDs.length == 0) {
      if (DEBUG) debug("No apps are subscribed to connect. Returning.");
      aTarget.sendAsyncMessage("Webapps:Connect:Return:KO",
                               { oid: aOuterWindowID, requestID: aRequestID });
      return;
    }

    // Return the message port IDs to open the message ports for the publisher.
    if (DEBUG) debug("messagePortIDs: " + messagePortIDs);
    aTarget.sendAsyncMessage("Webapps:Connect:Return:OK",
                             { keyword: aKeyword,
                               messagePortIDs: messagePortIDs,
                               oid: aOuterWindowID, requestID: aRequestID });
  },

  /**
   * Fetch the subscribers that are currently allowed to connect.
   *
   * @param aKeyword           The connection's keyword.
   * @param aPubAppManifestURL The manifest URL of the publisher.
   *
   * @param return an array of manifest URLs of the subscribers.
   */
  _getAllowedSubAppManifestURLs: function(aKeyword, aPubAppManifestURL) {
    let allowedPubAppManifestURLs = this._allowedConnections[aKeyword];
    if (!allowedPubAppManifestURLs) {
      return [];
    }

    let allowedSubAppManifestURLs =
      allowedPubAppManifestURLs[aPubAppManifestURL];
    if (!allowedSubAppManifestURLs) {
      return [];
    }

    return allowedSubAppManifestURLs;
  },

  /**
   * Add the newly selected apps into the allowed connections and return the
   * aggregated allowed connections.
   *
   * @param aKeyword           The connection's keyword.
   * @param aPubAppManifestURL The manifest URL of the publisher.
   * @param aSelectedApps      An array of the subscribers' information.
   *
   * @param return an array of manifest URLs of the subscribers.
   */
  _addSelectedApps: function(aKeyword, aPubAppManifestURL, aSelectedApps) {
    let allowedPubAppManifestURLs = this._allowedConnections[aKeyword];

    // Add a new entry for |aKeyword|.
    if (!allowedPubAppManifestURLs) {
      allowedPubAppManifestURLs = this._allowedConnections[aKeyword] = {};
    }

    let allowedSubAppManifestURLs =
      allowedPubAppManifestURLs[aPubAppManifestURL];

    // Add a new entry for |aPubAppManifestURL|.
    if (!allowedSubAppManifestURLs) {
      allowedSubAppManifestURLs =
        allowedPubAppManifestURLs[aPubAppManifestURL] = [];
    }

    // Add the selected apps into the existing set of allowed connections.
    aSelectedApps.forEach(function(aSelectedApp) {
      let allowedSubAppManifestURL = aSelectedApp.manifestURL;
      if (allowedSubAppManifestURLs.indexOf(allowedSubAppManifestURL) == -1) {
        allowedSubAppManifestURLs.push(allowedSubAppManifestURL);
      }
    });

    return allowedSubAppManifestURLs;
  },

  _connect: function(aMessage, aTarget) {
    let keyword = aMessage.keyword;
    let pubRules = aMessage.rules;
    let pubAppManifestURL = aMessage.manifestURL;
    let outerWindowID = aMessage.outerWindowID;
    let requestID = aMessage.requestID;

    let subAppManifestURLs = this._registeredConnections[keyword];
    if (!subAppManifestURLs) {
      if (DEBUG) {
        debug("No apps are subscribed for this connection. Returning.");
      }
      this._dispatchMessagePorts(keyword, pubAppManifestURL, [],
                                 aTarget, outerWindowID, requestID);
      return;
    }

    // Fetch the apps that are currently allowed to connect, so that users
    // don't need to select/allow them again, which means we only pop up the
    // prompt UI for the *new* connections.
    let allowedSubAppManifestURLs =
      this._getAllowedSubAppManifestURLs(keyword, pubAppManifestURL);

    // Check rules to see if a subscribed app is allowed to connect.
    let appsToSelect = [];
    for (let subAppManifestURL in subAppManifestURLs) {
      if (allowedSubAppManifestURLs.indexOf(subAppManifestURL) != -1) {
        if (DEBUG) {
          debug("Don't need to select again. Skipping: " + subAppManifestURL);
        }
        continue;
      }

      // Only rule-matched publishers/subscribers are allowed to connect.
      let subscribedInfo = subAppManifestURLs[subAppManifestURL];
      let subRules = subscribedInfo.rules;

      let matched =
        this._matchRules(pubAppManifestURL, pubRules,
                         subAppManifestURL, subRules);
      if (!matched) {
        if (DEBUG) {
          debug("Rules are not matched. Skipping: " + subAppManifestURL);
        }
        continue;
      }

      appsToSelect.push({
        manifestURL: subAppManifestURL,
        description: subscribedInfo.description
      });
    }

    if (appsToSelect.length == 0) {
      if (DEBUG) {
        debug("No additional apps need to be selected for this connection. " +
              "Just dispatch message ports for the existing connections.");
      }

      this._dispatchMessagePorts(keyword, pubAppManifestURL,
                                 allowedSubAppManifestURLs,
                                 aTarget, outerWindowID, requestID);
      return;
    }

    // Remember the caller info with an UUID so that we can know where to
    // return the promise resolver's callback when the prompt UI returns.
    let callerID = UUIDGenerator.generateUUID().toString();
    this._promptUICallers[callerID] = {
      outerWindowID: outerWindowID,
      requestID: requestID,
      target: aTarget
    };

    let glue = Cc["@mozilla.org/dom/apps/inter-app-comm-ui-glue;1"]
                 .createInstance(Ci.nsIInterAppCommUIGlue);
    if (glue) {
      glue.selectApps(callerID, pubAppManifestURL, keyword, appsToSelect).then(
        function(aData) {
          this._handleSelectedApps(aData);
        }.bind(this),
        function(aError) {
          if (DEBUG) {
            debug("Error occurred in the UI glue component. " + aError)
          }

          // Resolve the caller as if there were no selected apps.
          this._handleSelectedApps({ callerID: callerID,
                                     keyword: keyword,
                                     manifestURL: pubAppManifestURL,
                                     selectedApps: [] });
        }.bind(this)
      );
    } else {
      if (DEBUG) {
        debug("Error! The UI glue component is not implemented.")
      }

      // Resolve the caller as if there were no selected apps.
      this._handleSelectedApps({ callerID: callerID,
                                 keyword: keyword,
                                 manifestURL: pubAppManifestURL,
                                 selectedApps: [] });
    }
  },

  _getConnections: function(aMessage, aTarget) {
    let outerWindowID = aMessage.outerWindowID;
    let requestID = aMessage.requestID;

    let connections = [];
    for (let keyword in this._allowedConnections) {
      let allowedPubAppManifestURLs = this._allowedConnections[keyword];
      for (let allowedPubAppManifestURL in allowedPubAppManifestURLs) {
        let allowedSubAppManifestURLs =
          allowedPubAppManifestURLs[allowedPubAppManifestURL];
        allowedSubAppManifestURLs.forEach(function(allowedSubAppManifestURL) {
          connections.push({ keyword: keyword,
                             pubAppManifestURL: allowedPubAppManifestURL,
                             subAppManifestURL: allowedSubAppManifestURL });
        });
      }
    }

    aTarget.sendAsyncMessage("Webapps:GetConnections:Return:OK",
                             { connections: connections,
                               oid: outerWindowID, requestID: requestID });
  },

  _cancelConnection: function(aMessage) {
    let keyword = aMessage.keyword;
    let pubAppManifestURL = aMessage.pubAppManifestURL;
    let subAppManifestURL = aMessage.subAppManifestURL;

    let allowedPubAppManifestURLs = this._allowedConnections[keyword];
    if (!allowedPubAppManifestURLs) {
      if (DEBUG) debug("keyword is not found: " + keyword);
      return;
    }

    let allowedSubAppManifestURLs =
      allowedPubAppManifestURLs[pubAppManifestURL];
    if (!allowedSubAppManifestURLs) {
      if (DEBUG) debug("publisher is not found: " + pubAppManifestURL);
      return;
    }

    let index = allowedSubAppManifestURLs.indexOf(subAppManifestURL);
    if (index == -1) {
      if (DEBUG) debug("subscriber is not found: " + subAppManifestURL);
      return;
    }

    if (DEBUG) debug("Cancelling the connection.");
    allowedSubAppManifestURLs.splice(index, 1);

    // Clean up the parent entries if needed.
    if (allowedSubAppManifestURLs.length == 0) {
      delete allowedPubAppManifestURLs[pubAppManifestURL];
      if (Object.keys(allowedPubAppManifestURLs).length == 0) {
        delete this._allowedConnections[keyword];
      }
    }

    if (DEBUG) debug("Unregistering message ports based on this connection.");
    let messagePortIDs = [];
    for (let messagePortID in this._messagePortPairs) {
      let pair = this._messagePortPairs[messagePortID];
      if (pair.keyword == keyword &&
          pair.publisher.manifestURL == pubAppManifestURL &&
          pair.subscriber.manifestURL == subAppManifestURL) {
        messagePortIDs.push(messagePortID);
      }
    }
    messagePortIDs.forEach(function(aMessagePortID) {
      delete this._messagePortPairs[aMessagePortID];
    }, this);
  },

  _identifyMessagePort: function(aMessagePortID, aManifestURL) {
    let pair = this._messagePortPairs[aMessagePortID];
    if (!pair) {
      if (DEBUG) {
        debug("Error! The message port ID is invalid: " + aMessagePortID +
              ", which should have been generated by parent.");
      }
      return null;
    }

    // Check it the message port is for publisher.
    if (pair.publisher.manifestURL == aManifestURL) {
      return { pair: pair, isPublisher: true };
    }

    // Check it the message port is for subscriber.
    if (pair.subscriber.manifestURL == aManifestURL) {
      return { pair: pair, isPublisher: false };
    }

    if (DEBUG) {
      debug("Error! The manifest URL is invalid: " + aManifestURL +
            ", which might be a hacked app.");
    }
    return null;
  },

  _registerMessagePort: function(aMessage, aTarget) {
    let messagePortID = aMessage.messagePortID;
    let manifestURL = aMessage.manifestURL;
    let pageURL = aMessage.pageURL;

    let identity = this._identifyMessagePort(messagePortID, manifestURL);
    if (!identity) {
      if (DEBUG) {
        debug("Cannot identify the message port. Failed to register.");
      }
      return;
    }

    if (DEBUG) debug("Registering message port for " + manifestURL);
    let pair = identity.pair;
    let isPublisher = identity.isPublisher;

    let sender = isPublisher ? pair.publisher : pair.subscriber;
    sender.target = aTarget;
    sender.pageURL = pageURL;
    sender.messageQueue = [];

    // Check if the other port has queued messages. Deliver them if needed.
    if (DEBUG) {
      debug("Checking if the other port used to send messages but queued.");
    }
    let receiver = isPublisher ? pair.subscriber : pair.publisher;
    if (receiver.messageQueue) {
      while (receiver.messageQueue.length) {
        let message = receiver.messageQueue.shift();
        if (DEBUG) debug("Delivering message: " + JSON.stringify(message));
        sender.target.sendAsyncMessage("InterAppMessagePort:OnMessage",
                                       { message: message,
                                         manifestURL: sender.manifestURL,
                                         pageURL: sender.pageURL,
                                         messagePortID: messagePortID });
      }
    }
  },

  _unregisterMessagePort: function(aMessage) {
    let messagePortID = aMessage.messagePortID;
    let manifestURL = aMessage.manifestURL;

    let identity = this._identifyMessagePort(messagePortID, manifestURL);
    if (!identity) {
      if (DEBUG) {
        debug("Cannot identify the message port. Failed to unregister.");
      }
      return;
    }

    if (DEBUG) {
      debug("Unregistering message port for " + manifestURL);
    }
    delete this._messagePortPairs[messagePortID];
  },

  _removeTarget: function(aTarget) {
    if (!aTarget) {
      if (DEBUG) debug("Error! aTarget cannot be null/undefined in any way.");
      return
    }

    if (DEBUG) debug("Unregistering message ports based on this target.");
    let messagePortIDs = [];
    for (let messagePortID in this._messagePortPairs) {
      let pair = this._messagePortPairs[messagePortID];
      if (pair.publisher.target === aTarget ||
          pair.subscriber.target === aTarget) {
        messagePortIDs.push(messagePortID);
        // Send a shutdown message to the part of the pair that is still alive.
        let actor = pair.publisher.target === aTarget ? pair.subscriber
                                                       : pair.publisher;
        actor.target.sendAsyncMessage("InterAppMessagePort:Shutdown",
          { manifestURL: actor.manifestURL,
            pageURL: actor.pageURL,
            messagePortID: messagePortID });
      }
    }
    messagePortIDs.forEach(function(aMessagePortID) {
      delete this._messagePortPairs[aMessagePortID];
    }, this);
  },

  _postMessage: function(aMessage) {
    let messagePortID = aMessage.messagePortID;
    let manifestURL = aMessage.manifestURL;
    let message = aMessage.message;

    let identity = this._identifyMessagePort(messagePortID, manifestURL);
    if (!identity) {
      if (DEBUG) debug("Cannot identify the message port. Failed to post.");
      return;
    }

    let pair = identity.pair;
    let isPublisher = identity.isPublisher;

    let receiver = isPublisher ? pair.subscriber : pair.publisher;
    if (!receiver.target) {
      if (DEBUG) {
        debug("The receiver's target is not ready yet. Queuing the message.");
      }
      let sender = isPublisher ? pair.publisher : pair.subscriber;
      sender.messageQueue.push(message);
      return;
    }

    if (DEBUG) debug("Delivering message: " + JSON.stringify(message));
    receiver.target.sendAsyncMessage("InterAppMessagePort:OnMessage",
                                     { manifestURL: receiver.manifestURL,
                                       pageURL: receiver.pageURL,
                                       messagePortID: messagePortID,
                                       message: message });
  },

  _handleSelectedApps: function(aData) {
    let callerID = aData.callerID;
    let caller = this._promptUICallers[callerID];
    if (!caller) {
      if (DEBUG) debug("Error! Cannot find the caller.");
      return;
    }

    delete this._promptUICallers[callerID];

    let outerWindowID = caller.outerWindowID;
    let requestID = caller.requestID;
    let target = caller.target;

    let pubAppManifestURL = aData.manifestURL;
    let keyword = aData.keyword;
    let selectedApps = aData.selectedApps;

    let allowedSubAppManifestURLs;
    if (selectedApps.length == 0) {
      // Only do the connections for the existing allowed subscribers because
      // no new apps are selected to connect.
      if (DEBUG) debug("No new apps are selected to connect.")

      allowedSubAppManifestURLs =
        this._getAllowedSubAppManifestURLs(keyword, pubAppManifestURL);
    } else {
      // Do connections for for the existing allowed subscribers and the newly
      // selected subscribers.
      if (DEBUG) debug("Some new apps are selected to connect.")

      allowedSubAppManifestURLs =
        this._addSelectedApps(keyword, pubAppManifestURL, selectedApps);
    }

    // Finally, dispatch the message ports for the allowed connections,
    // including the old connections and the newly selected connection.
    this._dispatchMessagePorts(keyword, pubAppManifestURL,
                               allowedSubAppManifestURLs,
                               target, outerWindowID, requestID);
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("receiveMessage: name: " + aMessage.name);
    let message = aMessage.json;
    let target = aMessage.target;

    // To prevent the hacked child process from sending commands to parent
    // to do illegal connections, we need to check its manifest URL.
    if (aMessage.name !== "child-process-shutdown" &&
        // TODO: fix bug 988142 to re-enable "InterAppMessagePort:Unregister".
        aMessage.name !== "InterAppMessagePort:Unregister" &&
        kMessages.indexOf(aMessage.name) != -1) {
      if (!target.assertContainApp(message.manifestURL)) {
        if (DEBUG) {
          debug("Got message from a process carrying illegal manifest URL.");
        }
        return null;
      }
    }

    switch (aMessage.name) {
      case "Webapps:Connect":
        this._connect(message, target);
        break;
      case "Webapps:GetConnections":
        this._getConnections(message, target);
        break;
      case "InterAppConnection:Cancel":
        this._cancelConnection(message);
        break;
      case "InterAppMessagePort:PostMessage":
        this._postMessage(message);
        break;
      case "InterAppMessagePort:Register":
        this._registerMessagePort(message, target);
        break;
      case "InterAppMessagePort:Unregister":
        this._unregisterMessagePort(message);
        break;
      case "child-process-shutdown":
        this._removeTarget(target);
        break;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "webapps-clear-data");
        kMessages.forEach(function(aMsg) {
          ppmm.removeMessageListener(aMsg, this);
        }, this);
        ppmm = null;
        break;
      case "webapps-clear-data":
        let params =
          aSubject.QueryInterface(Ci.mozIApplicationClearPrivateDataParams);
        if (!params) {
          if (DEBUG) {
            debug("Error updating registered/allowed connections for an " +
                  "uninstalled app.");
          }
          return;
        }

        // Only update registered/allowed connections for apps.
        if (params.browserOnly) {
          if (DEBUG) {
            debug("Only update registered/allowed connections for apps.");
          }
          return;
        }

        let manifestURL = appsService.getManifestURLByLocalId(params.appId);
        if (!manifestURL) {
          if (DEBUG) {
            debug("Error updating registered/allowed connections for an " +
                  "uninstalled app.");
          }
          return;
        }

        // Update registered connections.
        for (let keyword in this._registeredConnections) {
          let subAppManifestURLs = this._registeredConnections[keyword];
          if (subAppManifestURLs[manifestURL]) {
            delete subAppManifestURLs[manifestURL];
            if (DEBUG) {
              debug("Remove " + manifestURL + " from registered connections " +
                    "due to app uninstallation.");
            }
          }
        }

        // Update allowed connections.
        for (let keyword in this._allowedConnections) {
          let allowedPubAppManifestURLs = this._allowedConnections[keyword];
          if (allowedPubAppManifestURLs[manifestURL]) {
            delete allowedPubAppManifestURLs[manifestURL];
            if (DEBUG) {
              debug("Remove " + manifestURL + " (as a pub app) from allowed " +
                    "connections due to app uninstallation.");
            }
          }

          for (let pubAppManifestURL in allowedPubAppManifestURLs) {
            let subAppManifestURLs = allowedPubAppManifestURLs[pubAppManifestURL];
            for (let i = subAppManifestURLs.length - 1; i >= 0; i--) {
              if (subAppManifestURLs[i] === manifestURL) {
                subAppManifestURLs.splice(i, 1);
                if (DEBUG) {
                  debug("Remove " + manifestURL + " (as a sub app to pub " +
                        pubAppManifestURL + ") from allowed connections " +
                        "due to app uninstallation.");
                }
              }
            }
          }
        }
        debug("Finish updating registered/allowed connections for an " +
              "uninstalled app.");
        break;
    }
  }
};

InterAppCommService.init();
