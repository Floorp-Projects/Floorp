load(libdir + "asserts.js");

function test() {
    var plainDataProperty = {
        value: 0, enumerable: true, configurable: true, writable: true
    };
    var nonEnumerableProperty = {
        value: 0, enumerable: false, configurable: true, writable: true
    };
    var nonConfigurableProperty = {
        value: 0, enumerable: true, configurable: false, writable: true
    };
    var n = 5000;
    for (var i = 0; i < n; ++i) {
        var obj = {};

        // Create a different shape for each object to ensure JSOP_INITELEM
        // below can get into the megamorphic state.
        Object.defineProperty(obj, "bar" + i, nonEnumerableProperty);

        // Plain data property, will be created through _DefineDataProperty,
        // which is emitted as JSOP_INITELEM. The object doesn't have a "foo"
        // property, so JSOP_INITELEM can simply create a new property.
        Object.defineProperty(obj, "foo", plainDataProperty);

        // Redefine as a non-data property for the last object.
        var desc = (i + 1 !== n) ? plainDataProperty : nonConfigurableProperty;
        Object.defineProperty(obj, "foo", desc);

        // Again JSOP_INITELEM, but this time JSOP_INITELEM can't simply add a
        // property, because "foo" is already present. And for the last object,
        // which has a non-configurable "foo" property, this call must throw a
        // TypeError exception.
        Object.defineProperty(obj, "foo", plainDataProperty);
    }
}

for (var i = 0; i < 2; ++i) {
    assertThrowsInstanceOf(test, TypeError);
}
