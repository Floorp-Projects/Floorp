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
  Assert.equal(gExpectedProp, undefined);
}

let getter_run = false;
function test_getter()
{
  Assert.equal(getter_run, false);
  getter_run = true;
  return 200;
}

let setter_run = false;
function test_setter(v)
{
  Assert.equal(setter_run, false);
  Assert.equal(v, 300);
  setter_run = true;
}

let TestInterposition = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAddonInterposition,
                                         Ci.nsISupportsWeakReference]),

  getWhitelist: function() {
    return ["abcxyz", "utils", "dataprop", "getterprop", "setterprop",
            "objprop", "defineprop", "configurableprop"];
  },

  interposeProperty: function(addonId, target, iid, prop) {
    Assert.equal(addonId, ADDONID);
    Assert.equal(gExpectedProp, prop);
    gExpectedProp = undefined;

    if (prop == "dataprop") {
      return { configurable: false, enumerable: true, writable: false, value: 100 };
    } else if (prop == "getterprop") {
      return { configurable: false, enumerable: true, get: test_getter };
    } else if (prop == "setterprop") {
      return { configurable: false, enumerable: true, get: test_getter, set: test_setter };
    } else if (prop == "utils") {
      Assert.equal(iid, Ci.nsIXPCComponents.number);
      return null;
    } else if (prop == "objprop") {
      gExpectedProp = "objprop"; // allow recursive access here
      return { configurable: false, enumerable: true, writable: false, value: { objprop: 1 } };
    } else if (prop == "configurableprop") {
      return { configurable: true, enumerable: true, writable: false, value: 10 };
    }

    return null;
  },

  interposeCall: function(addonId, originalFunc, originalThis, args) {
    Assert.equal(addonId, ADDONID);
    args.splice(0, 0, addonId);
    return originalFunc.apply(originalThis, args);
  }
};

function run_test()
{
  Cu.setAddonInterposition(ADDONID, TestInterposition);

  Cu.importGlobalProperties(["XMLHttpRequest"]);

  let sandbox = Cu.Sandbox(this, {addonId: ADDONID});
  sandbox.outerObj = new XMLHttpRequest();

  expectAccess("abcxyz", () => {
    Cu.evalInSandbox("outerObj.abcxyz = 12;", sandbox);
  });

  expectAccess("utils", () => {
    Cu.evalInSandbox("Components.utils;", sandbox);
  });

  expectAccess("dataprop", () => {
    Assert.equal(Cu.evalInSandbox("outerObj.dataprop;", sandbox), 100);
  });

  expectAccess("dataprop", () => {
    try {
      Cu.evalInSandbox("'use strict'; outerObj.dataprop = 400;", sandbox);
      Assert.ok(false); // it should throw
    } catch (e) {}
  });

  expectAccess("objprop", () => {
    Cu.evalInSandbox("outerObj.objprop.objprop;", sandbox);
    gExpectedProp = undefined;
  });

  expectAccess("getterprop", () => {
    Assert.equal(Cu.evalInSandbox("outerObj.getterprop;", sandbox), 200);
    Assert.equal(getter_run, true);
    getter_run = false;
  });

  expectAccess("getterprop", () => {
    try {
      Cu.evalInSandbox("'use strict'; outerObj.getterprop = 400;", sandbox);
      Assert.ok(false); // it should throw
    } catch (e) {}
    Assert.equal(getter_run, false);
  });

  expectAccess("setterprop", () => {
    Assert.equal(Cu.evalInSandbox("outerObj.setterprop;", sandbox), 200);
    Assert.equal(getter_run, true);
    getter_run = false;
    Assert.equal(setter_run, false);
  });

  expectAccess("setterprop", () => {
    Cu.evalInSandbox("'use strict'; outerObj.setterprop = 300;", sandbox);
    Assert.equal(getter_run, false);
    Assert.equal(setter_run, true);
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
    Assert.equal(desc.configurable, false);
  });

  let moduleScope = Cu.Sandbox(this);
  moduleScope.ADDONID = ADDONID;
  moduleScope.do_check_eq = do_check_eq;
  function funToIntercept(addonId) {
    Assert.equal(addonId, ADDONID);
    counter++;
  }
  sandbox.moduleFunction = Cu.evalInSandbox(funToIntercept.toSource() + "; funToIntercept", moduleScope);
  Cu.evalInSandbox("var counter = 0;", moduleScope);
  Cu.evalInSandbox("Components.utils.setAddonCallInterposition(this);", moduleScope);
  Cu.evalInSandbox("moduleFunction()", sandbox);
  Assert.equal(Cu.evalInSandbox("counter", moduleScope), 1);
  Cu.setAddonInterposition(ADDONID, null);
}
