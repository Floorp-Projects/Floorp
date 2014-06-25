/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function check() {
    obj2.__proto__ = null;
    return arguments.callee.caller;
}

var obj = { f: function() { check(); }};

var obj2 = { __proto__: obj };

obj2.f();

reportCompare(0, 0, "ok");
