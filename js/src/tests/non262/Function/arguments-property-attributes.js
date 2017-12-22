/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'arguments-property-attributes.js';
var BUGNUMBER = 516255;
var summary = "Attributes for properties of arguments objects";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// normal

function args() { return arguments; }
var a = args(0, 1);

var argProps = Object.getOwnPropertyNames(a).sort();
assertEq(argProps.indexOf("callee") >= 0, true);
assertEq(argProps.indexOf("0") >= 0, true);
assertEq(argProps.indexOf("1") >= 0, true);
assertEq(argProps.indexOf("length") >= 0, true);

var calleeDesc = Object.getOwnPropertyDescriptor(a, "callee");
assertEq(calleeDesc.value, args);
assertEq(calleeDesc.writable, true);
assertEq(calleeDesc.enumerable, false);
assertEq(calleeDesc.configurable, true);

var zeroDesc = Object.getOwnPropertyDescriptor(a, "0");
assertEq(zeroDesc.value, 0);
assertEq(zeroDesc.writable, true);
assertEq(zeroDesc.enumerable, true);
assertEq(zeroDesc.configurable, true);

var oneDesc = Object.getOwnPropertyDescriptor(a, "1");
assertEq(oneDesc.value, 1);
assertEq(oneDesc.writable, true);
assertEq(oneDesc.enumerable, true);
assertEq(oneDesc.configurable, true);

var lengthDesc = Object.getOwnPropertyDescriptor(a, "length");
assertEq(lengthDesc.value, 2);
assertEq(lengthDesc.writable, true);
assertEq(lengthDesc.enumerable, false);
assertEq(lengthDesc.configurable, true);


// strict

function strictArgs() { "use strict"; return arguments; }
var sa = strictArgs(0, 1);

var strictArgProps = Object.getOwnPropertyNames(sa).sort();
assertEq(strictArgProps.indexOf("callee") >= 0, true);
assertEq(strictArgProps.indexOf("caller") >= 0, false);
assertEq(strictArgProps.indexOf("0") >= 0, true);
assertEq(strictArgProps.indexOf("1") >= 0, true);
assertEq(strictArgProps.indexOf("length") >= 0, true);

var strictCalleeDesc = Object.getOwnPropertyDescriptor(sa, "callee");
assertEq(typeof strictCalleeDesc.get, "function");
assertEq(typeof strictCalleeDesc.set, "function");
assertEq(strictCalleeDesc.get, strictCalleeDesc.set);
assertEq(strictCalleeDesc.enumerable, false);
assertEq(strictCalleeDesc.configurable, false);

var strictCallerDesc = Object.getOwnPropertyDescriptor(sa, "caller");
assertEq(strictCallerDesc, undefined);

var strictZeroDesc = Object.getOwnPropertyDescriptor(sa, "0");
assertEq(strictZeroDesc.value, 0);
assertEq(strictZeroDesc.writable, true);
assertEq(strictZeroDesc.enumerable, true);
assertEq(strictZeroDesc.configurable, true);

var strictOneDesc = Object.getOwnPropertyDescriptor(sa, "1");
assertEq(strictOneDesc.value, 1);
assertEq(strictOneDesc.writable, true);
assertEq(strictOneDesc.enumerable, true);
assertEq(strictOneDesc.configurable, true);

var strictLengthDesc = Object.getOwnPropertyDescriptor(sa, "length");
assertEq(strictLengthDesc.value, 2);
assertEq(strictLengthDesc.writable, true);
assertEq(strictLengthDesc.enumerable, false);
assertEq(strictLengthDesc.configurable, true);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
