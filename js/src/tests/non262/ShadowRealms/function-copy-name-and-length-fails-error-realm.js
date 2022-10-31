// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

var sr = new ShadowRealm();

var id = sr.evaluate(`x => x()`);

// |id| is a Function from the current realm and _not_ from ShadowRealm.
assertEq(id instanceof Function, true);

function f() {
  return 1;
}

// Smoke test: Ensure calling |f| through the ShadowRealm works correctly.
assertEq(id(f), 1);

// Add an accessor for "name" which throws. This will lead to throwing an
// exception in CopyNameAndLength. The thrown exception should be from the
// realm of |id|, i.e. the current realm.
Object.defineProperty(f, "name", {
  get() { throw new Error; }
});

assertThrowsInstanceOf(() => id(f), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(true, true);
