/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Query function for determining if any IP address is available for
 * generating SDP.
 *
 * @return false if required additional network setup.
 */
function isNetworkReady() {
  // for gonk platform
  if ("nsINetworkInterfaceListService" in SpecialPowers.Ci) {
    var listService = SpecialPowers.Cc["@mozilla.org/network/interface-list-service;1"]
                        .getService(SpecialPowers.Ci.nsINetworkInterfaceListService);
    var itfList = listService.getDataInterfaceList(
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_MMS_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_SUPL_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_IMS_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_DUN_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_FOTA_INTERFACES);
    var num = itfList.getNumberOfInterface();
    for (var i = 0; i < num; i++) {
      var ips = {};
      var prefixLengths = {};
      var length = itfList.getInterface(i).getAddresses(ips, prefixLengths);

      for (var j = 0; j < length; j++) {
        var ip = ips.value[j];
        // skip IPv6 address until bug 797262 is implemented
        if (ip.indexOf(":") < 0) {
          info("Network interface is ready with address: " + ip);
          return true;
        }
      }
    }
    // ip address is not available
    info("Network interface is not ready, required additional network setup");
    return false;
  }
  info("Network setup is not required");
  return true;
}

/**
 * Network setup utils for Gonk
 *
 * @return {object} providing functions for setup/teardown data connection
 */
function getNetworkUtils() {
  var url = SimpleTest.getTestFileURL("NetworkPreparationChromeScript.js");
  var script = SpecialPowers.loadChromeScript(url);

  var utils = {
    /**
     * Utility for setting up data connection.
     *
     * @param aCallback callback after data connection is ready.
     */
    prepareNetwork: function() {
      return new Promise(resolve => {
        script.addMessageListener('network-ready', () =>  {
          info("Network interface is ready");
          resolve();
        });
        info("Setting up network interface");
        script.sendAsyncMessage("prepare-network", true);
      });
    },
    /**
     * Utility for tearing down data connection.
     *
     * @param aCallback callback after data connection is closed.
     */
    tearDownNetwork: function() {
      if (!isNetworkReady()) {
        info("No network to tear down");
        return Promise.resolve();
      }
      return new Promise(resolve => {
        script.addMessageListener('network-disabled', message => {
          info("Network interface torn down");
          script.destroy();
          resolve();
        });
        info("Tearing down network interface");
        script.sendAsyncMessage("network-cleanup", true);
      });
    }
  };

  return utils;
}

/**
 * Setup network on Gonk if needed and execute test once network is up
 *
 */
function startNetworkAndTest() {
  if (isNetworkReady()) {
    return Promise.resolve();
  }
  var utils = getNetworkUtils();
  // Trigger network setup to obtain IP address before creating any PeerConnection.
  return utils.prepareNetwork();
}

/**
 * A wrapper around SimpleTest.finish() to handle B2G network teardown
 */
function networkTestFinished() {
  var p;
  if ("nsINetworkInterfaceListService" in SpecialPowers.Ci) {
    var utils = getNetworkUtils();
    p = utils.tearDownNetwork();
  } else {
    p = Promise.resolve();
  }
  return p.then(() => finish());
}
