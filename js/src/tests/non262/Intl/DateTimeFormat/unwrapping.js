// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test UnwrapDateTimeFormat operation.

const dateTimeFormatFunctions = [];
dateTimeFormatFunctions.push({
    function: Intl.DateTimeFormat.prototype.resolvedOptions,
    unwrap: true,
});
dateTimeFormatFunctions.push({
    function: Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, "format").get,
    unwrap: true,
});
dateTimeFormatFunctions.push({
    function: Intl.DateTimeFormat.prototype.formatToParts,
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

const intlFallbackSymbol = Object.getOwnPropertySymbols(Intl.DateTimeFormat.call(Object.create(Intl.DateTimeFormat.prototype)))[0];

// Test Intl.DateTimeFormat.prototype methods.
for (let {function: dateTimeFormatFunction, unwrap} of dateTimeFormatFunctions) {
    // Test a TypeError is thrown when the this-value isn't an initialized
    // Intl.DateTimeFormat instance.
    for (let thisValue of thisValues(Intl.DateTimeFormat)) {
        assertThrowsInstanceOf(() => dateTimeFormatFunction.call(thisValue), TypeError);
    }

    // And test no error is thrown for initialized Intl.DateTimeFormat instances.
    for (let thisValue of intlObjects(Intl.DateTimeFormat)) {
        dateTimeFormatFunction.call(thisValue);
    }

    // Manually add [[FallbackSymbol]] to objects and then repeat the tests from above.
    for (let thisValue of thisValues(Intl.DateTimeFormat)) {
        assertThrowsInstanceOf(() => dateTimeFormatFunction.call({
            __proto__: Intl.DateTimeFormat.prototype,
            [intlFallbackSymbol]: thisValue,
        }), TypeError);
    }

    for (let thisValue of intlObjects(Intl.DateTimeFormat)) {
        let obj = {
            __proto__: Intl.DateTimeFormat.prototype,
            [intlFallbackSymbol]: thisValue,
        };
        if (unwrap) {
            dateTimeFormatFunction.call(obj);
        } else {
            assertThrowsInstanceOf(() => dateTimeFormatFunction.call(obj), TypeError);
        }
    }

    // Ensure [[FallbackSymbol]] isn't retrieved for Intl.DateTimeFormat instances.
    for (let thisValue of intlObjects(Intl.DateTimeFormat)) {
        Object.defineProperty(thisValue, intlFallbackSymbol, {
            get() { assertEq(false, true); }
        });
        dateTimeFormatFunction.call(thisValue);
    }

    // Ensure [[FallbackSymbol]] is only retrieved for objects inheriting from Intl.DateTimeFormat.prototype.
    for (let thisValue of thisValues(Intl.DateTimeFormat).filter(IsObject)) {
        if (Intl.DateTimeFormat.prototype.isPrototypeOf(thisValue))
            continue;
        Object.defineProperty(thisValue, intlFallbackSymbol, {
            get() { assertEq(false, true); }
        });
        assertThrowsInstanceOf(() => dateTimeFormatFunction.call(thisValue), TypeError);
    }

    // Repeat the test from above, but also change Intl.DateTimeFormat[@@hasInstance]
    // so it always returns |null|.
    for (let thisValue of thisValues(Intl.DateTimeFormat).filter(IsObject)) {
        let hasInstanceCalled = false, symbolGetterCalled = false;
        Object.defineProperty(Intl.DateTimeFormat, Symbol.hasInstance, {
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

        assertThrowsInstanceOf(() => dateTimeFormatFunction.call(thisValue), TypeError);

        delete Intl.DateTimeFormat[Symbol.hasInstance];

        assertEq(hasInstanceCalled, unwrap);
        assertEq(symbolGetterCalled, unwrap);
    }

    // Test with primitive values.
    for (let thisValue of thisValues(Intl.DateTimeFormat).filter(IsPrimitive)) {
        // Ensure @@hasInstance is not called.
        Object.defineProperty(Intl.DateTimeFormat, Symbol.hasInstance, {
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

        assertThrowsInstanceOf(() => dateTimeFormatFunction.call(thisValue), TypeError);

        delete Intl.DateTimeFormat[Symbol.hasInstance];
        if (!isUndefinedOrNull)
            delete symbolHolder[intlFallbackSymbol];
    }
}

// Test format() returns the correct result for objects initialized as Intl.DateTimeFormat instances.
{
    // An actual Intl.DateTimeFormat instance.
    let dateTimeFormat = new Intl.DateTimeFormat();

    // An object initialized as a DateTimeFormat instance.
    let thisValue = Object.create(Intl.DateTimeFormat.prototype);
    Intl.DateTimeFormat.call(thisValue);

    // Object with [[FallbackSymbol]] set to DateTimeFormat instance.
    let fakeObj = {
        __proto__: Intl.DateTimeFormat.prototype,
        [intlFallbackSymbol]: dateTimeFormat,
    };

    for (let number of [0, Date.now(), -Date.now()]) {
        let expected = dateTimeFormat.format(number);
        assertEq(thisValue.format(number), expected);
        assertEq(thisValue[intlFallbackSymbol].format(number), expected);
        assertEq(fakeObj.format(number), expected);
    }
}

// Ensure formatToParts() doesn't use the fallback semantics.
{
    let formatToParts = Intl.DateTimeFormat.prototype.formatToParts;

    // An object initialized as a DateTimeFormat instance.
    let thisValue = Object.create(Intl.DateTimeFormat.prototype);
    Intl.DateTimeFormat.call(thisValue);
    assertThrowsInstanceOf(() => formatToParts.call(thisValue), TypeError);

    // Object with [[FallbackSymbol]] set to DateTimeFormat instance.
    let fakeObj = {
        __proto__: Intl.DateTimeFormat.prototype,
        [intlFallbackSymbol]: new Intl.DateTimeFormat(),
    };
    assertThrowsInstanceOf(() => formatToParts.call(fakeObj), TypeError);
}

// Test resolvedOptions() returns the same results.
{
    // An actual Intl.DateTimeFormat instance.
    let dateTimeFormat = new Intl.DateTimeFormat();

    // An object initialized as a DateTimeFormat instance.
    let thisValue = Object.create(Intl.DateTimeFormat.prototype);
    Intl.DateTimeFormat.call(thisValue);

    // Object with [[FallbackSymbol]] set to DateTimeFormat instance.
    let fakeObj = {
        __proto__: Intl.DateTimeFormat.prototype,
        [intlFallbackSymbol]: dateTimeFormat,
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

    let expected = dateTimeFormat.resolvedOptions();
    assertEqOptions(thisValue.resolvedOptions(), expected);
    assertEqOptions(thisValue[intlFallbackSymbol].resolvedOptions(), expected);
    assertEqOptions(fakeObj.resolvedOptions(), expected);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
