// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: js2; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// Test DataView on SharedArrayBuffer.

if (this.SharedArrayBuffer) {

var sab = new SharedArrayBuffer(4096);
var dv = new DataView(sab);

assertEq(sab, dv.buffer);
assertEq(dv.byteLength, sab.byteLength);
assertEq(ArrayBuffer.isView(dv), true);

var dv2 = new DataView(sab, 1075, 2048);

assertEq(sab, dv2.buffer);
assertEq(dv2.byteLength, 2048);
assertEq(dv2.byteOffset, 1075);
assertEq(ArrayBuffer.isView(dv2), true);

// Test that it is the same buffer memory for the two views

dv.setInt8(1075, 37);
assertEq(dv2.getInt8(0), 37);

// Test that range checking works

assertThrowsInstanceOf(() => dv.setInt32(4098, -1), RangeError);
assertThrowsInstanceOf(() => dv.setInt32(4094, -1), RangeError);
assertThrowsInstanceOf(() => dv.setInt32(-1, -1), RangeError);

assertThrowsInstanceOf(() => dv2.setInt32(2080, -1), RangeError);
assertThrowsInstanceOf(() => dv2.setInt32(2046, -1), RangeError);
assertThrowsInstanceOf(() => dv2.setInt32(-1, -1), RangeError);

}

reportCompare(true,true);
