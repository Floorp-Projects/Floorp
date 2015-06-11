/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

XPCOMUtils.defineLazyServiceGetter(this, "aceService",
                                   "@mozilla.org/secureelement/access-control/ace;1",
                                   "nsIAccessControlEnforcer");

XPCOMUtils.defineLazyModuleGetter(this, "SEUtils",
                                  "resource://gre/modules/SEUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "SE", () => {
  let obj = {};
  Cu.import("resource://gre/modules/se_consts.js", obj);
  return obj;
});

let DEBUG = SE.DEBUG_SE;
function debug(aMsg) {
  if (DEBUG) {
    dump("-*- HCIEventTransaction: " + aMsg);
  }
}

/**
  * nsISystemMessagesConfigurator implementation.
  */
function HCIEventTransactionSystemMessageConfigurator() {
  debug("HCIEventTransactionSystemMessageConfigurator");
}

HCIEventTransactionSystemMessageConfigurator.prototype = {
  get mustShowRunningApp() {
    debug("mustShowRunningApp returning true");
    return true;
  },

  shouldDispatch: function shouldDispatch(aManifestURL, aPageURL, aType, aMessage, aExtra) {
    DEBUG && debug("message to dispatch: " + JSON.stringify(aMessage));
    debug("aManifest url: " + aManifestURL);

    if (!aMessage) {
      return Promise.resolve(false);
    }

    let appId = appsService.getAppLocalIdByManifestURL(aManifestURL);
    if (appId === Ci.nsIScriptSecurityManager.NO_APP_ID) {
      return Promise.resolve(false);
    }

    return new Promise((resolve, reject) => {
      appsService.getManifestFor(aManifestURL)
      .then((aManifest) => this._checkAppManifest(aMessage.origin, aMessage.aid, aManifest))
      .then(() => aceService.isAccessAllowed(appId, aMessage.origin, aMessage.aid))
      .then((allowed) => {
        debug("dispatching message: " + allowed);
        resolve(allowed);
      })
      .catch(() => {
        debug("not dispatching");
        resolve(false);
      });
    });
  },

  _checkAppManifest: function _checkAppManifest(aOrigin, aAid, aManifest) {
    DEBUG && debug("aManifest " + JSON.stringify(aManifest));

    // convert AID and Secure Element name to uppercased string for comparison
    // with manifest secure_element_access rules
    let aid = SEUtils.byteArrayToHexString(aAid);
    let seName = (aOrigin) ? aOrigin.toUpperCase() : "";

    let hciRules = aManifest["secure_element_access"] || [];
    let matchingRule = hciRules.find((rule) => {
      rule = rule.toUpperCase();
      if(rule === "*" || rule === (seName + "/" + aid)) {
        return true;
      }

      let isMatching = (match, element) => {
        if(match === "*") {
          return true;
        }
        if(match.charAt(match.length - 1) === '*') {
          return element.indexOf(match.substr(0,match.length - 1)) === 0;
        }

        return match === element;
      };

      return isMatching(rule.split('/')[0], seName) &&
             isMatching(rule.split('/')[1], aid);
    });

    return (matchingRule) ? Promise.resolve() : Promise.reject();
  },

  classID: Components.ID("{b501edd0-28bd-11e4-8c21-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesConfigurator])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HCIEventTransactionSystemMessageConfigurator]);
