/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIQuotaManagerService = Components.interfaces.nsIQuotaManagerService;

var gURI = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService).newURI("http://localhost");

function onUsageCallback(request) {}

function onLoad()
{
  var quotaManagerService =
    Components.classes["@mozilla.org/dom/quota-manager-service;1"]
              .getService(nsIQuotaManagerService);
  let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                            .getService(Components.interfaces.nsIScriptSecurityManager)
                            .createCodebasePrincipal(gURI, {});
  var quotaRequest = quotaManagerService.getUsageForPrincipal(principal,
                                                              onUsageCallback);
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
