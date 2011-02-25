/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var expect = true;
var actual;

var checkCaller = function(me) {
    var caller = arguments.callee.caller;
    var callerIsMethod = (caller === me['doThing']);
    actual = callerIsMethod;
};

Object.prototype.doThing = function() {
    checkCaller(this);
};

["dense"].doThing();

reportCompare(expect, actual, "ok");
