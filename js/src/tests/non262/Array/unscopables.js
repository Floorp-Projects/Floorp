let Array_unscopables = Array.prototype[Symbol.unscopables];

let desc = Reflect.getOwnPropertyDescriptor(Array.prototype, Symbol.unscopables);
assertDeepEq(desc, {
    value: Array_unscopables,
    writable: false,
    enumerable: false,
    configurable: true
});

assertEq(Reflect.getPrototypeOf(Array_unscopables), null);

let desc2 = Object.getOwnPropertyDescriptor(Array_unscopables, "values");
assertDeepEq(desc2, {
    value: true,
    writable: true,
    enumerable: true,
    configurable: true
});

let keys = Reflect.ownKeys(Array_unscopables);

let expectedKeys = ["at",
		    "copyWithin",
		    "entries",
		    "fill",
		    "find",
		    "findIndex",
		    "findLast",
		    "findLastIndex",
		    "flat",
		    "flatMap",
		    "includes",
		    "keys",
		    "values"];

if (typeof getBuildConfiguration === "undefined") {
  var getBuildConfiguration = SpecialPowers.Cu.getJSTestingFunctions().getBuildConfiguration;
}

if (typeof getRealmConfiguration === "undefined") {
  var getRealmConfiguration = SpecialPowers.Cu.getJSTestingFunctions().getRealmConfiguration;
}

if (!getBuildConfiguration().release_or_beta && getRealmConfiguration().enableArrayGrouping) {
    expectedKeys.push("group", "groupToMap");
}

if (getBuildConfiguration()['change-array-by-copy'] && getRealmConfiguration().enableChangeArrayByCopy) {
    expectedKeys.push("withAt", "withReversed", "withSorted", "withSpliced");
}

assertDeepEq(keys, expectedKeys);

for (let key of keys)
    assertEq(Array_unscopables[key], true);

// Test that it actually works
assertThrowsInstanceOf(() => {
    with ([]) {
        return entries;
    }
}, ReferenceError);

{
    let fill = 33;
    with (Array.prototype) {
        assertEq(fill, 33);
    }
}

reportCompare(0, 0);
