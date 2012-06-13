/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// getNetworks() can take some time..
MARIONETTE_TIMEOUT = 30000;
 
const WHITELIST_PREF = "dom.mobileconnection.whitelist";
let uriPrePath = window.location.protocol + "//" + window.location.host;
SpecialPowers.setCharPref(WHITELIST_PREF, uriPrePath);

let connection = navigator.mozMobileConnection;
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

function isAndroidNetwork(network) {
  is(network.longName, "Android");
  is(network.shortName, "Android");
  is(network.mcc, 310);
  is(network.mnc, 260);
}

function testConnectionInfo() {
  let voice = connection.voice;
  is(voice.connected, true);
  is(voice.emergencyCallsOnly, false);
  is(voice.roaming, false);
  isAndroidNetwork(voice.network);

  let data = connection.data;
  // TODO Bug 762959: enable these checks when data state updates have been implemented
  // is(data.connected, true);
  // is(data.emergencyCallsOnly, false);
  // is(data.roaming, false);
  isAndroidNetwork(data.network);

  testGetNetworks();
}

function testGetNetworks() {
  let request = connection.getNetworks();
  ok(request instanceof DOMRequest,
      "request is instanceof " + request.constructor);

  request.onerror = function() {
    ok(false, request.error);
    cleanUp();
  };

  request.onsuccess = function() {
    ok('result' in request, "Request did not contain a result");
    let networks = request.result;

    // The emulator RIL server should always return 2 networks:
    // {"longName":"Android","shortName":"Android","mcc":310,"mnc":260,"state":"available"}
    // {"longName":"TelKila","shortName":"TelKila","mcc":310,"mnc":295,"state":"available"}
    is(networks.length, 2);

    let network1 = networks[0];
    isAndroidNetwork(network1);
    is(network1.state, "available");

    let network2 = networks[1];
    is(network2.longName, "TelKila");
    is(network2.shortName, "TelKila");
    is(network2.mcc, 310);
    is(network2.mnc, 295);
    is(network2.state, "available");
    cleanUp();
  };
}

function cleanUp() {
  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}

testConnectionInfo();
