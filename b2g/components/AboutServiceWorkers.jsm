/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

this.EXPORTED_SYMBOLS = [];

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gServiceWorkerManager",
                                  "@mozilla.org/serviceworkers/manager;1",
                                  "nsIServiceWorkerManager");

function debug(aMsg) {
  //dump("AboutServiceWorkers - " + aMsg + "\n");
}

function serializeServiceWorkerInfo(aServiceWorkerInfo) {
  if (!aServiceWorkerInfo) {
    throw new Error("Invalid service worker information");
  }

  let result = {};

  Object.keys(aServiceWorkerInfo).forEach(property => {
    if (typeof aServiceWorkerInfo[property] == "function") {
      return;
    }
    if (property === "principal") {
      result.principal = {
        origin: aServiceWorkerInfo.principal.origin,
        appId: aServiceWorkerInfo.principal.appId,
        isInBrowser: aServiceWorkerInfo.principal.isInBrowser
      };
      return;
    }
    result[property] = aServiceWorkerInfo[property];
  });

  return result;
}

function sendResult(aId, aResult) {
  SystemAppProxy._sendCustomEvent("mozAboutServiceWorkersChromeEvent", {
    id: aId,
    result: aResult
  });
}

function sendError(aId, aError) {
  SystemAppProxy._sendCustomEvent("mozAboutServiceWorkersChromeEvent", {
    id: aId,
    error: aError
  });
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

  handleEvent: function(aEvent) {
    let message = aEvent.detail;

    debug("Got content event " + JSON.stringify(message));

    if (!message.id || !message.name) {
      dump("Invalid event " + JSON.stringify(message) + "\n");
      return;
    }

    switch(message.name) {
      case "init":
        if (!this.enabled) {
          sendResult({
            enabled: false,
            registrations: []
          });
          return;
        };

        let data = gServiceWorkerManager.getAllRegistrations();
        if (!data) {
          sendError(message.id, "NoServiceWorkersRegistrations");
          return;
        }

        let registrations = [];

        for (let i = 0; i < data.length; i++) {
          let info = data.queryElementAt(i, Ci.nsIServiceWorkerInfo);
          if (!info) {
            dump("AboutServiceWorkers: Invalid nsIServiceWorkerInfo " +
                 "interface.\n");
            continue;
          }
          registrations.push(serializeServiceWorkerInfo(info));
        }

        sendResult(message.id, {
          enabled: this.enabled,
          registrations: registrations
        });
        break;

      case "update":
        if (!message.scope) {
          sendError(message.id, "MissingScope");
          return;
        }
        gServiceWorkerManager.update(message.scope);
        sendResult(message.id, true);
        break;

      case "unregister":
        if (!message.principal ||
            !message.principal.origin ||
            !message.principal.appId) {
          sendError("MissingPrincipal");
          return;
        }

        let principal = Services.scriptSecurityManager.getAppCodebasePrincipal(
          Services.io.newURI(message.principal.origin, null, null),
          message.principal.appId,
          message.principal.isInBrowser
        );

        if (!message.scope) {
          sendError("MissingScope");
          return;
        }

        let serviceWorkerUnregisterCallback = {
          unregisterSucceeded: function() {
            sendResult(message.id, true);
          },

          unregisterFailed: function() {
            sendError(message.id, "UnregisterError");
          },

          QueryInterface: XPCOMUtils.generateQI([
            Ci.nsIServiceWorkerUnregisterCallback
          ])
        };
        gServiceWorkerManager.unregister(principal,
                                         serviceWorkerUnregisterCallback,
                                         message.scope);
        break;
    }
  }
};

AboutServiceWorkers.init();
