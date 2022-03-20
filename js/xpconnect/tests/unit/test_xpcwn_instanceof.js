/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests for custom `instanceof` behavior via XPC_SCRIPTABLE_WANT_HASINSTANCE.

add_task(function id_instanceof() {
  // ID objects are instances of Components.ID.
  let id = Components.ID("{f2f5c784-7f6c-43f5-81b0-45ff32c312b1}");
  Assert.equal(id instanceof Components.ID, true);
  Assert.equal({} instanceof Components.ID, false);
  Assert.equal(null instanceof Components.ID, false);

  // Components.ID has a Symbol.hasInstance function.
  let desc = Object.getOwnPropertyDescriptor(Components.ID, Symbol.hasInstance);
  Assert.equal(typeof desc, "object");
  Assert.equal(typeof desc.value, "function");

  // Test error handling when calling this function with unexpected values.
  Assert.throws(() => desc.value.call(null), /At least 1 argument required/);
  Assert.throws(() => desc.value.call(null, 1), /unexpected this value/);
  Assert.throws(() => desc.value.call({}, {}), /NS_ERROR_XPC_BAD_OP_ON_WN_PROTO/);
});
