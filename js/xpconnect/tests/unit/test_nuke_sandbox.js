/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=769273 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const global = this;

function run_test()
{
  var ifacePointer = Cc["@mozilla.org/supports-interface-pointer;1"]
      .createInstance(Ci.nsISupportsInterfacePointer);

  var sb = Cu.Sandbox(global);
  sb.prop = "prop"
  sb.ifacePointer = ifacePointer

  var refToObjFromSb = Cu.evalInSandbox(`
    Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

    ifacePointer.data = {
      QueryInterface: XPCOMUtils.generateQI([]),
      wrappedJSObject: {foo: "bar"},
    };

    var a = {prop2:'prop2'};
    a
  `, sb);

  equal(ifacePointer.data.wrappedJSObject.foo, "bar",
        "Got expected wrapper into sandbox")

  Cu.nukeSandbox(sb);
  ok(Cu.isDeadWrapper(sb), "sb should be dead");
  ok(Cu.isDeadWrapper(ifacePointer.data.wrappedJSObject),
     "Wrapper retrieved via XPConnect should be dead");

  try{
    sb.prop;
    Assert.ok(false);
  } catch (e) {
    Assert.ok(e.toString().indexOf("can't access dead object") > -1);
  }

  Components.utils.isDeadWrapper(refToObjFromSb, "ref to object from sb should be dead");
  try{
    refToObjFromSb.prop2;
    Assert.ok(false);
  } catch (e) {
    Assert.ok(e.toString().indexOf("can't access dead object") > -1);
  }
}
