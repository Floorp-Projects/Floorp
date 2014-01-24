/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

SpecialPowers.addPermission("mobileconnection", true, document);

// In single sim scenario, there is only one mobileConnection, we can always use
// the first instance.
let mobileConnection = window.navigator.mozMobileConnections[0];
ok(mobileConnection instanceof MozMobileConnection,
   "mobileConnection is instanceof " + mobileConnection.constructor);

/* Remove permission and execute finish() */
let cleanUp = function() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
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
  pendingCommandCount: 0,

  sendCommand: function(cmd, callback) {
    this.pendingCommandCount++;
    runEmulatorCmd(cmd, function(results) {
      this.pendingCommandCount--;

      let result = results[results.length - 1];
      is(result, "OK", "'"+ cmd +"' returns '" + result + "'");

      if (callback && typeof callback === "function") {
        callback(results);
      }
    });
  },
};
