/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const DATA_KEY = "ril.data.enabled";
const DATA_ROAMING_KEY = "ril.data.roaming_enabled";
const APN_KEY = "ril.data.apnSettings";

SpecialPowers.setBoolPref("dom.mozSettings.enabled", true);
SpecialPowers.addPermission("mobileconnection", true, document);
SpecialPowers.addPermission("settings-read", true, document);
SpecialPowers.addPermission("settings-write", true, document);

let settings = window.navigator.mozSettings;
let connection = window.navigator.mozMobileConnections[0];
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);


let pendingEmulatorCmdCount = 0;
function sendCmdToEmulator(cmd, callback) {
  ++pendingEmulatorCmdCount;

  runEmulatorCmd(cmd, function(result) {
    --pendingEmulatorCmdCount;

    is(result[0], "OK", "Emulator response");

    if (callback) {
      callback();
    }
  });
}

let tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function(func) {
    this._tasks.push(func);
  },

  next: function() {
    let index = this._nextTaskIndex++;
    let task = this._tasks[index];
    try {
      task();
    } catch (ex) {
      ok(false, "test task[" + index + "] throws: " + ex);
      // Run last task as clean up if possible.
      if (index != this._tasks.length - 1) {
        this.finish();
      }
    }
  },

  finish: function() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function() {
    this.next();
  }
};

function setSetting(key, value, callback) {
  let setLock = settings.createLock();
  let obj = {};
  obj[key] = value;

  let setReq = setLock.set(obj);
  setReq.addEventListener("success", function onSetSuccess() {
    ok(true, "set '" + key + "' to " + obj[key]);
    if (callback) {
      callback();
    }
  });
  setReq.addEventListener("error", function onSetError() {
    ok(false, "cannot set '" + key + "'");
    tasks.finish();
  });
}

function getSetting(key, callback) {
  let getLock = settings.createLock();

  let getReq = getLock.get(key);
  getReq.addEventListener("success", function onGetSuccess() {
    ok(true, "get " + key + " setting okay");
    let value = getReq.result[key];
    callback(value);
  });
  getReq.addEventListener("error", function onGetError() {
    ok(false, "cannot get '" + key + "'");
    tasks.finish();
  });
}

function setEmulatorAPN(callback) {
  let apn =
    [
      [
        {"carrier":"T-Mobile US",
         "apn":"epc.tmobile.com",
         "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
         "types":["default","supl","mms"]}
      ]
    ];
  setSetting(APN_KEY, apn, callback);
}

function setEmulatorRoaming(roaming, callback) {
  log("Setting emulator roaming state: " + roaming + ".");

  // Set voice registration state first and then data registration state.
  let cmd = "gsm voice " + (roaming ? "roaming" : "home");
  sendCmdToEmulator(cmd, function() {

    connection.addEventListener("voicechange", function onvoicechange() {
      connection.removeEventListener("voicechange", onvoicechange);
      log("mobileConnection.voice.roaming is now '"
        + connection.voice.roaming + "'.");
      is(connection.voice.roaming, roaming, "voice.roaming");

      let cmd = "gsm data " + (roaming ? "roaming" : "home");
      sendCmdToEmulator(cmd, function() {

        connection.addEventListener("datachange", function ondatachange() {
          connection.removeEventListener("datachange", ondatachange);
          log("mobileConnection.data.roaming is now '"
            + connection.data.roaming + "'.");
          is(connection.data.roaming, roaming, "data.roaming");
          if (callback) {
            callback();
          }
        });
      });
    });
  });
}

function setEmulatorHome(callback) {
  let voiceRegistration = false;
  let dataRegistration = false;

  if (connection.voice.state != "registered") {
    sendCmdToEmulator("gsm voice home", function() {
      connection.addEventListener("voicechange", function onvoicechange() {
        connection.removeEventListener("voicechange", onvoicechange);
        log("mobileConnection.voice.state is now '"
          + connection.voice.state + "'.");
        is(connection.voice.state, "registered", "voice.state");
        voiceRegistration = true;
      });
    });
  } else {
    voiceRegistration = true;
  }

  if (connection.data.state != "registered") {
    sendCmdToEmulator("gsm data home", function() {
      connection.addEventListener("datachange", function ondatachange() {
        connection.removeEventListener("datachange", ondatachange);
        log("mobileConnection.data.state is now '"
          + connection.data.state + "'.");
        is(connection.data.state, "registered", "data.state");
        dataRegistration = true;
      });
    });
  } else {
    dataRegistration = true;
  }

  waitFor(callback, function() {
    return (voiceRegistration && dataRegistration);
  });
}


tasks.push(function verifyInitialState() {
  log("Verifying initial state.");

  // Want to start test with mobileConnection.data.state 'registered',
  // This is the default state; if it is not currently this value then set it.
  setEmulatorHome(function() {
    // Want to start test with data off,
    // This is the default state; if it is not currently this value then set it.
    getSetting(DATA_KEY, function(result) {
      let value = result;
      log("Starting data enabled: " + value);
      if (value) {
        setSetting(DATA_KEY, false);

        connection.addEventListener("datachange", function ondatachange() {
          connection.removeEventListener("datachange", ondatachange);
          log("mobileConnection.data.connected is now '"
            + connection.data.connected + "'.");
          is(connection.data.connected, false, "data.connected");
          setEmulatorAPN(function() {
            tasks.next();
          });
        });
      } else {
        setEmulatorAPN(function() {
          tasks.next();
        });
      }
    });
  });
});

tasks.push(function testEnableData() {
  log("Turn data on.");

  connection.addEventListener("datachange", function ondatachange() {
    connection.removeEventListener("datachange", ondatachange);
    log("mobileConnection.data.connected is now '"
      + connection.data.connected + "'.");
    is(connection.data.connected, true, "data.connected");
    tasks.next();
  });

  setSetting(DATA_KEY, true);
});

tasks.push(function testUnregisterDataWhileDataEnabled() {
  log("Set data registration unregistered while data enabled.");

  // When data registration is unregistered, all data calls
  // will be automatically deactivated.
  sendCmdToEmulator("gsm data unregistered", function() {
    connection.addEventListener("datachange", function ondatachange() {
      log("mobileConnection.data.state is now '"
        + connection.data.state + "'.");
      if (connection.data.state == "notSearching") {
        connection.removeEventListener("datachange", ondatachange);
        log("mobileConnection.data.connected is now '"
          + connection.data.connected + "'.");
        is(connection.data.connected, false, "data.connected");
        tasks.next();
      }
    });
  });
});

tasks.push(function testRegisterDataWhileDataEnabled() {
  log("Set data registration home while data enabled.");

  // When data registration is registered, data call will be
  // (re)activated by gecko if ril.data.enabled is set to true.
  sendCmdToEmulator("gsm data home", function() {
    connection.addEventListener("datachange", function ondatachange() {
      connection.removeEventListener("datachange", ondatachange);
      log("mobileConnection.data.state is now '"
        + connection.data.state + "'.");
      is(connection.data.state, "registered", "data.state");

      connection.addEventListener("datachange", function ondatachange() {
        connection.removeEventListener("datachange", ondatachange);
        log("mobileConnection.data.connected is now '"
          + connection.data.connected + "'.");
        is(connection.data.connected, true, "data.connected");
        tasks.next();
      });
    });
  });
});

tasks.push(function testDisableDataRoamingWhileRoaming() {
  log("Disable data roaming while roaming.");

  setSetting(DATA_ROAMING_KEY, false);

  // Wait for roaming state to change, then data connection should
  // be disconnected due to data roaming set to off.
  setEmulatorRoaming(true, function() {
    connection.addEventListener("datachange", function ondatachange() {
      connection.removeEventListener("datachange", ondatachange);
      log("mobileConnection.data.connected is now '"
        + connection.data.connected + "'.");
      is(connection.data.connected, false, "data.connected");
      tasks.next();
    });
  });
});

tasks.push(function testEnableDataRoamingWhileRoaming() {
  log("Enable data roaming while roaming.");

  // Data should be re-connected as we enabled data roaming.
  connection.addEventListener("datachange", function ondatachange() {
    connection.removeEventListener("datachange", ondatachange);
    log("mobileConnection.data.connected is now '"
      + connection.data.connected + "'.");
    is(connection.data.connected, true, "data.connected");
    tasks.next();
  });

  setSetting(DATA_ROAMING_KEY, true);
});

tasks.push(function testDisableDataRoamingWhileNotRoaming() {
  log("Disable data roaming while not roaming.");

  // Wait for roaming state to change then set data roaming back
  // to off.
  setEmulatorRoaming(false, function() {
    setSetting(DATA_ROAMING_KEY, false);

    // No change event will be received cause data connection state
    // remains the same.
    window.setTimeout(function() {
      is(connection.data.connected, true, "data.connected");
      tasks.next();
    }, 1000);
  });
});

tasks.push(function testDisableData() {
  log("Turn data off.");

  connection.addEventListener("datachange", function ondatachange() {
    connection.removeEventListener("datachange", ondatachange);
    log("mobileConnection.data.connected is now '"
      + connection.data.connected + "'.");
    is(connection.data.connected, false, "data.connected");
    tasks.next();
  });

  setSetting(DATA_KEY, false);
});

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("mobileconnection", document);
  SpecialPowers.removePermission("settings-write", document);
  SpecialPowers.removePermission("settings-read", document);
  SpecialPowers.clearUserPref("dom.mozSettings.enabled");
  finish();
});

tasks.run();

