/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'function-bind.js';
var BUGNUMBER = 429507;
var summary = "ES5: Function.prototype.bind";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// ad-hoc testing

assertEq(Function.prototype.hasOwnProperty("bind"), true);

var bind = Function.prototype.bind;
assertEq(bind.length, 1);


var strictReturnThis = function() { "use strict"; return this; };

assertEq(strictReturnThis.bind(undefined)(), undefined);
assertEq(strictReturnThis.bind(null)(), null);

var obj = {};
assertEq(strictReturnThis.bind(obj)(), obj);

assertEq(strictReturnThis.bind(NaN)(), NaN);

assertEq(strictReturnThis.bind(true)(), true);
assertEq(strictReturnThis.bind(false)(), false);

assertEq(strictReturnThis.bind("foopy")(), "foopy");


// rigorous, step-by-step testing

function expectThrowTypeError(fun)
{
  try
  {
    var r = fun();
    throw new Error("didn't throw TypeError, returned " + r);
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true,
             "didn't throw TypeError, got: " + e);
  }
}

/*
 * 1. Let Target be the this value.
 * 2. If IsCallable(Target) is false, throw a TypeError exception.
 */
expectThrowTypeError(function() { bind.call(null); });
expectThrowTypeError(function() { bind.call(undefined); });
expectThrowTypeError(function() { bind.call(NaN); });
expectThrowTypeError(function() { bind.call(0); });
expectThrowTypeError(function() { bind.call(-0); });
expectThrowTypeError(function() { bind.call(17); });
expectThrowTypeError(function() { bind.call(42); });
expectThrowTypeError(function() { bind.call("foobar"); });
expectThrowTypeError(function() { bind.call(true); });
expectThrowTypeError(function() { bind.call(false); });
expectThrowTypeError(function() { bind.call([]); });
expectThrowTypeError(function() { bind.call({}); });


/*
 * 3. Let A be a new (possibly empty) internal list of all of the argument
 *    values provided after thisArg (arg1, arg2 etc), in order.
 * 4. Let F be a new native ECMAScript object .
 * 5. Set all the internal methods, except for [[Get]], of F as specified in
 *    8.12.
 * 6. Set the [[Get]] internal property of F as specified in 15.3.5.4.
 * 7. Set the [[TargetFunction]] internal property of F to Target.
 * 8. Set the [[BoundThis]] internal property of F to the value of thisArg.
 * 9. Set the [[BoundArgs]] internal property of F to A.
 */
// throughout


/* 10. Set the [[Class]] internal property of F to "Function". */
var toString = Object.prototype.toString;
assertEq(toString.call(function(){}), "[object Function]");
assertEq(toString.call(function a(){}), "[object Function]");
assertEq(toString.call(function(a){}), "[object Function]");
assertEq(toString.call(function a(b){}), "[object Function]");
assertEq(toString.call(function(){}.bind()), "[object Function]");
assertEq(toString.call(function a(){}.bind()), "[object Function]");
assertEq(toString.call(function(a){}.bind()), "[object Function]");
assertEq(toString.call(function a(b){}.bind()), "[object Function]");


/*
 * 11. Set the [[Prototype]] internal property of F to the standard built-in
 *     Function prototype object as specified in 15.3.3.1.
 */
assertEq(Object.getPrototypeOf(bind.call(function(){})), Function.prototype);
assertEq(Object.getPrototypeOf(bind.call(function a(){})), Function.prototype);
assertEq(Object.getPrototypeOf(bind.call(function(a){})), Function.prototype);
assertEq(Object.getPrototypeOf(bind.call(function a(b){})), Function.prototype);


/*
 * 12. Set the [[Call]] internal property of F as described in 15.3.4.5.1.
 */
var a = Array.bind(1, 2);
assertEq(a().length, 2);
assertEq(a(4).length, 2);
assertEq(a(4, 8).length, 3);

function t() { return this; }
var bt = t.bind(t);
assertEq(bt(), t);

function callee() { return arguments.callee; }
var call = callee.bind();
assertEq(call(), callee);
assertEq(new call(), callee);


/*
 * 13. Set the [[Construct]] internal property of F as described in 15.3.4.5.2.
 */
function Point(x, y)
{
  this.x = x;
  this.y = y;
}
var YAxisPoint = Point.bind(null, 0)

assertEq(YAxisPoint.hasOwnProperty("prototype"), false);
var p = new YAxisPoint(5);
assertEq(p.x, 0);
assertEq(p.y, 5);
assertEq(p instanceof Point, true);
assertEq(p instanceof YAxisPoint, true);
assertEq(Object.prototype.toString.call(YAxisPoint), "[object Function]");
assertEq(YAxisPoint.length, 1);


/*
 * 14. Set the [[HasInstance]] internal property of F as described in
 *     15.3.4.5.3.
 */
function JoinArguments()
{
  this.args = Array.prototype.join.call(arguments, ", ");
}

var Join1 = JoinArguments.bind(null, 1);
var Join2 = Join1.bind(null, 2);
var Join3 = Join2.bind(null, 3);
var Join4 = Join3.bind(null, 4);
var Join5 = Join4.bind(null, 5);
var Join6 = Join5.bind(null, 6);

var r = new Join6(7);
assertEq(r instanceof Join6, true);
assertEq(r instanceof Join5, true);
assertEq(r instanceof Join4, true);
assertEq(r instanceof Join3, true);
assertEq(r instanceof Join2, true);
assertEq(r instanceof Join1, true);
assertEq(r instanceof JoinArguments, true);
assertEq(r.args, "1, 2, 3, 4, 5, 6, 7");


/*
 * 15. If the [[Class]] internal property of Target is "Function", then
 *   a. Let L be the length property of Target minus the length of A.
 *   b. Set the length own property of F to either 0 or L, whichever is larger.
 * 16. Else set the length own property of F to 0.
 */
function none() { return arguments.length; }
assertEq(none.bind(1, 2)(3, 4), 3);
assertEq(none.bind(1, 2)(), 1);
assertEq(none.bind(1)(2, 3), 2);
assertEq(none.bind().length, 0);
assertEq(none.bind(null).length, 0);
assertEq(none.bind(null, 1).length, 0);
assertEq(none.bind(null, 1, 2).length, 0);

function one(a) { }
assertEq(one.bind().length, 1);
assertEq(one.bind(null).length, 1);
assertEq(one.bind(null, 1).length, 0);
assertEq(one.bind(null, 1, 2).length, 0);

// retch
var br = Object.create(null, { length: { value: 0 } });
try
{
  br = bind.call(/a/g, /a/g, "aaaa");
}
catch (e) { /* nothing */ }
assertEq(br.length, 0);


/*
 * 17. Set the attributes of the length own property of F to the values
 *     specified in 15.3.5.1.
 */
var len1Desc =
  Object.getOwnPropertyDescriptor(function(a, b, c){}.bind(), "length");
assertEq(len1Desc.value, 3);
assertEq(len1Desc.writable, false);
assertEq(len1Desc.enumerable, false);
assertEq(len1Desc.configurable, true);

var len2Desc =
  Object.getOwnPropertyDescriptor(function(a, b, c){}.bind(null, 2), "length");
assertEq(len2Desc.value, 2);
assertEq(len2Desc.writable, false);
assertEq(len2Desc.enumerable, false);
assertEq(len2Desc.configurable, true);


/*
 * 18. Set the [[Extensible]] internal property of F to true.
 */
var bound = (function() { }).bind();

var isExtensible = Object.isExtensible || function() { return true; };
assertEq(isExtensible(bound), true);

bound.foo = 17;
var fooDesc = Object.getOwnPropertyDescriptor(bound, "foo");
assertEq(fooDesc.value, 17);
assertEq(fooDesc.writable, true);
assertEq(fooDesc.enumerable, true);
assertEq(fooDesc.configurable, true);


/*
 * Steps 19-21 are removed from ES6, instead implemented through "arguments" and
 * "caller" accessors on Function.prototype.  So no own properties, but do check
 * for the same observable behavior (modulo where the accessors live).
 */
function strict() { "use strict"; }
function nonstrict() {}

function testBound(fun)
{
  var boundf = fun.bind();

  assertEq(Object.getOwnPropertyDescriptor(boundf, "arguments"), undefined,
           "should be no arguments property");
  assertEq(Object.getOwnPropertyDescriptor(boundf, "caller"), undefined,
           "should be no caller property");

  expectThrowTypeError(function() { return boundf.arguments; });
  expectThrowTypeError(function() { return boundf.caller; });
}

testBound(strict);
testBound(nonstrict);


/* 22. Return F. */
var passim = function p(){}.bind(1);
assertEq(typeof passim, "function");


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
