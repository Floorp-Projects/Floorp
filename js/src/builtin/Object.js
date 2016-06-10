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
        var keys = OwnPropertyKeys(from, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS);

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

// ES stage 4 proposal
function ObjectGetOwnPropertyDescriptors(O) {
    // Step 1.
    var obj = ToObject(O);

    // Step 2.
    var keys = OwnPropertyKeys(obj, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS);

    // Step 3.
    var descriptors = {};

    // Step 4.
    for (var index = 0, len = keys.length; index < len; index++) {
        var key = keys[index];

        // Steps 4.a-b.
        var desc = std_Object_getOwnPropertyDescriptor(obj, key);

        // Step 4.c.
        _DefineDataProperty(descriptors, key, desc);
    }

    // Step 5.
    return descriptors;
}

/* ES6 draft rev 32 (2015 Feb 2) 19.1.2.9. */
function ObjectGetPrototypeOf(obj) {
    return std_Reflect_getPrototypeOf(ToObject(obj));
}

/* ES6 draft rev 32 (2015 Feb 2) 19.1.2.11. */
function ObjectIsExtensible(obj) {
    return IsObject(obj) && std_Reflect_isExtensible(obj);
}

/* ES2015 19.1.3.5 Object.prototype.toLocaleString */
function Object_toLocaleString() {
    // Step 1.
    var O = this;

    // Step 2.
    return callContentFunction(O.toString, O);
}

// ES7 draft (2016 March 8) B.2.2.3
function ObjectDefineSetter(name, setter) {
    if (this === null || this === undefined)
        AddContentTelemetry(TELEMETRY_DEFINE_GETTER_SETTER_THIS_NULL_UNDEFINED, 1);
    else
        AddContentTelemetry(TELEMETRY_DEFINE_GETTER_SETTER_THIS_NULL_UNDEFINED, 0);

    // Step 1.
    var object = ToObject(this);

    // Step 2.
    if (!IsCallable(setter))
        ThrowTypeError(JSMSG_BAD_GETTER_OR_SETTER, "setter");

    // Step 3.
    var desc = {
        __proto__: null,
        enumerable: true,
        configurable: true,
        set: setter
    };

    // Step 4.
    var key = ToPropertyKey(name);

    // Step 5.
    std_Object_defineProperty(object, key, desc);

    // Step 6. (implicit)
}

// ES7 draft (2016 March 8) B.2.2.2
function ObjectDefineGetter(name, getter) {
    if (this === null || this === undefined)
        AddContentTelemetry(TELEMETRY_DEFINE_GETTER_SETTER_THIS_NULL_UNDEFINED, 1);
    else
        AddContentTelemetry(TELEMETRY_DEFINE_GETTER_SETTER_THIS_NULL_UNDEFINED, 0);

    // Step 1.
    var object = ToObject(this);

    // Step 2.
    if (!IsCallable(getter))
        ThrowTypeError(JSMSG_BAD_GETTER_OR_SETTER, "getter");

    // Step 3.
    var desc = {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get: getter
    };

    // Step 4.
    var key = ToPropertyKey(name);

    // Step 5.
    std_Object_defineProperty(object, key, desc);

    // Step 6. (implicit)
}

// ES7 draft (2016 March 8) B.2.2.5
function ObjectLookupSetter(name) {
    // Step 1.
    var object = ToObject(this);

    // Step 2.
    var key = ToPropertyKey(name)

    do {
        // Step 3.a.
        var desc = std_Object_getOwnPropertyDescriptor(object, key);

        // Step 3.b.
        if (desc) {
            // Step.b.i.
            if (callFunction(std_Object_hasOwnProperty, desc, "set"))
                return desc.set;

            // Step.b.ii.
            return undefined;
        }

        // Step 3.c.
        object = std_Reflect_getPrototypeOf(object);
    } while (object !== null);

    // Step 3.d. (implicit)
}

// ES7 draft (2016 March 8) B.2.2.4
function ObjectLookupGetter(name) {
    // Step 1.
    var object = ToObject(this);

    // Step 2.
    var key = ToPropertyKey(name)

    do {
        // Step 3.a.
        var desc = std_Object_getOwnPropertyDescriptor(object, key);

        // Step 3.b.
        if (desc) {
            // Step.b.i.
            if (callFunction(std_Object_hasOwnProperty, desc, "get"))
                return desc.get;

            // Step.b.ii.
            return undefined;
        }

        // Step 3.c.
        object = std_Reflect_getPrototypeOf(object);
    } while (object !== null);

    // Step 3.d. (implicit)
}
