/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

function debug(aMsg) {
  // dump("-- InterAppCommService: " + Date.now() + ": " + aMsg + "\n");
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

const kMessages =["Webapps:Connect"];

function InterAppCommService() {
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  Services.obs.addObserver(this, "inter-app-comm-select-app-result", false);

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
  //       appStatus: Ci.nsIPrincipal.APP_STATUS_CERTIFIED,
  //       rules: { ... }
  //     },
  //     "app://subApp2.gaiamobile.org/manifest.webapp": {
  //       pageURL: "app://subApp2.gaiamobile.org/handler.html",
  //       description: "blah blah",
  //       appStatus: Ci.nsIPrincipal.APP_STATUS_PRIVILEGED,
  //       rules: { ... }
  //     }
  //   },
  //   "bar": {
  //     "app://subApp3.gaiamobile.org/manifest.webapp": {
  //       pageURL: "app://subApp3.gaiamobile.org/handler.html",
  //       description: "blah blah",
  //       appStatus: Ci.nsIPrincipal.APP_STATUS_INSTALLED,
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
}

InterAppCommService.prototype = {
  registerConnection: function(aKeyword, aHandlerPageURI, aManifestURI,
                               aDescription, aAppStatus, aRules) {
    let manifestURL = aManifestURI.spec;
    let pageURL = aHandlerPageURI.spec;

    debug("registerConnection: aKeyword: " + aKeyword +
          " manifestURL: " + manifestURL + " pageURL: " + pageURL +
          " aDescription: " + aDescription + " aAppStatus: " + aAppStatus +
          " aRules.minimumAccessLevel: " + aRules.minimumAccessLevel +
          " aRules.manifestURLs: " + aRules.manifestURLs +
          " aRules.installOrigins: " + aRules.installOrigins);

    let subAppManifestURLs = this._registeredConnections[aKeyword];
    if (!subAppManifestURLs) {
      subAppManifestURLs = this._registeredConnections[aKeyword] = {};
    }

    subAppManifestURLs[manifestURL] = {
      pageURL: pageURL,
      description: aDescription,
      appStatus: aAppStatus,
      rules: aRules,
      manifestURL: manifestURL
    };
  },

  _matchMinimumAccessLevel: function(aRules, aAppStatus) {
    if (!aRules || !aRules.minimumAccessLevel) {
      debug("rules.minimumAccessLevel is not available. No need to match.");
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

    debug("rules.minimumAccessLevel is not matched! " +
          "minAccessLevel: " + minAccessLevel + " aAppStatus : " + aAppStatus);
    return false;
  },

  _matchManifestURLs: function(aRules, aManifestURL) {
    if (!aRules || !Array.isArray(aRules.manifestURLs)) {
      debug("rules.manifestURLs is not available. No need to match.");
      return true;
    }

    let manifestURLs = aRules.manifestURLs;
    if (manifestURLs.indexOf(aManifestURL) != -1) {
      return true;
    }

    debug("rules.manifestURLs is not matched! " +
          "manifestURLs: " + manifestURLs + " aManifestURL : " + aManifestURL);
    return false;
  },

  _matchInstallOrigins: function(aRules, aManifestURL) {
    if (!aRules || !Array.isArray(aRules.installOrigins)) {
      debug("rules.installOrigins is not available. No need to match.");
      return true;
    }

    let installOrigin =
      appsService.getAppByManifestURL(aManifestURL).installOrigin;

    let installOrigins = aRules.installOrigins;
    if (installOrigins.indexOf(installOrigin) != -1) {
      return true;
    }

    debug("rules.installOrigins is not matched! aManifestURL: " + aManifestURL +
          " installOrigins: " + installOrigins + " installOrigin : " + installOrigin);
    return false;
  },

  _matchRules: function(aPubAppManifestURL, aPubAppStatus, aPubRules,
                        aSubAppManifestURL, aSubAppStatus, aSubRules) {
    // TODO Bug 907068 In the initiative step, we only expose this API to
    // certified apps to meet the time line. Eventually, we need to make
    // it available for the non-certified apps as well. For now, only the
    // certified apps can match the rules.
    if (aPubAppStatus != Ci.nsIPrincipal.APP_STATUS_CERTIFIED ||
        aSubAppStatus != Ci.nsIPrincipal.APP_STATUS_CERTIFIED) {
      debug("Only certified apps are allowed to do connections.");
      return false;
    }

    if (!aPubRules && !aSubRules) {
      debug("Rules of publisher and subscriber are absent. No need to match.");
      return true;
    }

    // Check minimumAccessLevel.
    if (!this._matchMinimumAccessLevel(aPubRules, aSubAppStatus) ||
        !this._matchMinimumAccessLevel(aSubRules, aPubAppStatus)) {
      return false;
    }

    // Check manifestURLs.
    if (!this._matchManifestURLs(aPubRules, aSubAppManifestURL) ||
        !this._matchManifestURLs(aSubRules, aPubAppManifestURL)) {
      return false;
    }

    // Check installOrigins.
    if (!this._matchInstallOrigins(aPubRules, aSubAppManifestURL) ||
        !this._matchInstallOrigins(aSubRules, aPubAppManifestURL)) {
      return false;
    }

    // Check developers.
    // TODO Do we really want to check this? This one seems naive.

    debug("All rules are matched.");
    return true;
  },

  _dispatchMessagePorts: function(aKeyword, aAllowedSubAppManifestURLs,
                                  aTarget, aOuterWindowID, aRequestID) {
    debug("_dispatchMessagePorts: aKeyword: " + aKeyword +
          " aAllowedSubAppManifestURLs: " + aAllowedSubAppManifestURLs);

    if (aAllowedSubAppManifestURLs.length == 0) {
      debug("No apps are allowed to connect. Returning.");
      aTarget.sendAsyncMessage("Webapps:Connect:Return:KO",
                               { oid: aOuterWindowID, requestID: aRequestID });
      return;
    }

    let subAppManifestURLs = this._registeredConnections[aKeyword];
    if (!subAppManifestURLs) {
      debug("No apps are subscribed to connect. Returning.");
      aTarget.sendAsyncMessage("Webapps:Connect:Return:KO",
                               { oid: aOuterWindowID, requestID: aRequestID });
      return;
    }

    let messagePortIDs = [];
    aAllowedSubAppManifestURLs.forEach(function(aAllowedSubAppManifestURL) {
      let subscribedInfo = subAppManifestURLs[aAllowedSubAppManifestURL];
      if (!subscribedInfo) {
        debug("The sunscribed info is not available. Skipping: " +
               aAllowedSubAppManifestURL);
        return;
      }

      let messagePortID = UUIDGenerator.generateUUID().toString();

      // Fire system message to deliver the message port to the subscriber.
      messenger.sendMessage("connection",
        { keyword: aKeyword,
          messagePortID: messagePortID },
        Services.io.newURI(subscribedInfo.pageURL, null, null),
        Services.io.newURI(subscribedInfo.manifestURL, null, null));

      messagePortIDs.push(messagePortID);
    });

    if (messagePortIDs.length == 0) {
      debug("No apps are subscribed to connect. Returning.");
      aTarget.sendAsyncMessage("Webapps:Connect:Return:KO",
                               { oid: aOuterWindowID, requestID: aRequestID });
      return;
    }

    // Return the message port IDs to open the message ports for the publisher.
    debug("messagePortIDs: " + messagePortIDs);
    aTarget.sendAsyncMessage("Webapps:Connect:Return:OK",
                             { keyword: aKeyword,
                               messagePortIDs: messagePortIDs,
                               oid: aOuterWindowID, requestID: aRequestID });
  },

  _connect: function(aMessage, aTarget) {
    let keyword = aMessage.keyword;
    let pubRules = aMessage.rules;
    let pubAppManifestURL = aMessage.manifestURL;
    let outerWindowID = aMessage.outerWindowID;
    let requestID = aMessage.requestID;
    let pubAppStatus = aMessage.appStatus;

    let subAppManifestURLs = this._registeredConnections[keyword];
    if (!subAppManifestURLs) {
      debug("No apps are subscribed for this connection. Returning.")
      this._dispatchMessagePorts(keyword, [], aTarget, outerWindowID, requestID);
      return;
    }

    // Fetch the apps that used to be allowed to connect before, so that
    // users don't need to select/allow them again. That is, we only pop up
    // the prompt UI for the *new* connections.
    let allowedSubAppManifestURLs = [];
    let allowedPubAppManifestURLs = this._allowedConnections[keyword];
    if (allowedPubAppManifestURLs &&
        allowedPubAppManifestURLs[pubAppManifestURL]) {
      allowedSubAppManifestURLs = allowedPubAppManifestURLs[pubAppManifestURL];
    }

    // Check rules to see if a subscribed app is allowed to connect.
    let appsToSelect = [];
    for (let subAppManifestURL in subAppManifestURLs) {
      if (allowedSubAppManifestURLs.indexOf(subAppManifestURL) != -1) {
        debug("Don't need to select again. Skipping: " + subAppManifestURL);
        continue;
      }

      // Only rule-matched publishers/subscribers are allowed to connect.
      let subscribedInfo = subAppManifestURLs[subAppManifestURL];
      let subAppStatus = subscribedInfo.appStatus;
      let subRules = subscribedInfo.rules;

      let matched =
        this._matchRules(pubAppManifestURL, pubAppStatus, pubRules,
                         subAppManifestURL, subAppStatus, subRules);
      if (!matched) {
        debug("Rules are not matched. Skipping: " + subAppManifestURL);
        continue;
      }

      appsToSelect.push({
        manifestURL: subAppManifestURL,
        description: subscribedInfo.description
      });
    }

    if (appsToSelect.length == 0) {
      debug("No additional apps need to be selected for this connection. " +
            "Just dispatch message ports for the existing connections.");
      this._dispatchMessagePorts(keyword, allowedSubAppManifestURLs,
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

    // TODO Bug 897169 Temporarily disable the notification for popping up
    // the prompt until the UX/UI for the prompt is confirmed.
    //
    // TODO Bug 908191 We need to change the way of interaction between API and
    // run-time prompt from observer notification to xpcom-interface caller.
    //
    /*
    debug("appsToSelect: " + appsToSelect);
    Services.obs.notifyObservers(null, "inter-app-comm-select-app",
      JSON.stringify({ callerID: callerID,
                       manifestURL: pubAppManifestURL,
                       keyword: keyword,
                       appsToSelect: appsToSelect }));
    */

    // TODO Bug 897169 Simulate the return of the app-selected result by
    // the prompt, which always allows the connection. This dummy codes
    // will be removed when the UX/UI for the prompt is ready.
    debug("appsToSelect: " + appsToSelect);
    Services.obs.notifyObservers(null, 'inter-app-comm-select-app-result',
      JSON.stringify({ callerID: callerID,
                       manifestURL: pubAppManifestURL,
                       keyword: keyword,
                       selectedApps: appsToSelect }));
  },

  _handleSelectcedApps: function(aData) {
    let callerID = aData.callerID;
    let caller = this._promptUICallers[callerID];
    if (!caller) {
      debug("Error! Cannot find the caller.");
      return;
    }

    delete this._promptUICallers[callerID];

    let outerWindowID = caller.outerWindowID;
    let requestID = caller.requestID;
    let target = caller.target;

    let manifestURL = aData.manifestURL;
    let keyword = aData.keyword;
    let selectedApps = aData.selectedApps;

    if (selectedApps.length == 0) {
      debug("No apps are selected to connect.")
      this._dispatchMessagePorts(keyword, [], target, outerWindowID, requestID);
      return;
    }

    // Find the entry of allowed connections to add the selected apps.
    let allowedPubAppManifestURLs = this._allowedConnections[keyword];
    if (!allowedPubAppManifestURLs) {
      allowedPubAppManifestURLs = this._allowedConnections[keyword] = {};
    }
    let allowedSubAppManifestURLs = allowedPubAppManifestURLs[manifestURL];
    if (!allowedSubAppManifestURLs) {
      allowedSubAppManifestURLs = allowedPubAppManifestURLs[manifestURL] = [];
    }

    // Add the selected app into the existing set of allowed connections.
    selectedApps.forEach(function(aSelectedApp) {
      let allowedSubAppManifestURL = aSelectedApp.manifestURL;
      if (allowedSubAppManifestURLs.indexOf(allowedSubAppManifestURL) == -1) {
        allowedSubAppManifestURLs.push(allowedSubAppManifestURL);
      }
    });

    // Finally, dispatch the message ports for the allowed connections,
    // including the old connections and the newly selected connection.
    this._dispatchMessagePorts(keyword, allowedSubAppManifestURLs,
                               target, outerWindowID, requestID);
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage: name: " + aMessage.name);
    let message = aMessage.json;
    let target = aMessage.target;

    // To prevent the hacked child process from sending commands to parent
    // to do illegal connections, we need to check its manifest URL.
    if (kMessages.indexOf(aMessage.name) != -1) {
      if (!target.assertContainApp(message.manifestURL)) {
        debug("Got message from a child process carrying illegal manifest URL.");
        return null;
      }
    }

    switch (aMessage.name) {
      case "Webapps:Connect":
        this._connect(message, target);
        break;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "inter-app-comm-select-app-result");
        kMessages.forEach(function(aMsg) {
          ppmm.removeMessageListener(aMsg, this);
        }, this);
        ppmm = null;
        break;
      case "inter-app-comm-select-app-result":
        debug("inter-app-comm-select-app-result: " + aData);
        this._handleSelectcedApps(JSON.parse(aData));
        break;
    }
  },

  classID: Components.ID("{3dd15ce6-e7be-11e2-82bc-77967e7a63e6}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIInterAppCommService,
                                         Ci.nsIObserver])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([InterAppCommService]);
