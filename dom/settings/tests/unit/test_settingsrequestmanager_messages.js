"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

var principal = Services.scriptSecurityManager.getSystemPrincipal();
var lockID = "{435d2192-4f21-48d4-90b7-285f147a56be}";

// Helper to start the Settings Request Manager
function startSettingsRequestManager() {
  Cu.import("resource://gre/modules/SettingsRequestManager.jsm");
}

function handlerHelper(reply, callback, runNext = true) {
  let handler = {
    receiveMessage: function(message) {
      if (message.name === reply) {
        cpmm.removeMessageListener(reply, handler);
        callback(message);
        if (runNext) {
          run_next_test();
        }
      }
    }
  };
  cpmm.addMessageListener(reply, handler);
}

// Helper function to add a listener, send message and treat the reply
function addAndSend(msg, reply, callback, payload, runNext = true) {
  handlerHelper(reply, callback, runNext);
  cpmm.sendAsyncMessage(msg, payload, undefined, principal);
}

function errorHandler(reply, str) {
  let errHandler = function(message) {
    ok(true, str);
  };

  handlerHelper(reply, errHandler);
}

// We need to trigger a Settings:Run message to make the queue progress
function send_settingsRun() {
  let msg = {lockID: lockID, isServiceLock: true};
  cpmm.sendAsyncMessage("Settings:Run", msg, undefined, principal);
}

function kill_child() {
  let msg = {lockID: lockID, isServiceLock: true};
  cpmm.sendAsyncMessage("child-process-shutdown", msg, undefined, principal);
}

function run_test() {
  do_get_profile();
  startSettingsRequestManager();
  run_next_test();
}

add_test(function test_createLock() {
  let msg = {lockID: lockID, isServiceLock: true};
  cpmm.sendAsyncMessage("Settings:CreateLock", msg, undefined, principal);
  cpmm.sendAsyncMessage(
    "Settings:RegisterForMessages", undefined, undefined, principal);
  ok(true);
  run_next_test();
});

add_test(function test_get_empty() {
  let requestID = 10;
  let msgReply = "Settings:Get:OK";
  let msgHandler = function(message) {
    equal(requestID, message.data.requestID);
    equal(lockID, message.data.lockID);
    ok(Object.keys(message.data.settings).length >= 0);
  };

  errorHandler("Settings:Get:KO", "Settings GET failed");

  addAndSend("Settings:Get", msgReply, msgHandler, {
    requestID: requestID,
    lockID: lockID,
    name: "language.current"
  });

  send_settingsRun();
});

add_test(function test_set_get_nonempty() {
  let settings = { "language.current": "fr-FR:XPC" };
  let requestIDSet = 20;
  let msgReplySet = "Settings:Set:OK";
  let msgHandlerSet = function(message) {
    equal(requestIDSet, message.data.requestID);
    equal(lockID, message.data.lockID);
  };

  errorHandler("Settings:Set:KO", "Settings SET failed");

  addAndSend("Settings:Set", msgReplySet, msgHandlerSet, {
    requestID: requestIDSet,
    lockID: lockID,
    settings: settings
  }, false);

  let requestIDGet = 25;
  let msgReplyGet = "Settings:Get:OK";
  let msgHandlerGet = function(message) {
    equal(requestIDGet, message.data.requestID);
    equal(lockID, message.data.lockID);
    for(let p in settings) {
      equal(settings[p], message.data.settings[p]);
    }
  };

  addAndSend("Settings:Get", msgReplyGet, msgHandlerGet, {
    requestID: requestIDGet,
    lockID: lockID,
    name: Object.keys(settings)[0]
  });

  // Set and Get have been push into the queue, let's run
  send_settingsRun();
});

// This test exposes bug 1076597 behavior
add_test(function test_wait_for_finalize() {
  let settings = { "language.current": "en-US:XPC" };
  let requestIDSet = 30;
  let msgReplySet = "Settings:Set:OK";
  let msgHandlerSet = function(message) {
    equal(requestIDSet, message.data.requestID);
    equal(lockID, message.data.lockID);
  };

  errorHandler("Settings:Set:KO", "Settings SET failed");

  addAndSend("Settings:Set", msgReplySet, msgHandlerSet, {
    requestID: requestIDSet,
    lockID: lockID,
    settings: settings
  }, false);

  let requestIDGet = 35;
  let msgReplyGet = "Settings:Get:OK";
  let msgHandlerGet = function(message) {
    equal(requestIDGet, message.data.requestID);
    equal(lockID, message.data.lockID);
    for(let p in settings) {
      equal(settings[p], message.data.settings[p]);
    }
  };

  errorHandler("Settings:Get:KO", "Settings GET failed");

  addAndSend("Settings:Get", msgReplyGet, msgHandlerGet, {
    requestID: requestIDGet,
    lockID: lockID,
    name: Object.keys(settings)[0]
  });

  // We simulate a child death, which will force previous requests to be set
  // into finalize state
  kill_child();

  // Then when we issue Settings:Run, those finalized should be triggered
  send_settingsRun();
});
