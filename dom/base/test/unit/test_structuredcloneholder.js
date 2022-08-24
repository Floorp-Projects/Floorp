"use strict";

const global = this;

add_task(async function test_structuredCloneHolder() {
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://example.com/"),
    {}
  );

  let sandbox = Cu.Sandbox(principal);

  const obj = { foo: [{ bar: "baz" }] };

  let holder = new StructuredCloneHolder(obj);

  // Test same-compartment deserialization

  let res = holder.deserialize(global, true);

  notEqual(
    res,
    obj,
    "Deserialized result is a different object from the original"
  );

  deepEqual(
    res,
    obj,
    "Deserialized result is deeply equivalent to the original"
  );

  equal(
    Cu.getObjectPrincipal(res),
    Cu.getObjectPrincipal(global),
    "Deserialized result has the correct principal"
  );

  // Test non-object-value round-trip.

  equal(
    new StructuredCloneHolder("foo").deserialize(global),
    "foo",
    "Round-tripping non-object values works as expected"
  );

  // Test cross-compartment deserialization

  res = holder.deserialize(sandbox, true);

  notEqual(
    res,
    obj,
    "Cross-compartment-deserialized result is a different object from the original"
  );

  deepEqual(
    res,
    obj,
    "Cross-compartment-deserialized result is deeply equivalent to the original"
  );

  equal(
    Cu.getObjectPrincipal(res),
    principal,
    "Cross-compartment-deserialized result has the correct principal"
  );

  // Test message manager transportability

  const MSG = "StructuredCloneHolder";

  let resultPromise = new Promise(resolve => {
    Services.ppmm.addMessageListener(MSG, resolve);
  });

  Services.cpmm.sendAsyncMessage(MSG, holder);

  res = await resultPromise;

  ok(
    StructuredCloneHolder.isInstance(res.data),
    "Sending structured clone holders through message managers works as expected"
  );

  deepEqual(
    res.data.deserialize(global, true),
    obj,
    "Sending structured clone holders through message managers works as expected"
  );

  // Test that attempting to deserialize a neutered holder throws.

  deepEqual(
    holder.deserialize(global),
    obj,
    "Deserialized result is correct when discarding data"
  );

  Assert.throws(
    () => holder.deserialize(global),
    err => err.result == Cr.NS_ERROR_NOT_INITIALIZED,
    "Attempting to deserialize neutered holder throws"
  );

  Assert.throws(
    () => holder.deserialize(global, true),
    err => err.result == Cr.NS_ERROR_NOT_INITIALIZED,
    "Attempting to deserialize neutered holder throws"
  );
});

// Test that X-rays passed to an exported function are serialized
// through their exported wrappers.
add_task(async function test_structuredCloneHolder_xray() {
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://example.com/"),
    {}
  );

  let sandbox1 = Cu.Sandbox(principal, { wantXrays: true });

  let sandbox2 = Cu.Sandbox(principal, { wantXrays: true });
  Cu.evalInSandbox(`this.x = {y: "z", get z() { return "q" }}`, sandbox2);

  sandbox1.x = sandbox2.x;

  let holder;
  Cu.exportFunction(
    function serialize(val) {
      holder = new StructuredCloneHolder(val, sandbox1);
    },
    sandbox1,
    { defineAs: "serialize" }
  );

  Cu.evalInSandbox(`serialize(x)`, sandbox1);

  const obj = { y: "z" };

  let res = holder.deserialize(global);

  deepEqual(
    res,
    obj,
    "Deserialized result is deeply equivalent to the expected object"
  );
  deepEqual(
    res,
    sandbox2.x,
    "Deserialized result is deeply equivalent to the X-ray-wrapped object"
  );

  equal(
    Cu.getObjectPrincipal(res),
    Cu.getObjectPrincipal(global),
    "Deserialized result has the correct principal"
  );
});
