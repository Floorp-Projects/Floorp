/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "for-in-with-destructuring-assignments.js";
var BUGNUMBER = 1164741;
var summary = "|for (var <pat> = ... in ...)| is invalid syntax";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// This is a total grab-bag of junk originally in tests changed when this
// syntax was removed.  Avert your eyes!

assertThrowsInstanceOf(() => eval(`
   for (var [x] = x>>x in [[]<[]])
   {
     [];
   }`),
 SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(function() {
  // Abandon all hope, ye who try to read this.
  eval(`
    (function () {
      for
      (var [x] = function(){}
       in
       (function m(a) {
         if (a < 1) {
           x;
           return;
         }
         return m(a - 1) + m(a - 2);
       })(7)(eval(""))
      )
      {
        [];
      }
    })
  `)();
}, SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval(`
  for (var [e] = [] in (eval("for (b = 0; b < 6; ++b) gc()"))) {}
`), SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval("for (var [ v , c ] = 0 in undefined) { }"),
                       SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval("var b = e; for (var [e] = b in w) c"),
                       SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval("for (var {a: []} = 2 in []) { }"),
                       SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval(`try
  {
    for (var [,{y}] = 1 in []) {}
  }
  catch(ex)
  {
  }`),
SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval("for (var [x] = [] in null);"),
                       SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval("for (var [x] = x in y) var x;"),
                       SyntaxError);

/******************************************************************************/

assertThrowsInstanceOf(() => eval(`
  for (var [arguments] = ({ get y(){} }) in y ) (x);
`),
SyntaxError);

/******************************************************************************/

if (typeof evalcx == 'function') {
  var src = 'try {\n' +
  '    for (var [e] = /x/ in d) {\n' +
  '        (function () {});\n' +
  '    }\n' +
  '} catch (e) {}\n' +
  'try {\n' +
  '    let(x = Object.freeze(this, /x/))\n' +
  '    e = {}.toString\n' +
  '    function y() {}\n' +
  '} catch (e) {}';

  try
  {
    evalcx(src);
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e.name === "SyntaxError", true,
             "expected invalid syntax, got " + e);
  }
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
