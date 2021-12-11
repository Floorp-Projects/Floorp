/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "for-in-with-assignments.js";
var BUGNUMBER = 1164741;
var summary = "Parse |for (var ... = ... in ...)|."

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// This is a total grab-bag of junk originally in tests changed when this
// syntax was removed.  Leaving it all in one file will make it easier to
// eventually remove.  Avert your eyes!
// TC39 has revived this syntax for ES2017 - "What is dead may never die."

if (typeof Reflect !== "undefined" && Reflect.parse)
  Reflect.parse("for (var x = 3 in []) { }");

/******************************************************************************/

function testQ() {
  try {
    for (var i = 0 in this) throw p;
  } catch (e) {
    if (i !== 94)
      return "what";
  }
}
testQ();

/******************************************************************************/

function f3(i,o){for(var x=i in o)parseInt(o[x]); return x}
function f4(i,o){with(this)for(var x=i in o)parseInt(o[x]); return x}

assertEq(f3(42, []), 42);
assertEq(f3(42, ['first']), "0");
assertEq(f4(42, []), 42);
assertEq(f4(42, ['first']), "0");

/******************************************************************************/

function SetLangHead(l){
  with(p){
    for(var i=0 in x)
      if(getElementById("TxtH"+i)!=undefined)
        parseInt('huh');
  }
}
x=[0,1,2,3];
p={getElementById: function (id){
  if (typeof dis === "function") {
    dis(SetLangHead);
  }
  return undefined;
}};
SetLangHead(1);

/******************************************************************************/

(function(){for(var x = arguments in []){} function x(){}})();

/******************************************************************************/

with (0)
  for (var b = 0 in 0);  // don't assert in parser

/******************************************************************************/

function* g1() {
  for (var x = yield in {}) ;
}
var it = g1();
assertEq(it.next().done, false);
assertEq(it.next().done, true);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
