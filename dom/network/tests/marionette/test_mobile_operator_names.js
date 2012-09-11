/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

let connection = navigator.mozMobileConnection;
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

let voice = connection.voice;
ok(voice, "voice connection valid");

let network = voice.network;
ok(network, "voice network info valid");

let emulatorCmdPendingCount = 0;
function setEmulatorOperatorNames(longName, shortName) {
  emulatorCmdPendingCount++;

  let cmd = "operator set 0 " + longName + "," + shortName;
  runEmulatorCmd(cmd, function (result) {
    emulatorCmdPendingCount--;

    let re = new RegExp("^" + longName + "," + shortName + ",");
    ok(result[0].match(re), "Long/short name should be changed.");
  });
}

function checkValidMccMnc() {
  is(network.mcc, 310, "network.mcc");
  is(network.mnc, 260, "network.mnc");
}

function doTestMobileOperatorNames(longName, shortName, callback) {
  log("Testing '" + longName + "', '" + shortName + "':");

  checkValidMccMnc();

  connection.addEventListener("voicechange", function onvoicechange() {
    connection.removeEventListener("voicechange", onvoicechange);

    is(network.longName, longName, "network.longName");
    is(network.shortName, shortName, "network.shortName");

    checkValidMccMnc();

    setTimeout(callback, 0);
  });

  setEmulatorOperatorNames(longName, shortName);
}

function testMobileOperatorNames() {
  doTestMobileOperatorNames("Mozilla", "B2G", function () {
    doTestMobileOperatorNames("Mozilla", "", function () {
      doTestMobileOperatorNames("", "B2G", function () {
        doTestMobileOperatorNames("", "", function () {
          doTestMobileOperatorNames("Android", "Android", cleanUp);
        });
      });
    });
  });
}

function cleanUp() {
  if (emulatorCmdPendingCount > 0) {
    setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

testMobileOperatorNames();
