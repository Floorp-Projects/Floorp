// |reftest| skip-if(!Promise.allSettled)

// Smoke test for `Promise.allSettled`, test262 should cover the function in
// more detail.

// Empty elements.
Promise.allSettled([]).then(v => {
    assertDeepEq(v, []);
});

// Single element.
Promise.allSettled([Promise.resolve(0)]).then(v => {
    assertDeepEq(v, [
        {"status": "fulfilled", "value": 0},
    ]);
});
Promise.allSettled([Promise.reject(1)]).then(v => {
    assertDeepEq(v, [
        {"status": "rejected", "reason": 1},
    ]);
});

// Multiple elements.
Promise.allSettled([Promise.resolve(1), Promise.resolve(2)]).then(v => {
    assertDeepEq(v, [
        {"status": "fulfilled", "value": 1},
        {"status": "fulfilled", "value": 2},
    ]);
});
Promise.allSettled([Promise.resolve(3), Promise.reject(4)]).then(v => {
    assertDeepEq(v, [
        {"status": "fulfilled", "value": 3},
        {"status": "rejected", "reason": 4},
    ]);
});
Promise.allSettled([Promise.reject(5), Promise.resolve(6)]).then(v => {
    assertDeepEq(v, [
        {"status": "rejected", "reason": 5},
        {"status": "fulfilled", "value": 6},
    ]);
});
Promise.allSettled([Promise.reject(7), Promise.reject(8)]).then(v => {
    assertDeepEq(v, [
        {"status": "rejected", "reason": 7},
        {"status": "rejected", "reason": 8},
    ]);
});

// Cross-Realm tests.
//
// Note: When |g| is a cross-compartment global, Promise.allSettled creates
// the result array in |g|'s Realm. This doesn't follow the spec, but the code
// in js/src/builtin/Promise.cpp claims this is useful when the Promise
// compartment is less-privileged. This means for this test we can't use
// assertDeepEq below, because the result array may have the wrong prototype.
let g = newGlobal();

if (typeof isSameCompartment !== "function") {
    var isSameCompartment = SpecialPowers.Cu.getJSTestingFunctions().isSameCompartment;
}

// Test wrapping when neither Promise.allSettled element function is called.
Promise.allSettled.call(g.Promise, []).then(v => {
    assertEq(isSameCompartment(v, g), true);

    assertEq(v.length, 0);
});

// Test wrapping in `Promise.allSettled Resolve Element Function`.
Promise.allSettled.call(g.Promise, [Promise.resolve(0)]).then(v => {
    assertEq(isSameCompartment(v, g), true);

    assertEq(v.length, 1);
    assertEq(v[0].status, "fulfilled");
    assertEq(v[0].value, 0);
});

// Test wrapping in `Promise.allSettled Reject Element Function`.
Promise.allSettled.call(g.Promise, [Promise.reject(0)]).then(v => {
    assertEq(isSameCompartment(v, g), true);

    assertEq(v.length, 1);
    assertEq(v[0].status, "rejected");
    assertEq(v[0].reason, 0);
});

if (typeof reportCompare === "function")
    reportCompare(0, 0);
