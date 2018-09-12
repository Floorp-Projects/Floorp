/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["OfflineAppsChild"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

class OfflineAppsChild extends ActorChild {
  constructor(mm) {
    super(mm);

    this._docId = 0;
    this._docIdMap = new Map();

    this._docManifestSet = new Set();

    this._observerAdded = false;
  }

  registerWindow(aWindow) {
    if (!this._observerAdded) {
      this._observerAdded = true;
      Services.obs.addObserver(this, "offline-cache-update-completed", true);
    }
    let manifestURI = this._getManifestURI(aWindow);
    this._docManifestSet.add(manifestURI.spec);
  }

  handleEvent(event) {
    if (event.type == "MozApplicationManifest") {
      this.offlineAppRequested(event.originalTarget.defaultView);
    }
  }

  _getManifestURI(aWindow) {
    if (!aWindow.document.documentElement)
      return null;

    var attr = aWindow.document.documentElement.getAttribute("manifest");
    if (!attr)
      return null;

    try {
      return Services.io.newURI(attr, aWindow.document.characterSet,
                                Services.io.newURI(aWindow.location.href));
    } catch (e) {
      return null;
    }
  }

  offlineAppRequested(aContentWindow) {
    this.registerWindow(aContentWindow);
    if (!Services.prefs.getBoolPref("browser.offline-apps.notify")) {
      return;
    }

    let currentURI = aContentWindow.document.documentURIObject;
    // don't bother showing UI if the user has already made a decision
    if (Services.perms.testExactPermission(currentURI, "offline-app") != Services.perms.UNKNOWN_ACTION)
      return;

    try {
      if (Services.prefs.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch (e) {
      // this pref isn't set by default, ignore failures
    }
    let docId = ++this._docId;
    this._docIdMap.set(docId, Cu.getWeakReference(aContentWindow.document));
    this.mm.sendAsyncMessage("OfflineApps:RequestPermission", {
      uri: currentURI.spec,
      docId,
    });
  }

  _startFetching(aDocument) {
    if (!aDocument.documentElement)
      return;

    let manifestURI = this._getManifestURI(aDocument.defaultView);
    if (!manifestURI)
      return;

    var updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].
                        getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject,
                                 aDocument.nodePrincipal, aDocument.defaultView);
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

  observe(aSubject, aTopic, aState) {
    if (aTopic == "offline-cache-update-completed") {
      let cacheUpdate = aSubject.QueryInterface(Ci.nsIOfflineCacheUpdate);
      let uri = cacheUpdate.manifestURI;
      if (uri && this._docManifestSet.has(uri.spec)) {
        this.mm.sendAsyncMessage("OfflineApps:CheckUsage", {uri: uri.spec});
      }
    }
  }
}

OfflineAppsChild.prototype.QueryInterface =
  ChromeUtils.generateQI([Ci.nsIObserver,
                          Ci.nsISupportsWeakReference]);
