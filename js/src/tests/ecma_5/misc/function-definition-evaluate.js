// |reftest| skip-if(!xulRuntime.shell) -- needs evaluate()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 577325;
var summary = 'Implement the ES5 algorithm for processing function statements';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var outer, desc;

///////////////////////////////////////////////////
// Function definitions over accessor properties //
///////////////////////////////////////////////////

var getCalled, setCalled;

// configurable properties get blown away

getCalled = false, setCalled = false;
Object.defineProperty(this, "acc1",
                      {
                        get: function() { getCalled = true; throw "FAIL get 1"; },
                        set: function(v) { setCalled = true; throw "FAIL set 1 " + v; },
                        configurable: true,
                        enumerable: false
                      });

// does not throw
outer = undefined;
evaluate("function acc1() { throw 'FAIL redefined 1'; } outer = acc1;");
assertEq(getCalled, false);
assertEq(setCalled, false);
assertEq(typeof acc1, "function");
assertEq(acc1, outer);
desc = Object.getOwnPropertyDescriptor(this, "acc1");
assertEq(desc.value, acc1);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);


getCalled = false, setCalled = false;
Object.defineProperty(this, "acc2",
                      {
                        get: function() { getCalled = true; throw "FAIL get 2"; },
                        set: function(v) { setCalled = true; throw "FAIL set 2 " + v; },
                        configurable: true,
                        enumerable: true
                      });

// does not throw
outer = undefined;
evaluate("function acc2() { throw 'FAIL redefined 2'; } outer = acc2;");
assertEq(getCalled, false);
assertEq(setCalled, false);
assertEq(typeof acc2, "function");
assertEq(acc2, outer);
desc = Object.getOwnPropertyDescriptor(this, "acc2");
assertEq(desc.value, acc2);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);


// non-configurable properties produce a TypeError

getCalled = false, setCalled = false;
Object.defineProperty(this, "acc3",
                      {
                        get: function() { getCalled = true; throw "FAIL get 3"; },
                        set: function(v) { setCalled = true; throw "FAIL set 3 " + v; },
                        configurable: false,
                        enumerable: true
                      });

outer = undefined;
try
{
  evaluate("function acc3() { throw 'FAIL redefined 3'; }; outer = acc3");
  throw new Error("should have thrown trying to redefine global function " +
                  "over a non-configurable, enumerable accessor");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "global function definition, when that function would overwrite " +
           "a non-configurable, enumerable accessor, must throw a TypeError " +
           "per ES5+errata: " + e);
  desc = Object.getOwnPropertyDescriptor(this, "acc3");
  assertEq(typeof desc.get, "function");
  assertEq(typeof desc.set, "function");
  assertEq(desc.enumerable, true);
  assertEq(desc.configurable, false);
  assertEq(outer, undefined);
  assertEq(getCalled, false);
  assertEq(setCalled, false);
}


getCalled = false, setCalled = false;
Object.defineProperty(this, "acc4",
                      {
                        get: function() { getCalled = true; throw "FAIL get 4"; },
                        set: function(v) { setCalled = true; throw "FAIL set 4 " + v; },
                        configurable: false,
                        enumerable: false
                      });

outer = undefined;
try
{
  evaluate("function acc4() { throw 'FAIL redefined 4'; }; outer = acc4");
  throw new Error("should have thrown trying to redefine global function " +
                  "over a non-configurable, non-enumerable accessor");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "global function definition, when that function would overwrite " +
           "a non-configurable, non-enumerable accessor, must throw a " +
           "TypeError per ES5+errata: " + e);
  desc = Object.getOwnPropertyDescriptor(this, "acc4");
  assertEq(typeof desc.get, "function");
  assertEq(typeof desc.set, "function");
  assertEq(desc.enumerable, false);
  assertEq(desc.configurable, false);
  assertEq(outer, undefined);
  assertEq(getCalled, false);
  assertEq(setCalled, false);
}


///////////////////////////////////////////////
// Function definitions over data properties //
///////////////////////////////////////////////


// configurable properties, regardless of other attributes, get blown away

Object.defineProperty(this, "data1",
                      {
                        configurable: true,
                        enumerable: true,
                        writable: true,
                        value: "data1"
                      });

outer = undefined;
evaluate("function data1() { return 'data1 function'; } outer = data1;");
assertEq(typeof data1, "function");
assertEq(data1, outer);
desc = Object.getOwnPropertyDescriptor(this, "data1");
assertEq(desc.configurable, false);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, data1);


Object.defineProperty(this, "data2",
                      {
                        configurable: true,
                        enumerable: true,
                        writable: false,
                        value: "data2"
                      });

outer = undefined;
evaluate("function data2() { return 'data2 function'; } outer = data2;");
assertEq(typeof data2, "function");
assertEq(data2, outer);
desc = Object.getOwnPropertyDescriptor(this, "data2");
assertEq(desc.configurable, false);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, data2);


Object.defineProperty(this, "data3",
                      {
                        configurable: true,
                        enumerable: false,
                        writable: true,
                        value: "data3"
                      });

outer = undefined;
evaluate("function data3() { return 'data3 function'; } outer = data3;");
assertEq(typeof data3, "function");
assertEq(data3, outer);
desc = Object.getOwnPropertyDescriptor(this, "data3");
assertEq(desc.configurable, false);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, data3);


Object.defineProperty(this, "data4",
                      {
                        configurable: true,
                        enumerable: false,
                        writable: false,
                        value: "data4"
                      });

outer = undefined;
evaluate("function data4() { return 'data4 function'; } outer = data4;");
assertEq(typeof data4, "function");
assertEq(data4, outer);
desc = Object.getOwnPropertyDescriptor(this, "data4");
assertEq(desc.value, data4);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);


// non-configurable data properties are trickier

Object.defineProperty(this, "data5",
                      {
                        configurable: false,
                        enumerable: true,
                        writable: true,
                        value: "data5"
                      });

outer = undefined;
evaluate("function data5() { return 'data5 function'; } outer = data5;");
assertEq(typeof data5, "function");
assertEq(data5, outer);
desc = Object.getOwnPropertyDescriptor(this, "data5");
assertEq(desc.configurable, false);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, data5);


Object.defineProperty(this, "data6",
                      {
                        configurable: false,
                        enumerable: true,
                        writable: false,
                        value: "data6"
                      });

outer = undefined;
try
{
  evaluate("function data6() { return 'data6 function'; } outer = data6;");
  throw new Error("should have thrown trying to redefine global function " +
                  "over a non-configurable, enumerable, non-writable accessor");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "global function definition, when that function would overwrite " +
           "a non-configurable, enumerable, non-writable data property, must " +
           "throw a TypeError per ES5+errata: " + e);
  assertEq(data6, "data6");
  assertEq(outer, undefined);
  desc = Object.getOwnPropertyDescriptor(this, "data6");
  assertEq(desc.configurable, false);
  assertEq(desc.enumerable, true);
  assertEq(desc.writable, false);
  assertEq(desc.value, "data6");
}


Object.defineProperty(this, "data7",
                      {
                        configurable: false,
                        enumerable: false,
                        writable: true,
                        value: "data7"
                      });

outer = undefined;
try
{
  evaluate("function data7() { return 'data7 function'; } outer = data7;");
  throw new Error("should have thrown trying to redefine global function " +
                  "over a non-configurable, non-enumerable, writable data" +
                  "property");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "global function definition, when that function would overwrite " +
           "a non-configurable, non-enumerable, writable data property, must " +
           "throw a TypeError per ES5+errata: " + e);
  assertEq(data7, "data7");
  assertEq(outer, undefined);
  desc = Object.getOwnPropertyDescriptor(this, "data7");
  assertEq(desc.configurable, false);
  assertEq(desc.enumerable, false);
  assertEq(desc.writable, true);
  assertEq(desc.value, "data7");
}


Object.defineProperty(this, "data8",
                      {
                        configurable: false,
                        enumerable: false,
                        writable: false,
                        value: "data8"
                      });

outer = undefined;
try
{
  evaluate("function data8() { return 'data8 function'; } outer = data8;");
  throw new Error("should have thrown trying to redefine global function " +
                  "over a non-configurable, non-enumerable, non-writable data" +
                  "property");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "global function definition, when that function would overwrite " +
           "a non-configurable, non-enumerable, non-writable data property, " +
           "must throw a TypeError per ES5+errata: " + e);
  assertEq(data8, "data8");
  assertEq(outer, undefined);
  desc = Object.getOwnPropertyDescriptor(this, "data8");
  assertEq(desc.configurable, false);
  assertEq(desc.enumerable, false);
  assertEq(desc.writable, false);
  assertEq(desc.value, "data8");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
