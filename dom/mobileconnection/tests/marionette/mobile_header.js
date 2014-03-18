/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

SpecialPowers.addPermission("mobileconnection", true, document);

// In single sim scenario, there is only one mobileConnection, we can always use
// the first instance.
let mobileConnection = window.navigator.mozMobileConnections[0];
ok(mobileConnection instanceof MozMobileConnection,
   "mobileConnection is instanceof " + mobileConnection.constructor);

let _pendingEmulatorCmdCount = 0;

/* Remove permission and execute finish() */
let cleanUp = function() {
  waitFor(function() {
    SpecialPowers.removePermission("mobileconnection", document);
    finish();
  }, function() {
    return _pendingEmulatorCmdCount === 0;
  });
};

/* Helper for tasks */
let taskHelper = {
  tasks: [],

  push: function(task) {
    this.tasks.push(task);
  },

  runNext: function() {
    let task = this.tasks.shift();
    if (!task) {
      cleanUp();
      return;
    }

    if (typeof task === "function") {
      task();
    }
  },
};

/* Helper for emulator console command */
let emulatorHelper = {
  sendCommand: function(cmd, callback) {
    _pendingEmulatorCmdCount++;
    runEmulatorCmd(cmd, function(results) {
      _pendingEmulatorCmdCount--;

      let result = results[results.length - 1];
      is(result, "OK", "'"+ cmd +"' returns '" + result + "'");

      if (callback && typeof callback === "function") {
        callback(results);
      }
    });
  },
};
