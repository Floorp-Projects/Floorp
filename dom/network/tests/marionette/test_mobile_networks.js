/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// getNetworks() can take some time..
MARIONETTE_TIMEOUT = 30000;
 
const WHITELIST_PREF = "dom.mobileconnection.whitelist";
let uriPrePath = window.location.protocol + "//" + window.location.host;
SpecialPowers.setCharPref(WHITELIST_PREF, uriPrePath);

ok(navigator.mozMobileConnection instanceof MozMobileConnection,
   "mozMobileConnection is instanceof " + navigator.mozMobileConnection.constructor);

let request = navigator.mozMobileConnection.getNetworks();
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
  is(network1.longName, "Android");
  is(network1.shortName, "Android");
  is(network1.mcc, 310);
  is(network1.mnc, 260);
  is(network1.state, "available");

  let network2 = networks[1];
  is(network2.longName, "TelKila");
  is(network2.shortName, "TelKila");
  is(network2.mcc, 310);
  is(network2.mnc, 295);
  is(network2.state, "available");
  cleanUp();
};

function cleanUp() {
  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}
