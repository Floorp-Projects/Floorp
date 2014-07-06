// |jit-test| error: done

var g1 = newGlobal('same-compartment');
var g2 = newGlobal();
var proxyStr = "Proxy.create(                                    "+
"  { getOwnPropertyDescriptor: function() assertEq(true,false),  "+
"    getPropertyDescriptor: function() assertEq(true,false),     "+
"    getOwnPropertyNames: function() assertEq(true,false),       "+
"    getPropertyNames: function() assertEq(true,false),          "+
"    defineProperty: function() assertEq(true,false),            "+
"    delete: function() assertEq(true,false),                    "+
"    fix: function() assertEq(true,false), },                    "+
"  Object.prototype                                              "+
");                                                              ";
var proxy1 = g1.eval(proxyStr);
var proxy2 = g2.eval(proxyStr);

function test(str, f) {
    "use strict";

    var x = f(eval(str));
    assertEq(x, f(g1.eval(str)));
    assertEq(x, f(g2.eval(str)));

    var threw = false;
    try {
        f(g1.eval("new Object()"));
    } catch (e) {
        assertEq(Object.prototype.toString.call(e), "[object Error]");
        threw = true;
    }
    assertEq(threw, true);
    threw = false;
    try {
        f(g2.eval("new Object()"));
    } catch (e) {
        assertEq(Object.prototype.toString.call(e), "[object Error]");
        threw = true;
    }
    assertEq(threw, true);
    threw = false;
    try {
        f(proxy1);
    } catch (e) {
        assertEq(Object.prototype.toString.call(e), "[object Error]");
        threw = true;
    }
    assertEq(threw, true);
    threw = false;
    try {
        f(proxy2);
    } catch (e) {
        assertEq(Object.prototype.toString.call(e), "[object Error]");
        threw = true;
    }
    assertEq(threw, true);
}

test("new Boolean(true)", function(b) Boolean.prototype.toSource.call(b));
test("new Boolean(true)", function(b) Boolean.prototype.toString.call(b));
test("new Boolean(true)", function(b) Boolean.prototype.valueOf.call(b));
test("new Number(1)", function(n) Number.prototype.toSource.call(n));
test("new Number(1)", function(n) Number.prototype.toString.call(n));
test("new Number(1)", function(n) Number.prototype.valueOf.call(n));
test("new Number(1)", function(n) Number.prototype.toFixed.call(n));
test("new Number(1)", function(n) Number.prototype.toExponential.call(n));
test("new Number(1)", function(n) Number.prototype.toPrecision.call(n));
test("new Iterator({x:1})", function(i) Iterator.prototype.next.call(i).toString());
test("(function(){yield 1})()", function(i) (function(){yield})().next.call(i).toString());
test("new String('one')", function(s) String.prototype.toSource.call(s));
test("new String('one')", function(s) String.prototype.toString.call(s));
test("new RegExp('1')", function(r) RegExp.prototype.toString.call(r));
test("new RegExp('1')", function(r) RegExp.prototype.exec.call(r, '1').toString());
test("new RegExp('1')", function(r) RegExp.prototype.test.call(r, '1'));
test("new RegExp('1')", function(r) RegExp.prototype.compile.call(r, '1').toString());
test("new RegExp('1')", function(r) assertEq("a1".search(r), 1));
test("new RegExp('1')", function(r) assertEq("a1".match(r)[0], '1'));
test("new RegExp('1')", function(r) assertEq("a1".replace(r, 'A'), 'aA'));
test("new RegExp('1')", function(r) assertEq(String("a1b".split(r)), "a,b"));
test("new RegExp('1')", function(r) assertEq(r, RegExp(r)));
test("new RegExp('1')", function(r) assertEq(String(new RegExp(r)), String(r)));
test("new RegExp('1')", function(r) assertEq(String(/x/.compile(r)), String(r)));
test("new WeakMap()", function(w) WeakMap.prototype.has.call(w, {}));
test("new WeakMap()", function(w) WeakMap.prototype.get.call(w, {}));
test("new WeakMap()", function(w) WeakMap.prototype.delete.call(w, {}));
test("new WeakMap()", function(w) WeakMap.prototype.set.call(w, {}).toString());
test("new Map()", function(w) Map.prototype.has.call(w, {}));
test("new Map()", function(w) Map.prototype.get.call(w, {}));
test("new Map()", function(w) Map.prototype.delete.call(w, {}));
test("new Map()", function(w) Map.prototype.set.call(w, {}).toString());
test("new Set()", function(w) Set.prototype.has.call(w, {}));
test("new Set()", function(w) Set.prototype.add.call(w, {}).toString());
test("new Set()", function(w) Set.prototype.delete.call(w, {}));

test("new Int8Array(1)", function(a) Int8Array.prototype.subarray.call(a).toString());
test("new Uint8Array(1)", function(a) Uint8Array.prototype.subarray.call(a).toString());
test("new Int16Array(1)", function(a) Int16Array.prototype.subarray.call(a).toString());
test("new Uint16Array(1)", function(a) Uint16Array.prototype.subarray.call(a).toString());
test("new Int32Array(1)", function(a) Int32Array.prototype.subarray.call(a).toString());
test("new Uint32Array(1)", function(a) Uint32Array.prototype.subarray.call(a).toString());
test("new Float32Array(1)", function(a) Float32Array.prototype.subarray.call(a).toString());
test("new Float64Array(1)", function(a) Float64Array.prototype.subarray.call(a).toString());
test("new Uint8ClampedArray(1)", function(a) Uint8ClampedArray.prototype.subarray.call(a).toString());

test("new Int8Array(1)", function(a) Int8Array.subarray(a).toString());
test("new Uint8Array(1)", function(a) Uint8Array.subarray(a).toString());
test("new Int16Array(1)", function(a) Int16Array.subarray(a).toString());
test("new Uint16Array(1)", function(a) Uint16Array.subarray(a).toString());
test("new Int32Array(1)", function(a) Int32Array.subarray(a).toString());
test("new Uint32Array(1)", function(a) Uint32Array.subarray(a).toString());
test("new Float32Array(1)", function(a) Float32Array.subarray(a).toString());
test("new Float64Array(1)", function(a) Float64Array.subarray(a).toString());
test("new Uint8ClampedArray(1)", function(a) Uint8ClampedArray.subarray(a).toString());

test("new Int8Array(1)", function(a) Int8Array.prototype.set.call(a, []));
test("new Uint8Array(1)", function(a) Uint8Array.prototype.set.call(a, []));
test("new Int16Array(1)", function(a) Int16Array.prototype.set.call(a, []));
test("new Uint16Array(1)", function(a) Uint16Array.prototype.set.call(a, []));
test("new Int32Array(1)", function(a) Int32Array.prototype.set.call(a, []));
test("new Uint32Array(1)", function(a) Uint32Array.prototype.set.call(a, []));
test("new Float32Array(1)", function(a) Float32Array.prototype.set.call(a, []));
test("new Float64Array(1)", function(a) Float64Array.prototype.set.call(a, []));
test("new Uint8ClampedArray(1)", function(a) Uint8ClampedArray.prototype.set.call(a, []));

function justDontThrow() {}
test("new Date()", function(d) justDontThrow(Date.prototype.getTime.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getYear.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getFullYear.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getUTCFullYear.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getMonth.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getUTCMonth.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getDate.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getUTCDate.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getDay.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getUTCDay.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getHours.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getUTCHours.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getMinutes.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getUTCMinutes.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getSeconds.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getUTCSeconds.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.getTimezoneOffset.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setTime.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setMilliseconds.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setUTCMilliseconds.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setSeconds.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setUTCSeconds.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setMinutes.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setUTCMinutes.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setHours.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setUTCHours.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setDate.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setUTCDate.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setMonth.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setUTCMonth.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setFullYear.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setUTCFullYear.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.setYear.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toGMTString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toISOString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toLocaleString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toLocaleDateString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toLocaleTimeString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toLocaleFormat.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toTimeString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toDateString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toSource.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.toString.call(d)));
test("new Date()", function(d) justDontThrow(Date.prototype.valueOf.call(d)));

throw "done";
