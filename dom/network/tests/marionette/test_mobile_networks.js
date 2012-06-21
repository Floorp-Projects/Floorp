/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// getNetworks() can take some time..
MARIONETTE_TIMEOUT = 60000;
 
const WHITELIST_PREF = "dom.mobileconnection.whitelist";
let uriPrePath = window.location.protocol + "//" + window.location.host;
SpecialPowers.setCharPref(WHITELIST_PREF, uriPrePath);

let connection = navigator.mozMobileConnection;
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

is(connection.networkSelectionMode, "automatic");

let androidNetwork = null;
let telkilaNetwork = null;

function isAndroidNetwork(network) {
  is(network.longName, "Android");
  is(network.shortName, "Android");
  is(network.mcc, 310);
  is(network.mnc, 260);
}

function isTelkilaNetwork(network) {
  is(network.longName, "TelKila");
  is(network.shortName, "TelKila");
  is(network.mcc, 310);
  is(network.mnc, 295);
}

function testConnectionInfo() {
  let voice = connection.voice;
  is(voice.connected, true);
  is(voice.emergencyCallsOnly, false);
  is(voice.roaming, false);
  isAndroidNetwork(voice.network);

  let data = connection.data;
  // TODO Bug 762959: enable these checks when data state updates are implemented
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
    setTimeout(testSelectNetwork, 0);
  };

  request.onsuccess = function() {
    ok('result' in request, "Request did not contain a result");
    let networks = request.result;

    // The emulator RIL server should always return 2 networks:
    // {"longName":"Android","shortName":"Android","mcc":310,"mnc":260,"state":"available"}
    // {"longName":"TelKila","shortName":"TelKila","mcc":310,"mnc":295,"state":"available"}
    is(networks.length, 2);

    let network1 = androidNetwork = networks[0];
    isAndroidNetwork(network1);
    is(network1.state, "available");

    let network2 = telkilaNetwork = networks[1];
    isTelkilaNetwork(network2);
    is(network2.state, "available");

    setTimeout(testSelectNetwork, 0);
  };
}

function testSelectNetwork() {
  let request = connection.selectNetwork(telkilaNetwork);
  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  connection.addEventListener("voicechange", function voiceChange() {
    connection.removeEventListener("voicechange", voiceChange);

    isTelkilaNetwork(connection.voice.network);
    setTimeout(testSelectNetworkAutomatically, 0);
  });

  request.onsuccess = function() {
    is(connection.networkSelectionMode, "manual",
       "selectNetwork sets mode to: " + connection.networkSelectionMode);
  };

  request.onerror = function() {
    ok(false, request.error);
    setTimeout(testSelectNetworkAutomatically, 0);
  };
}

function testSelectNetworkAutomatically() {
  let request = connection.selectNetworkAutomatically();
  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  connection.addEventListener("voicechange", function voiceChange() {
    connection.removeEventListener("voicechange", voiceChange);

    isAndroidNetwork(connection.voice.network);
    setTimeout(testSelectNetworkErrors, 0);
  });

  request.onsuccess = function() {
    is(connection.networkSelectionMode, "automatic",
       "selectNetworkAutomatically sets mode to: " +
       connection.networkSelectionMode);
  };

  request.onerror = function() {
    ok(false, request.error);
    setTimeout(testSelectNetworkErrors, 0);
  };
}

function throwsException(fn) {
  try {
    fn();
    ok(false, "function did not throw an exception: " + fn);
  } catch (e) {
    ok(true, "function succesfully caught exception: " + e);
  }
}

function testSelectNetworkErrors() {
  throwsException(function() {
    connection.selectNetwork(null);
  });

  throwsException(function() {
    connection.selectNetwork({});
  });

  connection.addEventListener("voicechange", function voiceChange() {
    connection.removeEventListener("voicechange", voiceChange);
    setTimeout(testSelectExistingNetworkManual, 0);
  });

  let request1 = connection.selectNetwork(telkilaNetwork);
  request1.onerror = function() {
    ok(false, request.error);
    setTimeout(testSelectExistingNetworkManual, 0);
  };

  // attempt to selectNetwork while one request has already been sent
  throwsException(function() {
    connection.selectNetwork(androidNetwork);
  });
}

function testSelectExistingNetworkManual() {
  // When the current network is selected again, the DOMRequest's onsuccess
  // should be called, but the network shouldn't actually change

  // Telkila should be the currently selected network
  log("Selecting TelKila (should already be selected");
  let request = connection.selectNetwork(telkilaNetwork);

  let voiceChanged = false;
  connection.addEventListener("voicechange", function voiceChange() {
    connection.removeEventListener("voicechange", voiceChange);
    voiceChanged = true;
  });

  function nextTest() {
    // Switch back to automatic selection to setup the next test
    let autoRequest = connection.selectNetworkAutomatically();
    autoRequest.onsuccess = function() {
      setTimeout(testSelectExistingNetworkAuto, 0);
    };
    autoRequest.onerror = function() {
      ok(false, autoRequest.error);
      cleanUp();
    };
  }

  request.onsuccess = function() {
    // Give the voicechange event another opportunity to fire
    setTimeout(function() {
      is(voiceChanged, false,
         "voiceNetwork changed while manually selecting Telkila network? " +
         voiceChanged);
      nextTest();
    }, 0);
  };

  request.onerror = function() {
    ok(false, request.error);
    nextTest();
  };
}

function testSelectExistingNetworkAuto() {
  // Now try the same thing but using automatic selection
  log("Selecting automatically (should already be auto)");
  let request = connection.selectNetworkAutomatically();

  let voiceChanged = false;
  connection.addEventListener("voicechange", function voiceChange() {
    connection.removeEventListener("voicechange", voiceChange);
    voiceChanged = true;
  });

  request.onsuccess = function() {
    // Give the voicechange event another opportunity to fire
    setTimeout(function() {
      is(voiceChanged, false,
         "voiceNetwork changed while automatically selecting network? " +
         voiceChanged);
      cleanUp();
    }, 0);
  };

  request.onerror = function() {
    ok(false, request.error);
    cleanUp();
  };
}

function cleanUp() {
  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}

testConnectionInfo();
