"use strict";

if (this.Reflect && Reflect.set)
    throw new Error("Congrats on implementing Reflect.set! Uncomment this test for 1 karma point.");

/*
load("asserts.js");

var a = [0, 1, 2, 3];
var p = new Proxy({}, {});
Reflect.set(p, "length", 2, a);
assertEq("length" in p, false);
assertEq(a.length, 2);
assertDeepEq(a, [0, 1]);
*/
