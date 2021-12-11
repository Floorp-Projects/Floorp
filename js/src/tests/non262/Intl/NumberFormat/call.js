// |reftest| skip-if(!this.hasOwnProperty("Intl"))

function IsIntlService(c) {
    return typeof c === "function" &&
           c.hasOwnProperty("prototype") &&
           c.prototype.hasOwnProperty("resolvedOptions");
}

function IsObject(o) {
    return Object(o) === o;
}

function IsPrimitive(o) {
    return Object(o) !== o;
}

function thisValues() {
    const intlConstructors = Object.getOwnPropertyNames(Intl).map(name => Intl[name]).filter(IsIntlService);

    return [
        // Primitive values.
        ...[undefined, null, true, "abc", Symbol(), 123],

        // Object values.
        ...[{}, [], /(?:)/, function(){}, new Proxy({}, {})],

        // Intl objects.
        ...[].concat(...intlConstructors.map(ctor => {
            let args = [];
            if (ctor === Intl.DisplayNames) {
                // Intl.DisplayNames can't be constructed without any arguments.
                args = [undefined, {type: "language"}];
            }

            return [
                // Instance of an Intl constructor.
                new ctor(...args),

                // Instance of a subclassed Intl constructor.
                new class extends ctor {}(...args),

                // Object inheriting from an Intl constructor prototype.
                Object.create(ctor.prototype),

                // Intl object not inheriting from its default prototype.
                Object.setPrototypeOf(new ctor(...args), Object.prototype),
            ];
        })),
    ];
}

const intlFallbackSymbol = Object.getOwnPropertySymbols(Intl.NumberFormat.call(Object.create(Intl.NumberFormat.prototype)))[0];

// Invoking [[Call]] for Intl.NumberFormat returns a new instance unless called
// with an instance inheriting from Intl.NumberFormat.prototype.
for (let thisValue of thisValues()) {
    let obj = Intl.NumberFormat.call(thisValue);

    if (!Intl.NumberFormat.prototype.isPrototypeOf(thisValue)) {
        assertEq(Object.is(obj, thisValue), false);
        assertEq(obj instanceof Intl.NumberFormat, true);
        if (IsObject(thisValue))
            assertEqArray(Object.getOwnPropertySymbols(thisValue), []);
    } else {
        assertEq(Object.is(obj, thisValue), true);
        assertEq(obj instanceof Intl.NumberFormat, true);
        assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);
    }
}

// Intl.NumberFormat uses the legacy Intl constructor compromise semantics.
// - Test when InstanceofOperator(thisValue, %NumberFormat%) returns true.
for (let thisValue of thisValues().filter(IsObject)) {
    let isPrototypeOf = Intl.NumberFormat.prototype.isPrototypeOf(thisValue);
    let hasInstanceCalled = false;
    Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
        value() {
            assertEq(hasInstanceCalled, false);
            hasInstanceCalled = true;
            return true;
        }, configurable: true
    });
    let obj = Intl.NumberFormat.call(thisValue);
    delete Intl.NumberFormat[Symbol.hasInstance];

    assertEq(Object.is(obj, thisValue), isPrototypeOf);
    assertEq(hasInstanceCalled, false);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), isPrototypeOf ? [intlFallbackSymbol] : []);
}
// - Test when InstanceofOperator(thisValue, %NumberFormat%) returns false.
for (let thisValue of thisValues().filter(IsObject)) {
    let isPrototypeOf = Intl.NumberFormat.prototype.isPrototypeOf(thisValue);
    let hasInstanceCalled = false;
    Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
        value() {
            assertEq(hasInstanceCalled, false);
            hasInstanceCalled = true;
            return false;
        }, configurable: true
    });
    let obj = Intl.NumberFormat.call(thisValue);
    delete Intl.NumberFormat[Symbol.hasInstance];

    assertEq(Object.is(obj, thisValue), isPrototypeOf);
    assertEq(obj instanceof Intl.NumberFormat, true);
    assertEq(hasInstanceCalled, false);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), isPrototypeOf ? [intlFallbackSymbol] : []);
}
// - Test with primitive values.
for (let thisValue of thisValues().filter(IsPrimitive)) {
    // Ensure @@hasInstance is not called.
    Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
        value() { assertEq(true, false); }, configurable: true
    });
    let obj = Intl.NumberFormat.call(thisValue);
    delete Intl.NumberFormat[Symbol.hasInstance];

    assertEq(Object.is(obj, thisValue), false);
    assertEq(obj instanceof Intl.NumberFormat, true);
}

// Throws an error when attempting to install [[FallbackSymbol]] twice.
{
    let thisValue = Object.create(Intl.NumberFormat.prototype);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), []);

    assertEq(Intl.NumberFormat.call(thisValue), thisValue);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);

    assertThrowsInstanceOf(() => Intl.NumberFormat.call(thisValue), TypeError);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);
}

// Throws an error when the thisValue is non-extensible.
{
    let thisValue = Object.create(Intl.NumberFormat.prototype);
    Object.preventExtensions(thisValue);

    assertThrowsInstanceOf(() => Intl.NumberFormat.call(thisValue), TypeError);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), []);
}

// [[FallbackSymbol]] is installed as a frozen property holding an Intl.NumberFormat instance.
{
    let thisValue = Object.create(Intl.NumberFormat.prototype);
    Intl.NumberFormat.call(thisValue);

    let desc = Object.getOwnPropertyDescriptor(thisValue, intlFallbackSymbol);
    assertEq(desc !== undefined, true);
    assertEq(desc.writable, false);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, false);
    assertEq(desc.value instanceof Intl.NumberFormat, true);
}

// Ensure [[FallbackSymbol]] is installed last by changing the [[Prototype]]
// during initialization.
{
    let thisValue = {};
    let options = {
        get useGrouping() {
            Object.setPrototypeOf(thisValue, Intl.NumberFormat.prototype);
            return false;
        }
    };
    let obj = Intl.NumberFormat.call(thisValue, undefined, options);
    assertEq(Object.is(obj, thisValue), true);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);
}
{
    let thisValue = Object.create(Intl.NumberFormat.prototype);
    let options = {
        get useGrouping() {
            Object.setPrototypeOf(thisValue, Object.prototype);
            return false;
        }
    };
    let obj = Intl.NumberFormat.call(thisValue, undefined, options);
    assertEq(Object.is(obj, thisValue), false);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), []);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
