/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "for-loop.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 985733;
var summary =
  "ES6 for-loop semantics for for(;;) loops whose heads contain lexical "
  "declarations";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function isError(code, type)
{
  try
  {
    Function(code);
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e instanceof type, true,
             "unexpected error for `" + code + "`: got " + e);
  }
}

function isOK(code)
{
  Function(code);
}

isError("for (const x; ; ) ;", SyntaxError);
isError("for (const x = 5, y; ; ) ;", SyntaxError);
isError("for (const [z]; ; ) ;", SyntaxError);
//isError("for (const [z, z]; ; ) ;", SyntaxError);
//isError("for (const [z, z] = [0, 1]; ; ) ;", SyntaxError);

isOK("for (let x; ; ) ;");
isOK("for (let x = 5, y; ; ) ;");

// I'm fairly sure this is supposed to work: the negative-lookahead rules in
// IterationStatement ensure that |for (let| *always* is a loop header starting
// with a lexical declaration.  But I'm not 100% certain, so these tests might
// need to be fixed when we implement the negative-lookahead restrictions.
isOK("for (let [z] = [3]; ; ) ;");
isError("for (let [z, z]; ; ) ;", SyntaxError); // because missing initializer

isError("for (let [z, z] = [0, 1]; ; ) ;", SyntaxError);

// A for-loop with lexical declarations, with a mixture of bindings that are and
// aren't aliased.  (The mixture stress-tests any code that incorrectly assumes
// all bindings are aliased.)
var funcs = [];
for (let [i, j, k] = [0, 1, 2]; i < 10; i++)
  funcs.push(() => i);

assertEq(funcs[0](), 0);
assertEq(funcs[1](), 1);
assertEq(funcs[2](), 2);
assertEq(funcs[3](), 3);
assertEq(funcs[4](), 4);
assertEq(funcs[5](), 5);
assertEq(funcs[6](), 6);
assertEq(funcs[7](), 7);
assertEq(funcs[8](), 8);
assertEq(funcs[9](), 9);

var outer = "OUTER V IGNORE";
var save;
for (let outer = (save = function() { return outer; }); ; )
  break;
assertEq(save(), save);

var funcs = [];
function t(i, name, expect)
{
  assertEq(funcs[i].name, name);
  assertEq(funcs[i](), expect);
}

if (save() !== "OUTER V IGNORE")
{
  var v = "OUTER V IGNORE";
  var i = 0;
  for (let v = (funcs.push(function init() { return v; }),
               0);
      v = (funcs.push(function test() { return v; }),
           v + 1);
      v = (funcs.push(function incr() { return v; }),
           v + 1))
  {
    v = (funcs.push(function body() { return v; }),
         v + 1);
    i++;
    if (i >= 3)
      break;
  }
  t(0, "init", 0);
  t(1, "test", 2);
  t(2, "body", 2);
  t(3, "incr", 5);
  t(4, "test", 5);
  t(5, "body", 5);
  t(6, "incr", 8);
  t(7, "test", 8);
  t(8, "body", 8);
  assertEq(funcs.length, 9);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
