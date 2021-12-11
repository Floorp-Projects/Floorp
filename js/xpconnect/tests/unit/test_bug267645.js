/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let sb = new Cu.Sandbox("https://example.com",
                          { wantGlobalProperties: ["DOMException"] });
  Cu.exportFunction(function() {
    undefined.foo();
  }, sb, { defineAs: "func" });
  // By default, the stacks of things running in a sandbox will contain the
  // actual evalInSandbox() call location.  To override that, we have to pass an
  // explicit filename.
  let threw = Cu.evalInSandbox("var threw; try { func(); threw = false; } catch (e) { globalThis.exn = e; threw = true } threw",
                                sb, "", "FakeFile");
  Assert.ok(threw);

  // Check what the sandbox could see from this exception.
  Assert.ok(!Cu.evalInSandbox("exn.filename", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.fileName", sb), undefined);
  Assert.ok(!Cu.evalInSandbox("exn.stack", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.message", sb), "An exception was thrown");
  Assert.equal(Cu.evalInSandbox("exn.name", sb), "InvalidStateError");

  Cu.exportFunction(function() {
    throw new Error("Hello");
  }, sb, { defineAs: "func2" });
  threw = Cu.evalInSandbox("var threw; try { func2(); threw = false; } catch (e) { globalThis.exn = e; threw = true } threw",
                                sb, "", "FakeFile");
  Assert.ok(threw);
  Assert.ok(!Cu.evalInSandbox("exn.filename", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.fileName", sb), undefined);
  Assert.ok(!Cu.evalInSandbox("exn.stack", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.message", sb), "An exception was thrown");
  Assert.equal(Cu.evalInSandbox("exn.name", sb), "InvalidStateError");

  let ctor = Cu.evalInSandbox("TypeError", sb);
  Cu.exportFunction(function() {
    throw new ctor("Hello");
  }, sb, { defineAs: "func3" });
  threw = Cu.evalInSandbox("var threw; try { func3(); threw = false; } catch (e) { globalThis.exn = e; threw = true } threw",
                                sb, "", "FakeFile");
  Assert.ok(threw);
  Assert.ok(!Cu.evalInSandbox("exn.fileName", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.filename", sb), undefined);
  Assert.ok(!Cu.evalInSandbox("exn.stack", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.message", sb), "Hello");
  Assert.equal(Cu.evalInSandbox("exn.name", sb), "TypeError");

  ctor = Cu.evalInSandbox("DOMException", sb);
  Cu.exportFunction(function() {
    throw new ctor("Goodbye", "InvalidAccessError");
  }, sb, { defineAs: "func4" });
  threw = Cu.evalInSandbox("var threw; try { func4(); threw = false; } catch (e) { globalThis.exn = e; threw = true } threw",
                                sb, "", "FakeFile");
  Assert.ok(threw);
  Assert.ok(!Cu.evalInSandbox("exn.filename", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.fileName", sb), undefined);
  Assert.ok(!Cu.evalInSandbox("exn.stack", sb).includes("/unit/"));
  Assert.equal(Cu.evalInSandbox("exn.message", sb), "Goodbye");
  Assert.equal(Cu.evalInSandbox("exn.name", sb), "InvalidAccessError");
}
