"use strict";
var obj = {};
Object.defineProperty(obj, "test", {configurable: false, writable: false, value: "hey"});
Object.defineProperty(obj, "test", {configurable: false, writable: false});

var wrapper = new Proxy(obj, {});
Object.defineProperty(wrapper, "test", {configurable: false, writable: false});
assertEq(wrapper.test, "hey");
