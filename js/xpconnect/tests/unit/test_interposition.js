const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const ADDONID = "bogus-addon@mozilla.org";

let gExpectedProp;
function expectAccess(prop, f)
{
  gExpectedProp = prop;
  f();
  do_check_eq(gExpectedProp, undefined);
}

let getter_run = false;
function test_getter()
{
  do_check_eq(getter_run, false);
  getter_run = true;
  return 200;
}

let setter_run = false;
function test_setter(v)
{
  do_check_eq(setter_run, false);
  do_check_eq(v, 300);
  setter_run = true;
}

let TestInterposition = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAddonInterposition,
                                         Ci.nsISupportsWeakReference]),

  interpose: function(addonId, target, iid, prop) {
    do_check_eq(addonId, ADDONID);
    do_check_eq(gExpectedProp, prop);
    gExpectedProp = undefined;

    if (prop == "dataprop") {
      return { configurable: false, enumerable: true, writable: false, value: 100 };
    } else if (prop == "getterprop") {
      return { configurable: false, enumerable: true, get: test_getter };
    } else if (prop == "setterprop") {
      return { configurable: false, enumerable: true, get: test_getter, set: test_setter };
    } else if (prop == "utils") {
      do_check_eq(iid, Ci.nsIXPCComponents.number);
      return null;
    } else if (prop == "objprop") {
      gExpectedProp = "objprop"; // allow recursive access here
      return { configurable: false, enumerable: true, writable: false, value: { objprop: 1 } };
    } else if (prop == "configurableprop") {
      return { configurable: true, enumerable: true, writable: false, value: 10 };
    }

    return null;
  }
};

function run_test()
{
  Cu.setAddonInterposition(ADDONID, TestInterposition);

  let sandbox = Cu.Sandbox(this, {addonId: ADDONID});
  sandbox.outerObj = {};

  expectAccess("abcxyz", () => {
    Cu.evalInSandbox("outerObj.abcxyz = 12;", sandbox);
  });

  expectAccess("utils", () => {
    Cu.evalInSandbox("Components.utils;", sandbox);
  });

  expectAccess("dataprop", () => {
    do_check_eq(Cu.evalInSandbox("outerObj.dataprop;", sandbox), 100);
  });

  expectAccess("dataprop", () => {
    try {
      Cu.evalInSandbox("'use strict'; outerObj.dataprop = 400;", sandbox);
      do_check_true(false); // it should throw
    } catch (e) {}
  });

  expectAccess("objprop", () => {
    Cu.evalInSandbox("outerObj.objprop.objprop;", sandbox);
    gExpectedProp = undefined;
  });

  expectAccess("getterprop", () => {
    do_check_eq(Cu.evalInSandbox("outerObj.getterprop;", sandbox), 200);
    do_check_eq(getter_run, true);
    getter_run = false;
  });

  expectAccess("getterprop", () => {
    try {
      Cu.evalInSandbox("'use strict'; outerObj.getterprop = 400;", sandbox);
      do_check_true(false); // it should throw
    } catch (e) {}
    do_check_eq(getter_run, false);
  });

  expectAccess("setterprop", () => {
    do_check_eq(Cu.evalInSandbox("outerObj.setterprop;", sandbox), 200);
    do_check_eq(getter_run, true);
    getter_run = false;
    do_check_eq(setter_run, false);
  });

  expectAccess("setterprop", () => {
    Cu.evalInSandbox("'use strict'; outerObj.setterprop = 300;", sandbox);
    do_check_eq(getter_run, false);
    do_check_eq(setter_run, true);
    setter_run = false;
  });

  // Make sure that calling Object.defineProperty succeeds as long as
  // we're not interposing on that property.
  expectAccess("defineprop", () => {
    Cu.evalInSandbox("'use strict'; Object.defineProperty(outerObj, 'defineprop', {configurable:true, enumerable:true, writable:true, value:10});", sandbox);
  });

  // Related to above: make sure we can delete those properties too.
  expectAccess("defineprop", () => {
    Cu.evalInSandbox("'use strict'; delete outerObj.defineprop;", sandbox);
  });

  // Make sure we get configurable=false even if the interposition
  // sets it to true.
  expectAccess("configurableprop", () => {
    let desc = Cu.evalInSandbox("Object.getOwnPropertyDescriptor(outerObj, 'configurableprop')", sandbox);
    do_check_eq(desc.configurable, false);
  });

  Cu.setAddonInterposition(ADDONID, null);
}
