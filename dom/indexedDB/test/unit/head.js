/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { 'classes': Cc, 'interfaces': Ci, 'utils': Cu } = Components;

const DOMException = Ci.nsIDOMDOMException;

function is(a, b, msg) {
  dump("is(" + a + ", " + b + ", \"" + msg + "\")");
  do_check_eq(a, b, Components.stack.caller);
}

function ok(cond, msg) {
  dump("ok(" + cond + ", \"" + msg + "\")");
  do_check_true(!!cond, Components.stack.caller); 
}

function isnot(a, b, msg) {
  dump("isnot(" + a + ", " + b + ", \"" + msg + "\")");
  do_check_neq(a, b, Components.stack.caller); 
}

function executeSoon(fun) {
  do_execute_soon(fun);
}

function todo(condition, name, diag) {
  dump("TODO: ", diag);
}

function info(name, message) {
  do_print(name);
}

function run_test() {
  runTest();
};

function runTest()
{
  // XPCShell does not get a profile by default.
  do_get_profile();

  enableExperimental();

  do_test_pending();
  testGenerator.next();
}

function finishTest()
{
  resetExperimental();
  SpecialPowers.notifyObserversInParentProcess(null, "disk-space-watcher",
                                               "free");

  do_execute_soon(function(){
    testGenerator.close();
    do_test_finished();
  })
}

function grabEventAndContinueHandler(event)
{
  testGenerator.send(event);
}

function continueToNextStep()
{
  do_execute_soon(function() {
    testGenerator.next();
  });
}

function errorHandler(event)
{
  dump("indexedDB error: " + event.target.error.name);
  do_check_true(false);
  finishTest();
}

function unexpectedSuccessHandler()
{
  do_check_true(false);
  finishTest();
}

function expectedErrorHandler(name)
{
  return function(event) {
    do_check_eq(event.type, "error");
    do_check_eq(event.target.error.name, name);
    event.preventDefault();
    grabEventAndContinueHandler(event);
  };
}

function ExpectError(name)
{
  this._name = name;
}
ExpectError.prototype = {
  handleEvent: function(event)
  {
    do_check_eq(event.type, "error");
    do_check_eq(this._name, event.target.error.name);
    event.preventDefault();
    grabEventAndContinueHandler(event);
  }
};

function continueToNextStepSync()
{
  testGenerator.next();
}

function compareKeys(k1, k2) {
  let t = typeof k1;
  if (t != typeof k2)
    return false;

  if (t !== "object")
    return k1 === k2;

  if (k1 instanceof Date) {
    return (k2 instanceof Date) &&
      k1.getTime() === k2.getTime();
  }

  if (k1 instanceof Array) {
    if (!(k2 instanceof Array) ||
        k1.length != k2.length)
      return false;
    
    for (let i = 0; i < k1.length; ++i) {
      if (!compareKeys(k1[i], k2[i]))
        return false;
    }
    
    return true;
  }

  return false;
}

function addPermission(permission, url)
{
  throw "addPermission";
}

function removePermission(permission, url)
{
  throw "removePermission";
}

function setQuota(quota)
{
  throw "setQuota";
}

function allowIndexedDB(url)
{
  throw "allowIndexedDB";
}

function disallowIndexedDB(url)
{
  throw "disallowIndexedDB";
}

function allowUnlimitedQuota(url)
{
  throw "allowUnlimitedQuota";
}

function disallowUnlimitedQuota(url)
{
  throw "disallowUnlimitedQuota";
}

function enableExperimental()
{
  SpecialPowers.setBoolPref("dom.indexedDB.experimental", true);
}

function resetExperimental()
{
  SpecialPowers.clearUserPref("dom.indexedDB.experimental");
}

function gc()
{
  Components.utils.forceGC();
  Components.utils.forceCC();
}

function scheduleGC()
{
  SpecialPowers.exactGC(null, continueToNextStep);
}

function setTimeout(fun, timeout) {
  let timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(Components.interfaces.nsITimer);
  var event = {
    notify: function (timer) {
      fun();
    }
  };
  timer.initWithCallback(event, timeout,
                         Components.interfaces.nsITimer.TYPE_ONE_SHOT);
  return timer;
}

var SpecialPowers = {
  isMainProcess: function() {
    return Components.classes["@mozilla.org/xre/app-info;1"]
                     .getService(Components.interfaces.nsIXULRuntime)
                     .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  },
  notifyObservers: function(subject, topic, data) {
    var obsvc = Cc['@mozilla.org/observer-service;1']
                   .getService(Ci.nsIObserverService);
    obsvc.notifyObservers(subject, topic, data);
  },
  notifyObserversInParentProcess: function(subject, topic, data) {
    if (subject) {
      throw new Error("Can't send subject to another process!");
    }
    return this.notifyObservers(subject, topic, data);
  },
  getBoolPref: function(prefName) {
    return this._getPrefs().getBoolPref(prefName);
  },
  setBoolPref: function(prefName, value) {
    this._getPrefs().setBoolPref(prefName, value);
  },
  setIntPref: function(prefName, value) {
    this._getPrefs().setIntPref(prefName, value);
  },
  clearUserPref: function(prefName) {
    this._getPrefs().clearUserPref(prefName);
  },
  // Copied (and slightly adjusted) from specialpowersAPI.js
  exactGC: function(win, callback) {
    let count = 0;

    function doPreciseGCandCC() {
      function scheduledGCCallback() {
        Components.utils.forceCC();

        if (++count < 2) {
          doPreciseGCandCC();
        } else {
          callback();
        }
      }

      Components.utils.schedulePreciseGC(scheduledGCCallback);
    }

    doPreciseGCandCC();
  },

  _getPrefs: function() {
    var prefService =
      Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);
    return prefService.getBranch(null);
  }
};
