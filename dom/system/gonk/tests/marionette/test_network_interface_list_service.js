/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function getNetworkInterface(aType) {
  let networkListService =
    Cc["@mozilla.org/network/interface-list-service;1"].
      getService(Ci.nsINetworkInterfaceListService);
  // Get all available interfaces
  let networkList = networkListService.getDataInterfaceList(0);

  // Try to get nsINetworkInterface for aType.
  let numberOfInterface = networkList.getNumberOfInterface();
  for (let i = 0; i < numberOfInterface; i++) {
    let iface = networkList.getInterface(i);
    if (iface.type === aType) {
      return iface;
    }
  }

  return null;
}

// Test getDataInterfaceList by enabling/disabling mobile data.
function testGetDataInterfaceList(aMobileDataEnabled) {
  log("Test getDataInterfaceList with mobile data " +
      aMobileDataEnabled ? "enabled" : "disabled");

  return setDataEnabledAndWait(aMobileDataEnabled)
    .then(() => getNetworkInterface(NETWORK_TYPE_MOBILE))
    .then((networkInterface) => {
      if (!networkInterface) {
        ok(false, "Should get an valid nsINetworkInterface for mobile");
        return;
      }

      ok(networkInterface instanceof Ci.nsINetworkInterface,
         "networkInterface should be an instance of nsINetworkInterface");

      let ipAddresses = {};
      let prefixs = {};
      let numOfGateways = {};
      let numOfDnses = {};
      let numOfIpAddresses = networkInterface.getAddresses(ipAddresses, prefixs);
      let gateways = networkInterface.getGateways(numOfGateways);
      let dnses = networkInterface.getDnses(numOfDnses);

      if (aMobileDataEnabled) {
        // Mobile data is enabled.
        is(networkInterface.state, NETWORK_STATE_CONNECTED, "check state");
        ok(numOfIpAddresses > 0, "check number of ipAddresses");
        ok(ipAddresses.value.length > 0, "check ipAddresses.length");
        ok(prefixs.value.length > 0, "check prefixs.length");
        ok(numOfGateways.value > 0, "check number of gateways");
        ok(prefixs.value.length > 0, "check prefixs.length");
        ok(gateways.length > 0, "check gateways.length");
        ok(numOfDnses.value > 0, "check number of dnses");
        ok(dnses.length > 0, "check dnses.length");
      } else {
        // Mobile data is disabled.
        is(networkInterface.state, NETWORK_STATE_DISCONNECTED, "check state");
        is(numOfIpAddresses, 0, "check number of ipAddresses");
        is(ipAddresses.value.length, 0, "check ipAddresses.length");
        is(prefixs.value.length, 0, "check prefixs.length");
        is(numOfGateways.value, 0, "check number of gateways");
        is(prefixs.value.length, 0, "check prefixs.length");
        is(gateways.length, 0, "check gateways.length");
        is(numOfDnses.value, 0, "check number of dnses");
        is(dnses.length, 0, "check dnses.length");
      }
    });
}

// Start test
startTestBase(function() {
  return Promise.resolve()
    // Test initial State
    .then(() => {
      log("Test initial state");

      // Data should be off before starting any test.
      return getSettings(SETTINGS_KEY_DATA_ENABLED)
        .then(value => {
          is(value, false, "Mobile data must be off");
        });
    })

    // Test getDataInterfaceList with mobile data enabled.
    .then(() => testGetDataInterfaceList(true))

    // Test getDataInterfaceList with mobile data disabled.
    .then(() => testGetDataInterfaceList(false));
});
