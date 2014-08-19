/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


if (typeof assertThrowsInstanceOf === 'undefined') {
    var assertThrowsInstanceOf = function assertThrowsInstanceOf(f, ctor, msg) {
        var fullmsg;
        try {
            f();
        } catch (exc) {
            if (exc instanceof ctor)
                return;
            fullmsg = "Assertion failed: expected exception " + ctor.name + ", got " + exc;
        }
        if (fullmsg === undefined)
            fullmsg = "Assertion failed: expected exception " + ctor.name + ", no exception thrown";
        if (msg !== undefined)
            fullmsg += " - " + msg;
        throw new Error(fullmsg);
    };
}

if (typeof assertThrowsValue === 'undefined') {
    var assertThrowsValue = function assertThrowsValue(f, val, msg) {
        var fullmsg;
        try {
            f();
        } catch (exc) {
            if ((exc === val) === (val === val) && (val !== 0 || 1 / exc === 1 / val))
                return;
            fullmsg = "Assertion failed: expected exception " + val + ", got " + exc;
        }
        if (fullmsg === undefined)
            fullmsg = "Assertion failed: expected exception " + val + ", no exception thrown";
        if (msg !== undefined)
            fullmsg += " - " + msg;
        throw new Error(fullmsg);
    };
}

if (typeof assertDeepEq === 'undefined') {
    var assertDeepEq = (function(){
        var call = Function.prototype.call,
            Map_ = Map,
            Error_ = Error,
            Map_has = call.bind(Map.prototype.has),
            Map_get = call.bind(Map.prototype.get),
            Map_set = call.bind(Map.prototype.set),
            Object_toString = call.bind(Object.prototype.toString),
            Function_toString = call.bind(Function.prototype.toString),
            Object_getPrototypeOf = Object.getPrototypeOf,
            Object_hasOwnProperty = call.bind(Object.prototype.hasOwnProperty),
            Object_getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor,
            Object_isExtensible = Object.isExtensible,
            Object_getOwnPropertyNames = Object.getOwnPropertyNames,
            uneval_ = uneval;

        // Return true iff ES6 Type(v) isn't Object.
        // Note that `typeof document.all === "undefined"`.
        function isPrimitive(v) {
            return (v === null ||
                    v === undefined ||
                    typeof v === "boolean" ||
                    typeof v === "number" ||
                    typeof v === "string" ||
                    typeof v === "symbol");
        }

        function assertSameValue(a, b, msg) {
            try {
                assertEq(a, b);
            } catch (exc) {
                throw Error_(exc.message + (msg ? " " + msg : ""));
            }
        }

        function assertSameClass(a, b, msg) {
            var ac = Object_toString(a), bc = Object_toString(b);
            assertSameValue(ac, bc, msg);
            switch (ac) {
            case "[object Function]":
                assertSameValue(Function_toString(a), Function_toString(b), msg);
            }
        }

        function at(prevmsg, segment) {
            return prevmsg ? prevmsg + segment : "at _" + segment;
        }

        // Assert that the arguments a and b are thoroughly structurally equivalent.
        //
        // For the sake of speed, we cut a corner:
        //        var x = {}, y = {}, ax = [x];
        //        assertDeepEq([ax, x], [ax, y]);  // passes (?!)
        //
        // Technically this should fail, since the two object graphs are different.
        // (The graph of [ax, y] contains one more object than the graph of [ax, x].)
        //
        // To get technically correct behavior, pass {strictEquivalence: true}.
        // This is slower because we have to walk the entire graph, and Object.prototype
        // is big.
        //
        return function assertDeepEq(a, b, options) {
            var strictEquivalence = options ? options.strictEquivalence : false;

            function assertSameProto(a, b, msg) {
                check(Object_getPrototypeOf(a), Object_getPrototypeOf(b), at(msg, ".__proto__"));
            }

            function failPropList(na, nb, msg) {
                throw Error_("got own properties " + uneval_(na) + ", expected " + uneval_(nb) +
                             (msg ? " " + msg : ""));
            }

            function assertSameProps(a, b, msg) {
                var na = Object_getOwnPropertyNames(a),
                    nb = Object_getOwnPropertyNames(b);
                if (na.length !== nb.length)
                    failPropList(na, nb, msg);
                for (var i = 0; i < na.length; i++) {
                    var name = na[i];
                    if (name !== nb[i])
                        failPropList(na, nb, msg);
                    var da = Object_getOwnPropertyDescriptor(a, name),
                        db = Object_getOwnPropertyDescriptor(b, name);
                    var pmsg = at(msg, /^[_$A-Za-z0-9]+$/.test(name)
                                       ? /0|[1-9][0-9]*/.test(name) ? "[" + name + "]" : "." + name
                                       : "[" + uneval_(name) + "]");
                    assertSameValue(da.configurable, db.configurable, at(pmsg, ".[[Configurable]]"));
                    assertSameValue(da.enumerable, db.enumerable, at(pmsg, ".[[Enumerable]]"));
                    if (Object_hasOwnProperty(da, "value")) {
                        if (!Object_hasOwnProperty(db, "value"))
                            throw Error_("got data property, expected accessor property" + pmsg);
                        check(da.value, db.value, pmsg);
                    } else {
                        if (Object_hasOwnProperty(db, "value"))
                            throw Error_("got accessor property, expected data property" + pmsg);
                        check(da.get, db.get, at(pmsg, ".[[Get]]"));
                        check(da.set, db.set, at(pmsg, ".[[Set]]"));
                    }
                }
            };

            var ab = Map_();
            var bpath = Map_();

            function check(a, b, path) {
                if (typeof a === "symbol") {
                    // Symbols are primitives, but they have identity.
                    // Symbol("x") !== Symbol("x") but
                    // assertDeepEq(Symbol("x"), Symbol("x")) should pass.
                    if (typeof b !== "symbol") {
                        throw Error_("got " + uneval_(a) + ", expected " + uneval_(b) + " " + path);
                    } else if (uneval_(a) !== uneval_(b)) {
                        // We lamely use uneval_ to distinguish well-known symbols
                        // from user-created symbols. The standard doesn't offer
                        // a convenient way to do it.
                        throw Error_("got " + uneval_(a) + ", expected " + uneval_(b) + " " + path);
                    } else if (Map_has(ab, a)) {
                        assertSameValue(Map_get(ab, a), b, path);
                    } else if (Map_has(bpath, b)) {
                        var bPrevPath = Map_get(bpath, b) || "_";
                        throw Error_("got distinct symbols " + at(path, "") + " and " +
                                     at(bPrevPath, "") + ", expected the same symbol both places");
                    } else {
                        Map_set(ab, a, b);
                        Map_set(bpath, b, path);
                    }
                } else if (isPrimitive(a)) {
                    assertSameValue(a, b, path);
                } else if (isPrimitive(b)) {
                    throw Error_("got " + Object_toString(a) + ", expected " + uneval_(b) + " " + path);
                } else if (Map_has(ab, a)) {
                    assertSameValue(Map_get(ab, a), b, path);
                } else if (Map_has(bpath, b)) {
                    var bPrevPath = Map_get(bpath, b) || "_";
                    throw Error_("got distinct objects " + at(path, "") + " and " + at(bPrevPath, "") +
                                 ", expected the same object both places");
                } else {
                    Map_set(ab, a, b);
                    Map_set(bpath, b, path);
                    if (a !== b || strictEquivalence) {
                        assertSameClass(a, b, path);
                        assertSameProto(a, b, path);
                        assertSameProps(a, b, path);
                        assertSameValue(Object_isExtensible(a),
                                        Object_isExtensible(b),
                                        at(path, ".[[Extensible]]"));
                    }
                }
            }

            check(a, b, "");
        };
    })();
}
