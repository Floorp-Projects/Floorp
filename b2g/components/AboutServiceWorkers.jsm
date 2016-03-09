/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

this.EXPORTED_SYMBOLS = ["AboutServiceWorkers"];

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gServiceWorkerManager",
                                  "@mozilla.org/serviceworkers/manager;1",
                                  "nsIServiceWorkerManager");

function debug(aMsg) {
  dump("AboutServiceWorkers - " + aMsg + "\n");
}

function serializeServiceWorkerInfo(aServiceWorkerInfo) {
  if (!aServiceWorkerInfo) {
    throw new Error("Invalid service worker information");
  }

  let result = {};

  result.principal = {
    origin: aServiceWorkerInfo.principal.originNoSuffix,
    originAttributes: aServiceWorkerInfo.principal.originAttributes
  };

  ["scope", "scriptSpec"].forEach(property => {
    result[property] = aServiceWorkerInfo[property];
  });

  return result;
}


this.AboutServiceWorkers = {
  get enabled() {
    if (this._enabled) {
      return this._enabled;
    }
    this._enabled = false;
    try {
      this._enabled = Services.prefs.getBoolPref("dom.serviceWorkers.enabled");
    } catch(e) {}
    return this._enabled;
  },

  init: function() {
    SystemAppProxy.addEventListener("mozAboutServiceWorkersContentEvent",
                                    AboutServiceWorkers);
  },

  sendResult: function(aId, aResult) {
    SystemAppProxy._sendCustomEvent("mozAboutServiceWorkersChromeEvent", {
      id: aId,
      result: aResult
    });
  },

  sendError: function(aId, aError) {
    SystemAppProxy._sendCustomEvent("mozAboutServiceWorkersChromeEvent", {
      id: aId,
      error: aError
    });
  },

  handleEvent: function(aEvent) {
    let message = aEvent.detail;

    debug("Got content event " + JSON.stringify(message));

    if (!message.id || !message.name) {
      dump("Invalid event " + JSON.stringify(message) + "\n");
      return;
    }

    let self = AboutServiceWorkers;

    switch(message.name) {
      case "init":
        if (!self.enabled) {
          self.sendResult(message.id, {
            enabled: false,
            registrations: []
          });
          return;
        };

        let data = gServiceWorkerManager.getAllRegistrations();
        if (!data) {
          self.sendError(message.id, "NoServiceWorkersRegistrations");
          return;
        }

        let registrations = [];

        for (let i = 0; i < data.length; i++) {
          let info = data.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
          if (!info) {
            dump("AboutServiceWorkers: Invalid nsIServiceWorkerRegistrationInfo " +
                 "interface.\n");
            continue;
          }
          registrations.push(serializeServiceWorkerInfo(info));
        }

        self.sendResult(message.id, {
          enabled: self.enabled,
          registrations: registrations
        });
        break;

      case "update":
        if (!message.scope) {
          self.sendError(message.id, "MissingScope");
          return;
        }

        if (!message.principal ||
            !message.principal.originAttributes) {
          self.sendError(message.id, "MissingOriginAttributes");
          return;
        }

        gServiceWorkerManager.propagateSoftUpdate(
          message.principal.originAttributes,
          message.scope
        );

        self.sendResult(message.id, true);
        break;

      case "unregister":
        if (!message.principal ||
            !message.principal.origin ||
            !message.principal.originAttributes ||
            !message.principal.originAttributes.appId ||
            (message.principal.originAttributes.inIsolatedMozBrowser == null)) {
          self.sendError(message.id, "MissingPrincipal");
          return;
        }

        let principal = Services.scriptSecurityManager.createCodebasePrincipal(
          // TODO: Bug 1196652. use originNoSuffix
          Services.io.newURI(message.principal.origin, null, null),
          message.principal.originAttributes);

        if (!message.scope) {
          self.sendError(message.id, "MissingScope");
          return;
        }

        let serviceWorkerUnregisterCallback = {
          unregisterSucceeded: function() {
            self.sendResult(message.id, true);
          },

          unregisterFailed: function() {
            self.sendError(message.id, "UnregisterError");
          },

          QueryInterface: XPCOMUtils.generateQI([
            Ci.nsIServiceWorkerUnregisterCallback
          ])
        };
        gServiceWorkerManager.propagateUnregister(principal,
                                                  serviceWorkerUnregisterCallback,
                                                  message.scope);
        break;
    }
  }
};

AboutServiceWorkers.init();
