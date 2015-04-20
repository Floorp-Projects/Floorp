/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES6 draft rev36 2015-03-17 19.1.2.1
function ObjectStaticAssign(target, firstSource) {
    // Steps 1-2.
    var to = ToObject(target);

    // Step 3.
    if (arguments.length < 2)
        return to;

    // Steps 4-5.
    for (var i = 1; i < arguments.length; i++) {
        // Step 5.a.
        var nextSource = arguments[i];
        if (nextSource === null || nextSource === undefined)
            continue;

        // Steps 5.b.i-ii.
        var from = ToObject(nextSource);

        // Steps 5.b.iii-iv.
        var keys = OwnPropertyKeys(from);

        // Step 5.c.
        for (var nextIndex = 0, len = keys.length; nextIndex < len; nextIndex++) {
            var nextKey = keys[nextIndex];

            // Steps 5.c.i-iii. We abbreviate this by calling propertyIsEnumerable
            // which is faster and returns false for not defined properties.
            if (callFunction(std_Object_propertyIsEnumerable, from, nextKey)) {
                // Steps 5.c.iii.1-4.
                to[nextKey] = from[nextKey];
            }
        }
    }

    // Step 6.
    return to;
}

function ObjectDefineSetter(name, setter) {
    var object;
    if (this === null || this === undefined)
        object = global;
    else
        object = ToObject(this);

    if (!IsCallable(setter))
        ThrowTypeError(JSMSG_BAD_GETTER_OR_SETTER, "setter");

    var key = ToPropertyKey(name);

    var desc = {
        __proto__: null,
        enumerable: true,
        configurable: true,
        set: setter
    };

    std_Object_defineProperty(object, key, desc);
}

function ObjectDefineGetter(name, getter) {
    var object;
    if (this === null || this === undefined)
        object = global;
    else
        object = ToObject(this);

    if (!IsCallable(getter))
        ThrowTypeError(JSMSG_BAD_GETTER_OR_SETTER, "getter");

    var key = ToPropertyKey(name);

    var desc = {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get: getter
    };

    std_Object_defineProperty(object, key, desc);
}

function ObjectLookupSetter(name) {
    var key = ToPropertyKey(name);
    var object = ToObject(this);

    do {
        var desc = std_Object_getOwnPropertyDescriptor(object, key);
        if (desc) {
            if (callFunction(std_Object_hasOwnProperty, desc, "set"))
                return desc.set;
            return undefined;
        }
        object = std_Object_getPrototypeOf(object);
    } while (object !== null);
}

function ObjectLookupGetter(name) {
    var key = ToPropertyKey(name);
    var object = ToObject(this);

    do {
        var desc = std_Object_getOwnPropertyDescriptor(object, key);
        if (desc) {
            if (callFunction(std_Object_hasOwnProperty, desc, "get"))
                return desc.get;
            return undefined;
        }
        object = std_Object_getPrototypeOf(object);
    } while (object !== null);
}
