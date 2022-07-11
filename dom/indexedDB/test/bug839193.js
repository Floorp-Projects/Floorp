/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIQuotaManagerService = Ci.nsIQuotaManagerService;

var gURI = Services.io.newURI("http://localhost");

function onUsageCallback(request) {}

function onLoad() {
  var quotaManagerService = Cc[
    "@mozilla.org/dom/quota-manager-service;1"
  ].getService(nsIQuotaManagerService);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    gURI,
    {}
  );
  var quotaRequest = quotaManagerService.getUsageForPrincipal(
    principal,
    onUsageCallback
  );
  quotaRequest.cancel();
  Services.obs.notifyObservers(window, "bug839193-loaded");
}

function onUnload() {
  Services.obs.notifyObservers(window, "bug839193-unloaded");
}
