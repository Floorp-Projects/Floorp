/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function instanceof_xrays() {
  let sandbox = Cu.Sandbox(null);
  Cu.evalInSandbox(`
    this.proxy = new Proxy([], {
      getPrototypeOf() {
        return Date.prototype;
      },
    });

    this.inheritedProxy = Object.create(this.proxy);

    this.FunctionProxy = new Proxy(function() {}, {});
    this.functionProxyInstance = new this.FunctionProxy();

    this.CustomClass = class {};
    this.customClassInstance = new this.CustomClass();
  `, sandbox);

  {
    // Sanity check that instanceof still works with standard constructors when xrays are present.
    Assert.ok(Cu.evalInSandbox(`new Date()`, sandbox) instanceof sandbox.Date,
              "async function result in sandbox instanceof sandbox.Date");
    Assert.ok(new sandbox.Date() instanceof sandbox.Date,
              "sandbox.Date() instanceof sandbox.Date");

    Assert.ok(sandbox.CustomClass instanceof sandbox.Function,
              "Class constructor instanceof sandbox.Function");
    Assert.ok(sandbox.CustomClass instanceof sandbox.Object,
              "Class constructor instanceof sandbox.Object");

    // Both operands must have the same kind of Xray vision.
    Assert.equal(Cu.waiveXrays(sandbox.CustomClass) instanceof sandbox.Function, false,
                 "Class constructor with waived xrays instanceof sandbox.Function");
    Assert.equal(Cu.waiveXrays(sandbox.CustomClass) instanceof sandbox.Object, false,
                 "Class constructor with waived xrays instanceof sandbox.Object");
  }

  {
    let {proxy} = sandbox;
    Assert.equal(proxy instanceof sandbox.Date, false,
                 "instanceof should ignore the proxy trap");
    Assert.equal(proxy instanceof sandbox.Array, false,
                 "instanceof should ignore the proxy target");
    Assert.equal(Cu.waiveXrays(proxy) instanceof sandbox.Date, false,
                 "instanceof should ignore the proxy trap despite the waived xrays on the proxy");
    Assert.equal(Cu.waiveXrays(proxy) instanceof sandbox.Array, false,
                 "instanceof should ignore the proxy target despite the waived xrays on the proxy");

    Assert.ok(proxy instanceof Cu.waiveXrays(sandbox.Date),
              "instanceof should trigger the proxy trap after waiving Xrays on the constructor");
    Assert.equal(proxy instanceof Cu.waiveXrays(sandbox.Array), false,
                 "instanceof should trigger the proxy trap after waiving Xrays on the constructor");

    Assert.ok(Cu.waiveXrays(proxy) instanceof Cu.waiveXrays(sandbox.Date),
              "instanceof should trigger the proxy trap after waiving both Xrays");
  }


  {
    let {inheritedProxy} = sandbox;
    Assert.equal(inheritedProxy instanceof sandbox.Date, false,
                 "instanceof should ignore the inherited proxy trap");
    Assert.equal(Cu.waiveXrays(inheritedProxy) instanceof sandbox.Date, false,
                 "instanceof should ignore the inherited proxy trap despite the waived xrays on the proxy");

    Assert.ok(inheritedProxy instanceof Cu.waiveXrays(sandbox.Date),
              "instanceof should trigger the inherited proxy trap after waiving Xrays on the constructor");

    Assert.ok(Cu.waiveXrays(inheritedProxy) instanceof Cu.waiveXrays(sandbox.Date),
              "instanceof should trigger the inherited proxy trap after waiving both Xrays");
  }

  {
    let {FunctionProxy, functionProxyInstance} = sandbox;

    // Ideally, the next two test cases should both throw "... not a function".
    // However, because the opaque XrayWrapper does not override isCallable, an
    // opaque XrayWrapper is still considered callable if the proxy target is,
    // and "instanceof" will try to look up the prototype of the wrapper (and
    // fail because opaque XrayWrappers hide the properties).
    Assert.throws(
      () => functionProxyInstance instanceof FunctionProxy,
      /'prototype' property of FunctionProxy is not an object/,
      "Opaque constructor proxy should be hidden by Xrays");
    Assert.throws(
      () => functionProxyInstance instanceof sandbox.proxy,
      /sandbox.proxy is not a function/,
      "Opaque non-constructor proxy should be hidden by Xrays");

    Assert.ok(functionProxyInstance instanceof Cu.waiveXrays(FunctionProxy),
              "instanceof should get through the proxy after waiving Xrays on the constructor proxy");
    Assert.ok(Cu.waiveXrays(functionProxyInstance) instanceof Cu.waiveXrays(FunctionProxy),
              "instanceof should get through the proxy after waiving both Xrays");
  }

  {
    let {CustomClass, customClassInstance} = sandbox;
    // Under Xray vision, every JS object is either a plain object or array.
    // Prototypical inheritance is invisible when the constructor is wrapped.
    Assert.throws(
      () => customClassInstance instanceof CustomClass,
      /TypeError: 'prototype' property of CustomClass is not an object/,
      "instanceof on a custom JS class with xrays should fail");
    Assert.ok(customClassInstance instanceof Cu.waiveXrays(CustomClass),
              "instanceof should see the true prototype of CustomClass after waiving Xrays on the class");
    Assert.ok(Cu.waiveXrays(customClassInstance) instanceof Cu.waiveXrays(CustomClass),
              "instanceof should see the true prototype of CustomClass after waiving Xrays");
  }
});

add_task(function instanceof_dom_xrays_hasInstance() {
  const principal = Services.scriptSecurityManager.createNullPrincipal({});
  const webnav = Services.appShell.createWindowlessBrowser(false);
  webnav.docShell.createAboutBlankDocumentViewer(principal, principal);
  let window = webnav.document.defaultView;

  let sandbox = Cu.Sandbox(principal);
  sandbox.DOMObjectWithHasInstance = window.document;
  Cu.evalInSandbox(`
    this.DOMObjectWithHasInstance[Symbol.hasInstance] = function() {
      return true;
    };
    this.ObjectWithHasInstance = {
      [Symbol.hasInstance](v) {
        v.throwsIfVCannotBeAccessed;
        return true;
      },
    };
  `, sandbox);

  // Override the hasInstance handler in the window, so that we can detect when
  // we end up triggering hasInstance in the window's compartment.
  window.eval(`
    document[Symbol.hasInstance] = function() {
      throw "hasInstance_in_window";
    };
  `);

  sandbox.domobj = window.document.body;
  Assert.ok(sandbox.eval(`domobj.wrappedJSObject`),
            "DOM object is a XrayWrapper");
  Assert.ok(sandbox.eval(`DOMObjectWithHasInstance.wrappedJSObject`),
            "DOM object with Symbol.hasInstance is a XrayWrapper");

  for (let Obj of ["ObjectWithHasInstance", "DOMObjectWithHasInstance"]) {
    // Tests Xray vision *inside* the sandbox. The Symbol.hasInstance member
    // is a property / expando object in the sandbox's compartment, so the
    // "instanceof" operator should always trigger the hasInstance function.
    Assert.ok(sandbox.eval(`[] instanceof ${Obj}`),
              `Should call ${Obj}[Symbol.hasInstance] when left operand has no Xrays`);
    Assert.ok(sandbox.eval(`domobj instanceof ${Obj}`),
              `Should call ${Obj}[Symbol.hasInstance] when left operand has Xrays`);
    Assert.ok(sandbox.eval(`domobj.wrappedJSObject instanceof ${Obj}`),
              `Should call ${Obj}[Symbol.hasInstance] when left operand has waived Xrays`);

    // Tests Xray vision *outside* the sandbox. The Symbol.hasInstance member
    // should be hidden by Xrays.
    let sandboxObjWithHasInstance = sandbox[Obj];
    Assert.ok(Cu.isXrayWrapper(sandboxObjWithHasInstance),
              `sandbox.${Obj} is a XrayWrapper`);
    Assert.throws(
      () => sandbox.Object() instanceof sandboxObjWithHasInstance,
      /sandboxObjWithHasInstance is not a function/,
      `sandbox.${Obj}[Symbol.hasInstance] should be hidden by Xrays`);

    Assert.throws(
      () => Cu.waiveXrays(sandbox.Object()) instanceof sandboxObjWithHasInstance,
      /sandboxObjWithHasInstance is not a function/,
      `sandbox.${Obj}[Symbol.hasInstance] should be hidden by Xrays, despite the waived Xrays at the left`);

    // (Cases where the left operand has no Xrays are checked below.)
  }

  // hasInstance is expected to be called, but still trigger an error because
  // properties of the object from the current context should not be readable
  // by the hasInstance function in the sandbox with a different principal.
  Assert.throws(
    () => [] instanceof Cu.waiveXrays(sandbox.ObjectWithHasInstance),
    /Permission denied to access property "throwsIfVCannotBeAccessed"/,
    `Should call (waived) sandbox.ObjectWithHasInstance[Symbol.hasInstance] when the right operand has waived Xrays`);

  // The Xray waiver on the right operand should be sufficient to call
  // hasInstance even if the left operand still has Xrays.
  Assert.ok(sandbox.Object() instanceof Cu.waiveXrays(sandbox.ObjectWithHasInstance),
            `Should call (waived) sandbox.ObjectWithHasInstance[Symbol.hasInstance] when the right operand has waived Xrays`);
  Assert.ok(Cu.waiveXrays(sandbox.Object()) instanceof Cu.waiveXrays(sandbox.ObjectWithHasInstance),
            `Should call (waived) sandbox.ObjectWithHasInstance[Symbol.hasInstance] when both operands have waived Xrays`);

  // When Xrays of the DOM object are waived, we end up in the owner document's
  // compartment (instead of the sandbox).
  Assert.throws(
    () => [] instanceof Cu.waiveXrays(sandbox.DOMObjectWithHasInstance),
    /hasInstance_in_window/,
    "Should call (waived) sandbox.DOMObjectWithHasInstance[Symbol.hasInstance] when the right operand has waived Xrays");

  Assert.throws(
    () => Cu.waiveXrays(sandbox.Object()) instanceof Cu.waiveXrays(sandbox.DOMObjectWithHasInstance),
    /hasInstance_in_window/,
    "Should call (waived) sandbox.DOMObjectWithHasInstance[Symbol.hasInstance] when both operands have waived Xrays");

  webnav.close();
});
