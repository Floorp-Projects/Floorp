// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test UnwrapNumberFormat operation.

const numberFormatFunctions = [];
numberFormatFunctions.push(Intl.NumberFormat.prototype.resolvedOptions);
numberFormatFunctions.push(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, "format").get);
// "formatToParts" isn't yet enabled by default.
if ("formatToParts" in Intl.NumberFormat.prototype)
    numberFormatFunctions.push(Intl.NumberFormat.prototype.formatToParts);

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
for (let numberFormatFunction of numberFormatFunctions) {
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
        numberFormatFunction.call({
            __proto__: Intl.NumberFormat.prototype,
            [intlFallbackSymbol]: thisValue,
        });
    }

    // Ensure [[FallbackSymbol]] isn't retrieved for Intl.NumberFormat instances.
    for (let thisValue of intlObjects(Intl.NumberFormat)) {
        Object.defineProperty(thisValue, intlFallbackSymbol, {
            get() { assertEq(false, true); }
        });
        numberFormatFunction.call(thisValue);
    }

    // Ensure [[FallbackSymbol]] is only retrieved for objects inheriting from Intl.NumberFormat.prototype.
    for (let thisValue of thisValues(Intl.NumberFormat)) {
        if (!IsObject(thisValue) || Intl.NumberFormat.prototype.isPrototypeOf(thisValue))
            continue;
        Object.defineProperty(thisValue, intlFallbackSymbol, {
            get() { assertEq(false, true); }
        });
        assertThrowsInstanceOf(() => numberFormatFunction.call(thisValue), TypeError);
    }

    // Repeat the test from above, but also change Intl.NumberFormat[@@hasInstance]
    // so it always returns |null|.
    for (let thisValue of thisValues(Intl.NumberFormat)) {
        let hasInstanceCalled = false, symbolGetterCalled = false;
        Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
            value() {
                assertEq(hasInstanceCalled, false);
                hasInstanceCalled = true;
                return true;
            }, configurable: true
        });
        let isUndefinedOrNull = thisValue !== undefined || thisValue !== null;
        let symbolHolder;
        if (!isUndefinedOrNull) {
            symbolHolder = IsObject(thisValue) ? thisValue : Object.getPrototypeOf(thisValue);
            Object.defineProperty(symbolHolder, intlFallbackSymbol, {
                get() {
                    assertEq(symbolGetterCalled, false);
                    symbolGetterCalled = true;
                    return null;
                }, configurable: true
            });
        }

        assertThrowsInstanceOf(() => numberFormatFunction.call(thisValue), TypeError);

        delete Intl.NumberFormat[Symbol.hasInstance];
        if (!isUndefinedOrNull && !IsObject(thisValue))
            delete symbolHolder[intlFallbackSymbol];

        assertEq(hasInstanceCalled, true);
        assertEq(symbolGetterCalled, !isUndefinedOrNull);
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

// Test formatToParts() returns the correct result for objects initialized as Intl.NumberFormat instances.
if ("formatToParts" in Intl.NumberFormat.prototype) {
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

    function assertEqParts(actual, expected) {
        assertEq(actual.length, expected.length, "parts count mismatch");
        for (var i = 0; i < expected.length; i++) {
            assertEq(actual[i].type, expected[i].type, "type mismatch at " + i);
            assertEq(actual[i].value, expected[i].value, "value mismatch at " + i);
        }
    }

    for (let number of [0, 1, 1.5, Infinity, NaN]) {
        let expected = numberFormat.formatToParts(number);
        assertEqParts(thisValue.formatToParts(number), expected);
        assertEqParts(thisValue[intlFallbackSymbol].formatToParts(number), expected);
        assertEqParts(fakeObj.formatToParts(number), expected);
    }
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
