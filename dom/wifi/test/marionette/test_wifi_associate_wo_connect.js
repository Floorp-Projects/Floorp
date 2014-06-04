/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

/**
 * Associate with the given networks but don't connect to it.
 *
 * Issue a association-only request, which will not have us connect
 * to the network. Instead, the network config will be added as the
 * known networks. After calling the "associate" API, we will wait
 * a while to make sure the "connected" event is not received.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aNetwork
 *        MozWifiNetwork object.
 *
 * @return A deferred promise.
 */
function associateButDontConnect(aNetwork) {
  log('Associating with ' + aNetwork.ssid);
  aNetwork.dontConnect = true;

  let promises = [];
  promises.push(gTestSuite.waitForTimeout(10 * 1000)
                  .then(() => { throw 'timeout'; }));

  promises.push(gTestSuite.testAssociate(aNetwork));

  return Promise.all(promises)
    .then(() => { throw 'unexpected state'; },
          function(aReason) {
      is(typeof aReason, 'string', 'typeof aReason');
      is(aReason, 'timeout', aReason);
    });
}

gTestSuite.doTest(function() {
  let firstNetwork;
  return gTestSuite.ensureWifiEnabled(true)
    .then(gTestSuite.requestWifiScan)
    .then(function(aNetworks) {
      firstNetwork = aNetworks[0];
      return associateButDontConnect(firstNetwork);
    })
    .then(gTestSuite.getKnownNetworks)
    .then(function(aKnownNetworks) {
      is(1, aKnownNetworks.length, 'There should be only one known network!');
      is(aKnownNetworks[0].ssid, firstNetwork.ssid,
         'The only one known network should be ' + firstNetwork.ssid)
    });
});
