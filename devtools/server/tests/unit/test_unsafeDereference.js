// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

/* eslint-disable strict */

// Test Debugger.Object.prototype.unsafeDereference in the presence of
// interesting cross-compartment wrappers.
//
// This is not really a debugger server test; it's more of a Debugger test.
// But we need xpcshell and Components.utils.Sandbox to get
// cross-compartment wrappers with interesting properties, and this is the
// xpcshell test directory most closely related to the JS Debugger API.

Components.utils.import("resource://gre/modules/jsdebugger.jsm");
addDebuggerToGlobal(this);

// Add a method to Debugger.Object for fetching value properties
// conveniently.
Debugger.Object.prototype.getProperty = function (name) {
  let desc = this.getOwnPropertyDescriptor(name);
  if (!desc) {
    return undefined;
  }
  if (!desc.value) {
    throw Error("Debugger.Object.prototype.getProperty: " +
                "not a value property: " + name);
  }
  return desc.value;
};

function run_test() {
  // Create a low-privilege sandbox, and a chrome-privilege sandbox.
  let contentBox = Components.utils.Sandbox("http://www.example.com");
  let chromeBox = Components.utils.Sandbox(this);

  // Create an objects in this compartment, and one in each sandbox. We'll
  // refer to the objects as "mainObj", "contentObj", and "chromeObj", in
  // variable and property names.
  let mainObj = { name: "mainObj" };
  Components.utils.evalInSandbox('var contentObj = { name: "contentObj" };',
                                 contentBox);
  Components.utils.evalInSandbox('var chromeObj = { name: "chromeObj" };',
                                 chromeBox);

  // Give each global a pointer to all the other globals' objects.
  contentBox.mainObj = chromeBox.mainObj = mainObj;
  let contentObj = chromeBox.contentObj = contentBox.contentObj;
  let chromeObj = contentBox.chromeObj = chromeBox.chromeObj;

  // First, a whole bunch of basic sanity checks, to ensure that JavaScript
  // evaluated in various scopes really does see the world the way this
  // test expects it to.

  // The objects appear as global variables in the sandbox, and as
  // the sandbox object's properties in chrome.
  Assert.ok(Components.utils.evalInSandbox("mainObj", contentBox)
            === contentBox.mainObj);
  Assert.ok(Components.utils.evalInSandbox("contentObj", contentBox)
            === contentBox.contentObj);
  Assert.ok(Components.utils.evalInSandbox("chromeObj", contentBox)
            === contentBox.chromeObj);
  Assert.ok(Components.utils.evalInSandbox("mainObj", chromeBox)
            === chromeBox.mainObj);
  Assert.ok(Components.utils.evalInSandbox("contentObj", chromeBox)
            === chromeBox.contentObj);
  Assert.ok(Components.utils.evalInSandbox("chromeObj", chromeBox)
            === chromeBox.chromeObj);

  // We (the main global) can see properties of all objects in all globals.
  Assert.ok(contentBox.mainObj.name === "mainObj");
  Assert.ok(contentBox.contentObj.name === "contentObj");
  Assert.ok(contentBox.chromeObj.name === "chromeObj");

  // chromeBox can see properties of all objects in all globals.
  Assert.equal(Components.utils.evalInSandbox("mainObj.name", chromeBox),
               "mainObj");
  Assert.equal(Components.utils.evalInSandbox("contentObj.name", chromeBox),
               "contentObj");
  Assert.equal(Components.utils.evalInSandbox("chromeObj.name", chromeBox),
               "chromeObj");

  // contentBox can see properties of the content object, but not of either
  // chrome object, because by default, content -> chrome wrappers hide all
  // object properties.
  Assert.equal(Components.utils.evalInSandbox("mainObj.name", contentBox),
               undefined);
  Assert.equal(Components.utils.evalInSandbox("contentObj.name", contentBox),
               "contentObj");
  Assert.equal(Components.utils.evalInSandbox("chromeObj.name", contentBox),
               undefined);

  // When viewing an object in compartment A from the vantage point of
  // compartment B, Debugger should give the same results as debuggee code
  // would.

  // Create a debugger, debugging our two sandboxes.
  let dbg = new Debugger();

  // Create Debugger.Object instances referring to the two sandboxes, as
  // seen from their own compartments.
  let contentBoxDO = dbg.addDebuggee(contentBox);
  let chromeBoxDO = dbg.addDebuggee(chromeBox);

  // Use Debugger to view the objects from contentBox. We should get the
  // same D.O instance from both getProperty and makeDebuggeeValue, and the
  // same property visibility we checked for above.
  let mainFromContentDO = contentBoxDO.getProperty("mainObj");
  Assert.equal(mainFromContentDO, contentBoxDO.makeDebuggeeValue(mainObj));
  Assert.equal(mainFromContentDO.getProperty("name"), undefined);
  Assert.equal(mainFromContentDO.unsafeDereference(), mainObj);

  let contentFromContentDO = contentBoxDO.getProperty("contentObj");
  Assert.equal(contentFromContentDO, contentBoxDO.makeDebuggeeValue(contentObj));
  Assert.equal(contentFromContentDO.getProperty("name"), "contentObj");
  Assert.equal(contentFromContentDO.unsafeDereference(), contentObj);

  let chromeFromContentDO = contentBoxDO.getProperty("chromeObj");
  Assert.equal(chromeFromContentDO, contentBoxDO.makeDebuggeeValue(chromeObj));
  Assert.equal(chromeFromContentDO.getProperty("name"), undefined);
  Assert.equal(chromeFromContentDO.unsafeDereference(), chromeObj);

  // Similarly, viewing from chromeBox.
  let mainFromChromeDO = chromeBoxDO.getProperty("mainObj");
  Assert.equal(mainFromChromeDO, chromeBoxDO.makeDebuggeeValue(mainObj));
  Assert.equal(mainFromChromeDO.getProperty("name"), "mainObj");
  Assert.equal(mainFromChromeDO.unsafeDereference(), mainObj);

  let contentFromChromeDO = chromeBoxDO.getProperty("contentObj");
  Assert.equal(contentFromChromeDO, chromeBoxDO.makeDebuggeeValue(contentObj));
  Assert.equal(contentFromChromeDO.getProperty("name"), "contentObj");
  Assert.equal(contentFromChromeDO.unsafeDereference(), contentObj);

  let chromeFromChromeDO = chromeBoxDO.getProperty("chromeObj");
  Assert.equal(chromeFromChromeDO, chromeBoxDO.makeDebuggeeValue(chromeObj));
  Assert.equal(chromeFromChromeDO.getProperty("name"), "chromeObj");
  Assert.equal(chromeFromChromeDO.unsafeDereference(), chromeObj);
}
