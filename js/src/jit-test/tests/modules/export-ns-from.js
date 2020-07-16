// |jit-test| module

import { ns } from "export-ns.js";

load(libdir + 'asserts.js');

assertEq(isProxy(ns), true);
assertEq(ns.a, 1);
assertThrowsInstanceOf(function() { eval("delete ns"); }, SyntaxError);
assertThrowsInstanceOf(function() { ns = null; }, TypeError);
