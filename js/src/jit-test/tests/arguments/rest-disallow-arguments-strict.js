"use strict";
load(libdir + "asserts.js");
assertThrowsInstanceOf(function () {
    eval("(function (...arguments) {})");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("(function (...eval) {})");
}, SyntaxError);
