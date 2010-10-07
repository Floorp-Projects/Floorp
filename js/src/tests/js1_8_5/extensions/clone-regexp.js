// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function testRegExp(b) {
    var a = deserialize(serialize(b));
    assertEq(a === b, false);
    assertEq(Object.getPrototypeOf(a), RegExp.prototype);
    assertEq(Object.prototype.toString.call(a), "[object RegExp]");
    for (p in a)
        throw new Error("cloned RegExp should have no enumerable properties");

    assertEq(a.source, b.source);
    assertEq(a.global, b.global);
    assertEq(a.ignoreCase, b.ignoreCase);
    assertEq(a.multiline, b.multiline);
    assertEq(a.sticky, b.sticky);
}

testRegExp(RegExp(""));
testRegExp(/(?:)/);
testRegExp(/^(.*)$/gimy);
testRegExp(RegExp.prototype);

var re = /\bx\b/gi;
re.expando = true;
testRegExp(re);
re.__proto__ = {};
testRegExp(re);

reportCompare(0, 0, 'ok');
