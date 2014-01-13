/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 20000;

SpecialPowers.addPermission("mobileconnection", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let mobileConnection;
ifr.onload = function() {
  mobileConnection = ifr.contentWindow.navigator.mozMobileConnections[0];

  tasks.run();
};
document.body.appendChild(ifr);

let tasks = {
  // List of test functions. Each of them should call |tasks.next()| when
  // completed or |tasks.abort()| to jump to the last one.
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
        this.abort();
      }
    }
  },

  abort: function() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function() {
    this.next();
  }
};

tasks.push(function verifyInitialState() {
  log("Verifying initial state.");

  ok(mobileConnection instanceof ifr.contentWindow.MozMobileConnection,
      "mobileConnection is instanceof " + mobileConnection.constructor);

  tasks.next();
});

tasks.push(function testGettingIMEI() {
  log("Test *#06# ...");

  let request = mobileConnection.sendMMI("*#06#");
  ok(request instanceof DOMRequest,
     "request is instanceof " + request.constructor);

  request.onsuccess = function onsuccess(event) {
    ok(true, "request success");
    is(typeof event.target.result, "object", "typeof result object");
    ok(event.target.result instanceof Object, "result instanceof Object");
    is(event.target.result.statusMessage, "000000000000000", "Emulator IMEI");
    is(event.target.result.serviceCode, "scImei", "Service code IMEI");
    is(event.target.result.additionalInformation, undefined,
       "No additional information");
    tasks.next();
  }
  request.onerror = function onerror() {
    ok(false, "request should not error");
    tasks.abort();
  };
});

tasks.push(function testInvalidMMICode(){
  log("Test invalid MMI code ...");

  let request = mobileConnection.sendMMI("InvalidMMICode");
  ok(request instanceof DOMRequest,
     "request is instanceof " + request.constructor);

  request.onsuccess = function onsuccess(event) {
    ok(false, "request should not success");
    tasks.abort();
  };

  request.onerror = function onerror() {
    ok(true, "request error");
    is(request.error.name, "emMmiError", "MMI error name");
    tasks.next();
  };
});

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
});
