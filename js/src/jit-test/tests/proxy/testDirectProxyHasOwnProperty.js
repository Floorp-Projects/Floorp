// Forward to the target if the trap is not defined
var proto = Object.create(null, {
    'foo': {
        configurable: true
    }
});
var descs = {
    'bar': {
        configurable: true
    }
};
descs[Symbol.for("quux")] = {configurable: true};
var target = Object.create(proto, descs);
var proxy = Proxy(target, {});
assertEq(({}).hasOwnProperty.call(proxy, 'foo'), false);
assertEq(({}).hasOwnProperty.call(proxy, 'bar'), true);
assertEq(({}).hasOwnProperty.call(proxy, 'quux'), false);
assertEq(({}).hasOwnProperty.call(proxy, Symbol('quux')), false);
assertEq(({}).hasOwnProperty.call(proxy, 'Symbol(quux)'), false);
assertEq(({}).hasOwnProperty.call(proxy, Symbol.for('quux')), true);

// Make sure only the getOwnPropertyDescriptor trap is called, and not the has
// trap.
var called = false;
var handler = { getOwnPropertyDescriptor: function () { called = true; },
                has: function () { assertEq(false, true, "has trap must not be called"); }
              }
proxy = new Proxy({}, handler);
assertEq(({}).hasOwnProperty.call(proxy, 'foo'), false);
assertEq(called, true);
