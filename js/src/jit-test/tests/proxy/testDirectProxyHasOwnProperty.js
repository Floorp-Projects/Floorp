// Forward to the target if the trap is not defined
var proxy = Proxy(Object.create(Object.create(null, {
    'foo': {
        configurable: true
    }
}), {
    'bar': {
        configurable: true
    }
}), {});
assertEq(({}).hasOwnProperty.call(proxy, 'foo'), false);
assertEq(({}).hasOwnProperty.call(proxy, 'bar'), true);

// Make sure only the getOwnPropertyDescriptor trap is called, and not the has
// trap.
var called = false;
var handler = { getOwnPropertyDescriptor: function () { called = true; },
                has: function () { assertEq(false, true, "has trap must not be called"); }
              }
proxy = new Proxy({}, handler);
assertEq(({}).hasOwnProperty.call(proxy, 'foo'), false);
assertEq(called, true);
