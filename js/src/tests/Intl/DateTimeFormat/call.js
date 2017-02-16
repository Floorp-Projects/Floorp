// |reftest| skip-if(!this.hasOwnProperty("Intl"))

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

function thisValues() {
    const intlConstructors = Object.getOwnPropertyNames(Intl).map(name => Intl[name]).filter(IsConstructor);

    return [
        // Primitive values.
        ...[undefined, null, true, "abc", Symbol(), 123],

        // Object values.
        ...[{}, [], /(?:)/, function(){}, new Proxy({}, {})],

        // Intl objects.
        ...[].concat(...intlConstructors.map(ctor => [
            // Instance of an Intl constructor.
            new ctor(),

            // Instance of a subclassed Intl constructor.
            new class extends ctor {},

            // Object inheriting from an Intl constructor prototype.
            Object.create(ctor.prototype),

            // Intl object not inheriting from its default prototype.
            Object.setPrototypeOf(new ctor(), Object.prototype),
        ])),
    ];
}

const intlFallbackSymbol = Object.getOwnPropertySymbols(Intl.DateTimeFormat.call(Object.create(Intl.DateTimeFormat.prototype)))[0];

// Invoking [[Call]] for Intl.DateTimeFormat returns a new instance unless called
// with an instance inheriting from Intl.DateTimeFormat.prototype.
for (let thisValue of thisValues()) {
    let obj = Intl.DateTimeFormat.call(thisValue);

    if (!Intl.DateTimeFormat.prototype.isPrototypeOf(thisValue)) {
        assertEq(Object.is(obj, thisValue), false);
        assertEq(obj instanceof Intl.DateTimeFormat, true);
        if (IsObject(thisValue))
            assertEqArray(Object.getOwnPropertySymbols(thisValue), []);
    } else {
        assertEq(Object.is(obj, thisValue), true);
        assertEq(obj instanceof Intl.DateTimeFormat, true);
        assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);
    }
}

// Intl.DateTimeFormat uses the legacy Intl constructor compromise semantics.
// - Test when InstanceofOperator(thisValue, %DateTimeFormat%) returns true.
for (let thisValue of thisValues().filter(IsObject)) {
    let hasInstanceCalled = false;
    Object.defineProperty(Intl.DateTimeFormat, Symbol.hasInstance, {
        value() {
            assertEq(hasInstanceCalled, false);
            hasInstanceCalled = true;
            return true;
        }, configurable: true
    });
    let obj = Intl.DateTimeFormat.call(thisValue);
    delete Intl.DateTimeFormat[Symbol.hasInstance];

    assertEq(Object.is(obj, thisValue), true);
    assertEq(hasInstanceCalled, true);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);
}
// - Test when InstanceofOperator(thisValue, %DateTimeFormat%) returns false.
for (let thisValue of thisValues().filter(IsObject)) {
    let hasInstanceCalled = false;
    Object.defineProperty(Intl.DateTimeFormat, Symbol.hasInstance, {
        value() {
            assertEq(hasInstanceCalled, false);
            hasInstanceCalled = true;
            return false;
        }, configurable: true
    });
    let obj = Intl.DateTimeFormat.call(thisValue);
    delete Intl.DateTimeFormat[Symbol.hasInstance];

    assertEq(Object.is(obj, thisValue), false);
    assertEq(obj instanceof Intl.DateTimeFormat, true);
    assertEq(hasInstanceCalled, true);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), []);
}
// - Test with primitive values.
for (let thisValue of thisValues().filter(IsPrimitive)) {
    // Ensure @@hasInstance is not called.
    Object.defineProperty(Intl.DateTimeFormat, Symbol.hasInstance, {
        value() { assertEq(true, false); }, configurable: true
    });
    let obj = Intl.DateTimeFormat.call(thisValue);
    delete Intl.DateTimeFormat[Symbol.hasInstance];

    assertEq(Object.is(obj, thisValue), false);
    assertEq(obj instanceof Intl.DateTimeFormat, true);
}

// Throws an error when attempting to install [[FallbackSymbol]] twice.
{
    let thisValue = Object.create(Intl.DateTimeFormat.prototype);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), []);

    assertEq(Intl.DateTimeFormat.call(thisValue), thisValue);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);

    assertThrowsInstanceOf(() => Intl.DateTimeFormat.call(thisValue), TypeError);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);
}

// Throws an error when the thisValue is non-extensible.
{
    let thisValue = Object.create(Intl.DateTimeFormat.prototype);
    Object.preventExtensions(thisValue);

    assertThrowsInstanceOf(() => Intl.DateTimeFormat.call(thisValue), TypeError);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), []);
}

// [[FallbackSymbol]] is installed as a frozen property holding an Intl.DateTimeFormat instance.
{
    let thisValue = Object.create(Intl.DateTimeFormat.prototype);
    Intl.DateTimeFormat.call(thisValue);

    let desc = Object.getOwnPropertyDescriptor(thisValue, intlFallbackSymbol);
    assertEq(desc !== undefined, true);
    assertEq(desc.writable, false);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, false);
    assertEq(desc.value instanceof Intl.DateTimeFormat, true);
}

// Ensure [[FallbackSymbol]] is installed last by changing the [[Prototype]]
// during initialization.
{
    let thisValue = {};
    let options = {
        get hour12() {
            Object.setPrototypeOf(thisValue, Intl.DateTimeFormat.prototype);
            return false;
        }
    };
    let obj = Intl.DateTimeFormat.call(thisValue, undefined, options);
    assertEq(Object.is(obj, thisValue), true);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), [intlFallbackSymbol]);
}
{
    let thisValue = Object.create(Intl.DateTimeFormat.prototype);
    let options = {
        get hour12() {
            Object.setPrototypeOf(thisValue, Object.prototype);
            return false;
        }
    };
    let obj = Intl.DateTimeFormat.call(thisValue, undefined, options);
    assertEq(Object.is(obj, thisValue), false);
    assertEqArray(Object.getOwnPropertySymbols(thisValue), []);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
