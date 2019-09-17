/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["OfflineAppsChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);

class OfflineAppsChild extends ActorChild {
  constructor(dispatcher) {
    super(dispatcher);

    this._docId = 0;
    this._docIdMap = new Map();

    this._docManifestSet = new Set();
  }

  registerWindow(aWindow) {
    let manifestURI = this._getManifestURI(aWindow);
    this._docManifestSet.add(manifestURI.spec);
  }

  handleEvent(event) {
    if (event.type == "MozApplicationManifest") {
      this.registerWindow(event.originalTarget.defaultView);
    }
  }

  _getManifestURI(aWindow) {
    if (!aWindow.document.documentElement) {
      return null;
    }

    var attr = aWindow.document.documentElement.getAttribute("manifest");
    if (!attr) {
      return null;
    }

    try {
      return Services.io.newURI(
        attr,
        aWindow.document.characterSet,
        Services.io.newURI(aWindow.location.href)
      );
    } catch (e) {
      return null;
    }
  }

  _startFetching(aDocument) {
    if (!aDocument.documentElement) {
      return;
    }

    let manifestURI = this._getManifestURI(aDocument.defaultView);
    if (!manifestURI) {
      return;
    }

    var updateService = Cc[
      "@mozilla.org/offlinecacheupdate-service;1"
    ].getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(
      manifestURI,
      aDocument.documentURIObject,
      aDocument.nodePrincipal,
      aDocument.defaultView
    );
  }

  receiveMessage(aMessage) {
    if (aMessage.name == "OfflineApps:StartFetching") {
      let doc = this._docIdMap.get(aMessage.data.docId);
      doc = doc && doc.get();
      if (doc) {
        this._startFetching(doc);
      }
      this._docIdMap.delete(aMessage.data.docId);
    }
  }
}

OfflineAppsChild.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIObserver,
  Ci.nsISupportsWeakReference,
]);
