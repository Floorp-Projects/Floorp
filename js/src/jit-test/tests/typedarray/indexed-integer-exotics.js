// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function assertThrows(code) {
  try {
    eval(code);
  } catch (e) {
    return;
  }
  assertEq(true, false);
}

Object.prototype["10"] = "unreachable";
Object.prototype["7"] = "unreachable";
Object.prototype["-1"] = "unreachable";
Object.prototype["-0"] = "unreachable";
Object.prototype["4294967295"] = "unreachable";

var array = new Int32Array(10);

function check() {
  for (var i = 0; i < 4; i++) {
    assertEq(undefined, array["-1"]);
    assertEq(undefined, array["-0"]);
    assertEq(undefined, array["10"]);
    assertEq(undefined, array["4294967295"]);
  }
  assertEq("unreachable", array.__proto__["-1"]);
  assertEq("unreachable", array.__proto__["-0"]);
  assertEq("unreachable", array.__proto__["10"]);
  assertEq("unreachable", array.__proto__["4294967295"]);
}

check();

array["-1"] = "unreachable";
array["-0"] = "unreachable";
array["10"] = "unreachable";
array["4294967295"] = "unreachable";

check();

delete array["-0"];
delete array["-1"];
delete array["10"];
delete array["4294967295"];

assertEq(undefined, Object.getOwnPropertyDescriptor(array, "-1"));
assertEq(undefined, Object.getOwnPropertyDescriptor(array, "-0"));
assertEq(undefined, Object.getOwnPropertyDescriptor(array, "10"));
assertEq(undefined, Object.getOwnPropertyDescriptor(array, "4294967295"));
assertEq(10, Object.keys(array).length);

check();

function f() { return array["-1"]; }

for (var i = 0; i < 3; i++) {
  assertEq(undefined, f());
}
setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("ion.warmup.trigger", 0);
assertEq(undefined, f());

// These checks currently fail due to bug 1129202 not being implemented yet.
// We should uncomment them once that bug landed.
//assertThrows('Object.defineProperty(new Int32Array(100), -1, {value: 1})');
// -0 gets converted to the string "0", so use "-0" instead.
//assertThrows('Object.defineProperty(new Int32Array(100), "-0", {value: 1})');
//assertThrows('Object.defineProperty(new Int32Array(100), -10, {value: 1})');
//assertThrows('Object.defineProperty(new Int32Array(), 4294967295, {value: 1})');

check();
