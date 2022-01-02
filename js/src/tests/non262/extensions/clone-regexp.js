// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function testRegExp(b, c=b) {
    var a = deserialize(serialize(b));
    assertEq(a === b, false);
    assertEq(Object.getPrototypeOf(a), RegExp.prototype);
    assertEq(Object.prototype.toString.call(a), "[object RegExp]");
    for (p in a)
        throw new Error("cloned RegExp should have no enumerable properties");

    assertEq(a.source, c.source);
    assertEq(a.global, c.global);
    assertEq(a.ignoreCase, c.ignoreCase);
    assertEq(a.multiline, c.multiline);
    assertEq(a.sticky, c.sticky);
    assertEq("expando" in a, false);
}

testRegExp(RegExp(""));
testRegExp(/(?:)/);
testRegExp(/^(.*)$/gimy);

var re = /\bx\b/gi;
re.expando = true;
testRegExp(re);
// `source` and the flag accessors are defined on RegExp.prototype, so they're
// not available after re.__proto__ has been changed. We solve that by passing
// in an additional copy of the same RegExp to compare the
// serialized-then-deserialized clone with."
re.__proto__ = {};
testRegExp(re, /\bx\b/gi);

reportCompare(0, 0, 'ok');
