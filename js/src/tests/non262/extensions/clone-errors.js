// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function check(v) {
    try {
        serialize(v);
    } catch (exc) {
        return;
    }
    throw new Error("serializing " + JSON.stringify(v) + " should have failed with an exception");
}

// Unsupported object types.
check(this);
check(Math);
check(function () {});
check(new Proxy({}, {}));

// A failing getter.
check({get x() { throw new Error("fail"); }});

// Mismatched scopes.
for (let [write_scope, read_scope] of [['SameProcess', 'DifferentProcessForIndexedDB'],
                                       ['SameProcess', 'DifferentProcess']])
{
  var ab = new ArrayBuffer(12);
  var buffer = serialize(ab, [ab], { scope: write_scope });
  var caught = false;
  try {
    deserialize(buffer, { scope: read_scope });
  } catch (exc) {
    caught = true;
  }
  assertEq(caught, true, `${write_scope} clone buffer should not be deserializable as ${read_scope}`);
}

// Extra data. This is not checked in #define FUZZING builds.
const fuzzing = getBuildConfiguration("fuzzing-defined");
const shouldThrow = fuzzing === false;

var clone = serialize({foo: 7}, undefined, {scope: 'DifferentProcess'});
deserialize(clone);
clone.clonebuffer = clone.clonebuffer + "\0\0\0\0\0\0\0\0";
var exc = {message: 'no error'};
try {
  deserialize(clone);
} catch (e) {
  exc = e;
}
if (shouldThrow) {
  assertEq(exc.message.includes("bad serialized structured data"), true);
  assertEq(exc.message.includes("extra data"), true);
}

// Extra data between the main body and "tail" of the clone data.
function dumpData(data) {
  data.forEach((x, i) => print(`[${i}] 0x${(i*8).toString(16)} : 0x${x.toString(16)}`));
}

function testInnerExtraData() {
  const ab = new ArrayBuffer(8);
  (new BigUint64Array(ab))[0] = 0xdeadbeefn;
  const clone = serialize({ABC: 7, CBA: ab}, [ab], {scope: 'DifferentProcess'});

  const data = [...new BigUint64Array(clone.arraybuffer)];
  dumpData(data);

  const fake = new ArrayBuffer(clone.arraybuffer.byteLength + 24);
  const view = new BigUint64Array(fake);
  view.set(new BigUint64Array(clone.arraybuffer), 0);
  view[1] = view[1] & ~1n; // SCTAG_TRANSFER_MAP_HEADER with SCTAG_TM_UNREAD
  view[5] += 24n; // Make space for another ArrayBuffer clone at the end
  view[9] = 0xffff00030000000dn; // Change the constant 7 to 13
  view[16] = 0xfeeddeadbeef2dadn; // Change stored ArrayBuffer contents
  view[17] = view[14]; // SCTAG_ARRAY_BUFFER_OBJECT_V2
  view[18] = view[15]; // 8 bytes long
  view[19] = 0x1cedc0ffeen; // Content

  dumpData(view);
  clone.arraybuffer = fake;

  let d;
  let exc;
  try {
    d = deserialize(clone);
    print(JSON.stringify(d));
    print(new BigUint64Array(d.CBA)[0].toString(16));
  } catch (e) {
    exc = e;
  }

  const fuzzing = getBuildConfiguration("fuzzing-defined");
  const shouldThrow = fuzzing === false;

  if (shouldThrow) {
    assertEq(Boolean(exc), true);
    assertEq(exc.message.includes("extra data"), true);
    print(`PASS with FUZZING: Found expected exception "${exc.message}"`);
  } else {
    assertEq(new BigUint64Array(d.CBA)[0].toString(16), "1cedc0ffee");
    assertEq(d.ABC, 13);
    print("PASS without FUZZING");
  }
}

testInnerExtraData();

reportCompare(0, 0, "ok");
