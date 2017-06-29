var g_foo = "foo";
var g_bar = "bar";

// Define base class with a read-only and a writable data property
class Base
{
}
Object.defineProperty(Base.prototype, "foo", { value: "Base", writable: true });
Object.defineProperty(Base.prototype, "bar", { value: "Base", writable: false });

// Test various cases that should throw during SETPROP_SUPER
class Derived extends Base
{
    // ECMA-2018 9.1.9.1, step 4.a
    testReadonly() {
        super.bar = "Derived";
    }
    testReadonlyElem() {
        super[g_bar] = "Derived";
    }

    // ECMA-2018 9.1.9.1, step 4.b
    testPrimitiveReceiver() {
        super.foo = "Derived";
    }
    testPrimitiveReceiverElem() {
        super[g_foo] = "Derived";
    }

    // ECMA-2018 9.1.9.1, step 4.d.i
    testAccessorShadow() {
        Object.defineProperty(this, "foo", { get: function() { } });
        super.foo = "Derived";
    }
    testAccessorShadowElem() {
        Object.defineProperty(this, "foo", { get: function() { } });
        super[g_foo] = "Derived";
    }

    // ECMA-2018 9.1.9.1, step 4.d.ii
    testReadonlyShadow() {
        Object.defineProperty(this, "foo", { writable: false });
        super.foo = "Derived";
    }
    testReadonlyShadowElem() {
        Object.defineProperty(this, "foo", { writable: false });
        super[g_foo] = "Derived";
    }
}

for (let i = 0; i < 10; ++i) {
    var cnt = 0;

    try { new Derived().testReadonly(); } catch(e) { cnt++; }
    try { new Derived().testReadonlyElem(); } catch(e) { cnt++; }
    try { Derived.prototype.testPrimitiveReceiver.call(null); } catch(e) { cnt++; }
    try { Derived.prototype.testPrimitiveReceiverElem.call(null); } catch(e) { cnt++; }
    try { new Derived().testAccessorShadow(); } catch(e) { cnt++; }
    try { new Derived().testAccessorShadowElem(); } catch(e) { cnt++; }
    try { new Derived().testReadonlyShadow(); } catch(e) { cnt++; }
    try { new Derived().testReadonlyShadowElem(); } catch(e) { cnt++; }

    assertEq(cnt, 8);
}
