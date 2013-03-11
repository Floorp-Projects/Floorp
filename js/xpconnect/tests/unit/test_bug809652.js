/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=813901 */

const Cu = Components.utils;
const TypedArrays = [ Int8Array, Uint8Array, Int16Array, Uint16Array,
                      Int32Array, Uint32Array, Float32Array, Float64Array,
                      Uint8ClampedArray ];

// Make sure that the correct nativecall-y stuff is denied on security wrappers.

function run_test() {

  var sb = new Cu.Sandbox('http://www.example.org');
  sb.obj = {foo: 2};

  /* Set up some typed arrays. */
  sb.ab = new ArrayBuffer(8);
  for (var i = 0; i < 8; ++i)
    new Uint8Array(sb.ab)[i] = i * 10;
  sb.ta = [];
  TypedArrays.forEach(function(f) sb.ta.push(new f(sb.ab)));
  sb.dv = new DataView(sb.ab);

  /* Things that should throw. */
  checkThrows("Object.prototype.__lookupSetter__('__proto__').call(obj, {});", sb);
  sb.re = /f/;
  checkThrows("RegExp.prototype.exec.call(re, 'abcdefg').index", sb);
  sb.d = new Date();
  checkThrows("Date.prototype.setYear.call(d, 2011)", sb);
  sb.m = new Map();
  checkThrows("(new Map()).clear.call(m)", sb);
  checkThrows("ArrayBuffer.prototype.__lookupGetter__('byteLength').call(ab);", sb);
  checkThrows("ArrayBuffer.prototype.slice.call(ab, 0);", sb);
  checkThrows("DataView.prototype.getInt8.call(dv, 0);", sb);

  /* Things we explicitly allow in ExposedPropertiesOnly::allowNativeCall. */

  /* Date */
  do_check_eq(Cu.evalInSandbox("Date.prototype.getYear.call(d)", sb), sb.d.getYear());
  do_check_eq(Cu.evalInSandbox("Date.prototype.valueOf.call(d)", sb), sb.d.valueOf());
  do_check_eq(Cu.evalInSandbox("d.valueOf()", sb), sb.d.valueOf());
  do_check_eq(Cu.evalInSandbox("Date.prototype.toString.call(d)", sb), sb.d.toString());
  do_check_eq(Cu.evalInSandbox("d.toString()", sb), sb.d.toString());

  /* Typed arrays. */
  function testForTypedArray(t) {
    sb.curr = t;
    do_check_eq(Cu.evalInSandbox("this[curr.constructor.name].prototype.subarray.call(curr, 0)[0]", sb), t[0]);
    do_check_eq(Cu.evalInSandbox("(new this[curr.constructor.name]).__lookupGetter__('length').call(curr)", sb), t.length);
    do_check_eq(Cu.evalInSandbox("(new this[curr.constructor.name]).__lookupGetter__('buffer').call(curr)", sb), sb.ab);
    do_check_eq(Cu.evalInSandbox("(new this[curr.constructor.name]).__lookupGetter__('byteOffset').call(curr)", sb), t.byteOffset);
    do_check_eq(Cu.evalInSandbox("(new this[curr.constructor.name]).__lookupGetter__('byteLength').call(curr)", sb), t.byteLength);
  }
  sb.ta.forEach(testForTypedArray);
}

function checkThrows(expression, sb) {
  var result = Cu.evalInSandbox('(function() { try { ' + expression + '; return "allowed"; } catch (e) { return e.toString(); }})();', sb);
  dump('result: ' + result + '\n\n\n');
  do_check_true(!!/denied/.exec(result));
}

