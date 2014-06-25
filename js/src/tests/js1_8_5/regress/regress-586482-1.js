/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
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

var MyObj = function() {
};

MyObj.prototype.doThing = function() {
    checkCaller(this);
};

(new MyObj()).doThing();

reportCompare(expect, actual, "ok");
