// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test UnwrapNumberFormat operation.

const numberFormatFunctions = [];
numberFormatFunctions.push({
    function: Intl.NumberFormat.prototype.resolvedOptions,
    unwrap: true,
});
numberFormatFunctions.push({
    function: Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, "format").get,
    unwrap: true,
});
numberFormatFunctions.push({
    function: Intl.NumberFormat.prototype.formatToParts,
    unwrap: false,
});

function IsConstructor(o) {
  try {
    new (new Proxy(o, {construct: () => ({})}));
    return true;
  } catch (e) {
    return false;
  }
}

function IsObject(o) {
    return Object(o) === o;
}

function IsPrimitive(o) {
    return Object(o) !== o;
}

function intlObjects(ctor) {
    return [
        // Instance of an Intl constructor.
        new ctor(),

        // Instance of a subclassed Intl constructor.
        new class extends ctor {},

        // Intl object not inheriting from its default prototype.
        Object.setPrototypeOf(new ctor(), Object.prototype),
    ];
}

function thisValues(C) {
    const intlConstructors = Object.getOwnPropertyNames(Intl).map(name => Intl[name]).filter(IsConstructor);

    return [
        // Primitive values.
        ...[undefined, null, true, "abc", Symbol(), 123],

        // Object values.
        ...[{}, [], /(?:)/, function(){}, new Proxy({}, {})],

        // Intl objects.
        ...[].concat(...intlConstructors.filter(ctor => ctor !== C).map(intlObjects)),

        // Object inheriting from an Intl constructor prototype.
        ...intlConstructors.map(ctor => Object.create(ctor.prototype)),
    ];
}

const intlFallbackSymbol = Object.getOwnPropertySymbols(Intl.NumberFormat.call(Object.create(Intl.NumberFormat.prototype)))[0];

// Test Intl.NumberFormat.prototype methods.
for (let {function: numberFormatFunction, unwrap} of numberFormatFunctions) {
    // Test a TypeError is thrown when the this-value isn't an initialized
    // Intl.NumberFormat instance.
    for (let thisValue of thisValues(Intl.NumberFormat)) {
        assertThrowsInstanceOf(() => numberFormatFunction.call(thisValue), TypeError);
    }

    // And test no error is thrown for initialized Intl.NumberFormat instances.
    for (let thisValue of intlObjects(Intl.NumberFormat)) {
        numberFormatFunction.call(thisValue);
    }

    // Manually add [[FallbackSymbol]] to objects and then repeat the tests from above.
    for (let thisValue of thisValues(Intl.NumberFormat)) {
        assertThrowsInstanceOf(() => numberFormatFunction.call({
            __proto__: Intl.NumberFormat.prototype,
            [intlFallbackSymbol]: thisValue,
        }), TypeError);
    }

    for (let thisValue of intlObjects(Intl.NumberFormat)) {
        let obj = {
            __proto__: Intl.NumberFormat.prototype,
            [intlFallbackSymbol]: thisValue,
        };
        if (unwrap) {
            numberFormatFunction.call(obj);
        } else {
            assertThrowsInstanceOf(() => numberFormatFunction.call(obj), TypeError);
        }
    }

    // Ensure [[FallbackSymbol]] isn't retrieved for Intl.NumberFormat instances.
    for (let thisValue of intlObjects(Intl.NumberFormat)) {
        Object.defineProperty(thisValue, intlFallbackSymbol, {
            get() { assertEq(false, true); }
        });
        numberFormatFunction.call(thisValue);
    }

    // Ensure [[FallbackSymbol]] is only retrieved for objects inheriting from Intl.NumberFormat.prototype.
    for (let thisValue of thisValues(Intl.NumberFormat).filter(IsObject)) {
        if (Intl.NumberFormat.prototype.isPrototypeOf(thisValue))
            continue;
        Object.defineProperty(thisValue, intlFallbackSymbol, {
            get() { assertEq(false, true); }
        });
        assertThrowsInstanceOf(() => numberFormatFunction.call(thisValue), TypeError);
    }

    // Repeat the test from above, but also change Intl.NumberFormat[@@hasInstance]
    // so it always returns |null|.
    for (let thisValue of thisValues(Intl.NumberFormat).filter(IsObject)) {
        let hasInstanceCalled = false, symbolGetterCalled = false;
        Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
            value() {
                assertEq(hasInstanceCalled, false);
                hasInstanceCalled = true;
                return true;
            }, configurable: true
        });
        Object.defineProperty(thisValue, intlFallbackSymbol, {
            get() {
                assertEq(symbolGetterCalled, false);
                symbolGetterCalled = true;
                return null;
            }, configurable: true
        });

        assertThrowsInstanceOf(() => numberFormatFunction.call(thisValue), TypeError);

        delete Intl.NumberFormat[Symbol.hasInstance];

        assertEq(hasInstanceCalled, unwrap);
        assertEq(symbolGetterCalled, unwrap);
    }

    // Test with primitive values.
    for (let thisValue of thisValues(Intl.NumberFormat).filter(IsPrimitive)) {
        // Ensure @@hasInstance is not called.
        Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
            value() { assertEq(true, false); }, configurable: true
        });
        let isUndefinedOrNull = thisValue === undefined || thisValue === null;
        let symbolHolder;
        if (!isUndefinedOrNull) {
            // Ensure the fallback symbol isn't retrieved from the primitive wrapper prototype.
            symbolHolder = Object.getPrototypeOf(thisValue);
            Object.defineProperty(symbolHolder, intlFallbackSymbol, {
                get() { assertEq(true, false); }, configurable: true
            });
        }

        assertThrowsInstanceOf(() => numberFormatFunction.call(thisValue), TypeError);

        delete Intl.NumberFormat[Symbol.hasInstance];
        if (!isUndefinedOrNull)
            delete symbolHolder[intlFallbackSymbol];
    }
}

// Test format() returns the correct result for objects initialized as Intl.NumberFormat instances.
{
    // An actual Intl.NumberFormat instance.
    let numberFormat = new Intl.NumberFormat();

    // An object initialized as a NumberFormat instance.
    let thisValue = Object.create(Intl.NumberFormat.prototype);
    Intl.NumberFormat.call(thisValue);

    // Object with [[FallbackSymbol]] set to NumberFormat instance.
    let fakeObj = {
        __proto__: Intl.NumberFormat.prototype,
        [intlFallbackSymbol]: numberFormat,
    };

    for (let number of [0, 1, 1.5, Infinity, NaN]) {
        let expected = numberFormat.format(number);
        assertEq(thisValue.format(number), expected);
        assertEq(thisValue[intlFallbackSymbol].format(number), expected);
        assertEq(fakeObj.format(number), expected);
    }
}

// Ensure formatToParts() doesn't use the fallback semantics.
{
    let formatToParts = Intl.NumberFormat.prototype.formatToParts;

    // An object initialized as a NumberFormat instance.
    let thisValue = Object.create(Intl.NumberFormat.prototype);
    Intl.NumberFormat.call(thisValue);
    assertThrowsInstanceOf(() => formatToParts.call(thisValue), TypeError);

    // Object with [[FallbackSymbol]] set to NumberFormat instance.
    let fakeObj = {
        __proto__: Intl.NumberFormat.prototype,
        [intlFallbackSymbol]: new Intl.NumberFormat(),
    };
    assertThrowsInstanceOf(() => formatToParts.call(fakeObj), TypeError);
}

// Test resolvedOptions() returns the same results.
{
    // An actual Intl.NumberFormat instance.
    let numberFormat = new Intl.NumberFormat();

    // An object initialized as a NumberFormat instance.
    let thisValue = Object.create(Intl.NumberFormat.prototype);
    Intl.NumberFormat.call(thisValue);

    // Object with [[FallbackSymbol]] set to NumberFormat instance.
    let fakeObj = {
        __proto__: Intl.NumberFormat.prototype,
        [intlFallbackSymbol]: numberFormat,
    };

    function assertEqOptions(actual, expected) {
        actual = Object.entries(actual);
        expected = Object.entries(expected);

        assertEq(actual.length, expected.length, "options count mismatch");
        for (var i = 0; i < expected.length; i++) {
            assertEq(actual[i][0], expected[i][0], "key mismatch at " + i);
            assertEq(actual[i][1], expected[i][1], "value mismatch at " + i);
        }
    }

    let expected = numberFormat.resolvedOptions();
    assertEqOptions(thisValue.resolvedOptions(), expected);
    assertEqOptions(thisValue[intlFallbackSymbol].resolvedOptions(), expected);
    assertEqOptions(fakeObj.resolvedOptions(), expected);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
