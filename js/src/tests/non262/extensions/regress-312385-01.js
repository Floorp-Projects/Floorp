/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 312385;
var summary = 'Generic methods with null or undefined |this|';
var actual = '';
var expect = true;
var voids = [null, undefined];


function noop() { }

var generics = {
  Array:  [{ join: [] },
{ reverse: [] },
{ sort: [] },
           // { push: [0] },  // push(item1, ...)
           // { pop: [] },
           // { shift: [] },
{ unshift: [] },
           // { splice: [0, 0, 1] }, // splice(start, deleteCount, item1, ...)
{ concat: [] },
{ indexOf: [] },
{ lastIndexOf: [] },
           // forEach is excluded since it does not return a value...
           /* { forEach: [noop] },  // forEach(callback, thisObj) */
{ map: [noop] },      // map(callback, thisObj)
{ filter: [noop] },   // filter(callback, thisObj)
{ some: [noop] },     // some(callback, thisObj)
{ every: [noop] }     // every(callback, thisObj)
    ]
};

printBugNumber(BUGNUMBER);
printStatus (summary);

var global = this;

for (var c in generics)
{
  var methods = generics[c];
  for (var i = 0; i < methods.length; i++)
  {
    var method = methods[i];

    for (var methodname in method)
    {
      for (var v = 0; v < voids.length; v++)
      {
        var Constructor = global[c]

        var argsLen = method[methodname].length;
        assertEq(argsLen === 0 || argsLen === 1, true, "not all arities handled");

        var generic = Constructor[methodname];
        var prototypy = Constructor.prototype[methodname];

        assertEq(typeof generic, "function");
        assertEq(typeof prototypy, "function");

        // GENERIC METHOD TESTING

        try
        {
          switch (method[methodname].length)
          {
            case 0:
              generic(voids[v]);
              break;

            case 1:
              generic(voids[v], method[methodname][0]);
              break;
          }
          throw new Error(c + "." + methodname + " must throw for null or " +
                          "undefined first argument");
        }
        catch (e)
        {
          assertEq(e instanceof TypeError, true,
                   "Didn't get a TypeError for " + c + "." + methodname +
                   " called with null or undefined first argument");
        }


        // PROTOTYPE METHOD TESTING

        try
        {
          prototypy.apply(voids[v], method[methodname][0]);
          throw new Error(c + ".prototype." + methodname + " must throw " +
                          "for null or undefined this");
        }
        catch (e)
        {
          assertEq(e instanceof TypeError, true,
                   c + ".prototype." + methodname + "didn't throw a " +
                   "TypeError when called with null or undefined this");
        }
      }
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests finished.");
