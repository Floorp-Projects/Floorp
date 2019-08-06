// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function testBigInt(b) {
    var a = deserialize(serialize(b));
    assertEq(typeof b, "bigint");
    assertEq(typeof a, "bigint");
    assertEq(a, b);
}

testBigInt(0n);
testBigInt(-1n);
testBigInt(1n);

testBigInt(0xffffFFFFffffFFFFffffFFFFffffFFFFn);
testBigInt(-0xffffFFFFffffFFFFffffFFFFffffFFFFn);

var g = newGlobal({ enableBigInt: false, sameCompartmentAs: this });
var deserializeNoBigInt = g.evaluate("deserialize");

assertEq(deserializeNoBigInt(serialize(1)), 1);
assertThrows(() => deserializeNoBigInt(serialize(1n)))
assertThrows(() => deserializeNoBigInt(serialize(0n)))
assertThrows(() => deserializeNoBigInt(serialize(0xffffffn)))
assertThrows(() => deserializeNoBigInt(serialize(-1n)))

reportCompare(0, 0, 'ok');
