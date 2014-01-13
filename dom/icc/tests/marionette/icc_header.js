/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

SpecialPowers.addPermission("mobileconnection", true, document);

let iccManager = navigator.mozIccManager;
ok(iccManager instanceof MozIccManager,
   "iccManager is instanceof " + iccManager.constructor);

// TODO: Bug 932650 - B2G RIL: WebIccManager API - add marionette tests for
//                    multi-sim
// In single sim scenario, there is only one sim card, we can use below way to
// check iccId and get icc object. But in multi-sim, the index of iccIds may
// not map to sim slot directly, we should have a better way to handle this.
let iccIds = iccManager.iccIds;
ok(Array.isArray(iccIds), "iccIds is an array");
is(iccIds.length, 1, "iccIds.length is " + iccIds.length);

let iccId = iccIds[0];
is(iccId, "89014103211118510720", "iccId is " + iccId);

let icc = iccManager.getIccById(iccId);
ok(icc instanceof MozIcc, "icc is instanceof " + icc.constructor);

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
    runEmulatorCmd(cmd, function(result) {
      this.pendingCommandCount--;
      is(result[result.length - 1], "OK");

      if (callback && typeof callback === "function") {
        callback(result);
      }
    });
  },
};
