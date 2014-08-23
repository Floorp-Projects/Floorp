/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function check(obj, name, value, readonly) {
    // Start with a non-configurable, writable data property implemented via
    // js::PropertyOp getter and setter.
    var pd = Object.getOwnPropertyDescriptor(obj, name);

    assertEq(pd.configurable, false, "non-configurable " + name);
    assertEq(pd.writable, !readonly, "writable " + name);

    try {
        // Do not allow a transition from js::PropertyOp to writable js::Value
        // data property.
        Object.defineProperty(obj, name, {writable: true});
        assertEq(0, 1);
    } catch (e) {
        assertEq('' + e, "TypeError: can't redefine non-configurable property '" + name + "'");
    }

    if (!readonly) {
        try {
            // Do not allow the property denoted by name to become a true data
            // property via a descriptor that preserves the native property's
            // writable attribute.
            Object.defineProperty(obj, name, {value: value});
            assertEq(0, 1);
        } catch (e) {
            assertEq('' + e, "TypeError: can't redefine non-configurable property '" + name + "'");
        }
    }

    try {
        // Do not allow the property to be frozen at some bogus value.
        Object.defineProperty(obj, name, {value: "bogus", writable: false});
        assertEq(0, 1);
    } catch (e) {
        assertEq('' + e, "TypeError: can't redefine non-configurable property '" + name + "'");
    }

    // Now make name non-writable.
    Object.defineProperty(obj, name, {writable: false})

    // Assert that the right getter result "stuck".
    assertEq(obj[name], value);

    // Test that it is operationally non-writable now.
    obj[name] = "eek!";
    assertEq(obj[name], value);
}

// Reset RegExp.leftContext to the empty string.
/x/.test('x');

var d = Object.getOwnPropertyDescriptor(RegExp, "leftContext");
assertEq(d.set, undefined);
assertEq(typeof d.get, "function");
assertEq(d.enumerable, true);
assertEq(d.configurable, false);
assertEq(d.get.call(RegExp), "");

reportCompare(0, 0, "ok");
