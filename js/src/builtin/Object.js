/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        if (typeof desc !== "undefined")
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

// ES 2017 draft bb96899bb0d9ef9be08164a26efae2ee5f25e875 19.1.3.7
function Object_valueOf() {
    // Step 1.
    return ToObject(this);
}

// ES 2018 draft 19.1.3.2
function Object_hasOwnProperty(V) {
    // Implement hasOwnProperty as a pseudo function that becomes a JSOp
    // to easier add an inline cache for this.
    return hasOwn(V, this);
}

// ES7 draft (2016 March 8) B.2.2.3
function ObjectDefineSetter(name, setter) {
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
            if (hasOwn("set", desc))
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
            if (hasOwn("get", desc))
                return desc.get;

            // Step.b.ii.
            return undefined;
        }

        // Step 3.c.
        object = std_Reflect_getPrototypeOf(object);
    } while (object !== null);

    // Step 3.d. (implicit)
}
