/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const SCAN_RETRY_CNT = 5;

/**
 * Test wifi association.
 *
 * Associate with the given network object which is obtained by
 * MozWifiManager.getNetworks() (i.e. MozWifiNetwork).
 * Resolve when the 'connected' status change event is received.
 * Note that we might see other events like 'connecting'
 * before 'connected'. So we need to call |waitForWifiManagerEventOnce|
 * again whenever non 'connected' event is seen. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aNetwork
 *        An object of MozWifiNetwork.
 *
 * @return A deferred promise.
 */
function testAssociate(aNetwork) {
  if (!setPasswordIfNeeded(aNetwork)) {
    throw 'Failed to set password';
  }

  function waitForConnected() {
    return gTestSuite.waitForWifiManagerEventOnce('statuschange')
      .then(function onstatuschange(event) {
        log("event.status: " + event.status);
        log("event.network.ssid: " + (event.network ? event.network.ssid : ''));

        if ("connected" === event.status &&
            event.network.ssid === aNetwork.ssid) {
          return; // Got expected 'connected' event from aNetwork.ssid.
        }

        log('Not expected "connected" statuschange event. Wait again!');
        return waitForConnected();
      });
  }

  let promises = [];

  // Register the event listerner to wait for 'connected' event first
  // to avoid racing issue.
  promises.push(waitForConnected());

  // Then we do the association.
  let request = gTestSuite.getWifiManager().associate(aNetwork);
  promises.push(gTestSuite.wrapDomRequestAsPromise(request));

  return Promise.all(promises);
}

/**
 * Convert the given MozWifiNetwork object array to testAssociate chain.
 *
 * @param aNetworks
 *        An array of MozWifiNetwork which we want to convert.
 *
 * @return A promise chain which "then"s testAssociate accordingly.
 */
function convertToTestAssociateChain(aNetworks) {
  let chain = Promise.resolve();

  aNetworks.forEach(function (aNetwork) {
    chain = chain.then(() => testAssociate(aNetwork));
  });

  return chain;
}

/**
 * Set the password for associating the given network if needed.
 *
 * Set the password by looking up HOSTAPD_CONFIG_LIST. This function
 * will also set |keyManagement| properly.
 *
 * @param aNetwork
 *        The MozWifiNetwork object.
 *
 * @return |true| if either insesure or successfully set the password/keyManagement.
 *         |false| if the given network is not found in HOSTAPD_CONFIG_LIST.
 */
function setPasswordIfNeeded(aNetwork) {
  let i = gTestSuite.getFirstIndexBySsid(aNetwork.ssid, HOSTAPD_CONFIG_LIST);
  if (-1 === i) {
    log('unknown ssid: ' + aNetwork.ssid);
    return false; // Error!
  }

  if (!aNetwork.security.length) {
    return true; // No need to set password.
  }

  let security = aNetwork.security[0];
  if (/PSK$/.test(security)) {
    aNetwork.psk = HOSTAPD_CONFIG_LIST[i].wpa_passphrase;
    aNetwork.keyManagement = 'WPA-PSK';
  } else if (/WEP$/.test(security)) {
    aNetwork.wep = HOSTAPD_CONFIG_LIST[i].wpa_passphrase;
    aNetwork.keyManagement = 'WEP';
  }

  return true;
}

gTestSuite.doTestWithoutStockAp(function() {
  return gTestSuite.ensureWifiEnabled(true)
    .then(() => gTestSuite.startHostapds(HOSTAPD_CONFIG_LIST))
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', HOSTAPD_CONFIG_LIST.length))
    .then(() => gTestSuite.testWifiScanWithRetry(SCAN_RETRY_CNT, HOSTAPD_CONFIG_LIST))
    .then(networks => convertToTestAssociateChain(networks))
    .then(gTestSuite.killAllHostapd)
    .then(() => gTestSuite.verifyNumOfProcesses('hostapd', 0));
});
