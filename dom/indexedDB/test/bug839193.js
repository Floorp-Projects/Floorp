/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIQuotaManager = Components.interfaces.nsIQuotaManager;

let gURI = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService).newURI("http://localhost", null, null);

function onUsageCallback(uri, usage, fileUsage) {}

function onLoad()
{
  var quotaManager = Components.classes["@mozilla.org/dom/quota/manager;1"]
                               .getService(nsIQuotaManager);
  var quotaRequest = quotaManager.getUsageForURI(gURI, onUsageCallback);
  quotaRequest.cancel();
  Components.classes["@mozilla.org/observer-service;1"]
            .getService(Components.interfaces.nsIObserverService)
            .notifyObservers(window, "bug839193-loaded", null);
}

function onUnload()
{
  Components.classes["@mozilla.org/observer-service;1"]
            .getService(Components.interfaces.nsIObserverService)
            .notifyObservers(window, "bug839193-unloaded", null);
}
