function strictArgs() {
    return (function (a, b, c) {'use strict'; return arguments; })(1, 2);
}

function normalArgs() {
    return (function (a, b, c) { return arguments; })(1, 2);
}

function checkProperty(options, prop, shouldThrow) {
    var desc, orig;
    var obj = options.strict ? strictArgs() : normalArgs();
    var objType = options.strict ? "strict arguments." : "normal arguments.";

    function check() {
        orig = Object.getOwnPropertyDescriptor(obj, prop);

        var threw = false;
        try {
            obj[prop] = obj[prop];
        }
        catch (e) {
            threw = true;
        }
        assertEq(threw, shouldThrow, objType + prop + " threw");

        if (orig === undefined) {
            // The property wasn't defined, so we can skip it.
            return;
        }

        desc = Object.getOwnPropertyDescriptor(obj, prop);
        if ("value" in orig) {
            assertEq(desc.value, orig.value, objType + prop + " value");
        } else {
            assertEq(desc.get, orig.get, objType + prop + " get");
            assertEq(desc.set, orig.set, objType + prop + " set");
        }
        assertEq(desc.writable, orig.writable, objType + prop + " writable");
        assertEq(desc.enumerable, orig.enumerable, objType + prop + " enumerable");
        assertEq(desc.configurable, orig.configurable, objType + prop + " configurable");
    }

    check();

    if (orig && orig.configurable) {
        if(options.refresh) { obj = options.strict ? strictArgs() : normalArgs(); }
        Object.defineProperty(obj, prop, {writable: false, enumerable: true});
        check();

        if(options.refresh) { obj = options.strict ? strictArgs() : normalArgs(); }
        Object.defineProperty(obj, prop, {writable: true, enumerable: false});
        check();

        if(options.refresh) { obj = options.strict ? strictArgs() : normalArgs(); }
        Object.defineProperty(obj, prop, {writable: false, configurable: false});
        check();
    }
}

checkProperty({strict: true, refresh: true}, 'callee', true);
checkProperty({strict: true, refresh: false}, 'callee', true);
checkProperty({strict: false, refresh: true}, 'callee', false);
checkProperty({strict: false, refresh: false}, 'callee', false);

checkProperty({strict: true, refresh: true}, 'length', false);
checkProperty({strict: true, refresh: false}, 'length', false);
checkProperty({strict: false, refresh: true}, 'length', false);
checkProperty({strict: false, refresh: false}, 'length', false);

for (var i = 0; i <= 5; i++) {
    checkProperty({strict: true, refresh: true}, "" + i, false);
    checkProperty({strict: true, refresh: false}, "" + i, false);
    checkProperty({strict: false, refresh: true}, "" + i, false);
    checkProperty({strict: false, refresh: false}, "" + i, false);
}
