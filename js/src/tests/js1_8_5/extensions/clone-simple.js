// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function testEq(b) {
    var a = deserialize(serialize(b));
    assertEq(a, b);
}

testEq(void 0);
testEq(null);

testEq(true);
testEq(false);

testEq(0);
testEq(-0);
testEq(1/0);
testEq(-1/0);
testEq(0/0);
testEq(Math.PI);

testEq("");
testEq("\0");
testEq("a");  // unit string
testEq("ab");  // length-2 string
testEq("abc\0123\r\n");  // nested null character
testEq("\xff\x7f\u7fff\uffff\ufeff\ufffe");  // random unicode stuff
testEq("\ud800 \udbff \udc00 \udfff"); // busted surrogate pairs
testEq(Array(1024).join(Array(1024).join("x")));  // 2MB string

reportCompare(0, 0, 'ok');
