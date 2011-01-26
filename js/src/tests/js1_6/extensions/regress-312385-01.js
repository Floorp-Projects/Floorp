/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//-----------------------------------------------------------------------------
var BUGNUMBER = 312385;
var summary = 'Generic methods with null or undefined |this|';
var actual = '';
var expect = true;
var voids = [null, undefined];


function noop() { }

var generics = {
  String: [{ quote: [] },
{ substring: [] },
{ toLowerCase: [] },
{ toUpperCase: [] },
{ charAt: [] },
{ charCodeAt: [] },
{ indexOf: [] },
{ lastIndexOf: [] },
{ toLocaleLowerCase: [] },
{ toLocaleUpperCase: [] },
{ localeCompare: [] },
{ match: [/(?:)/] }, // match(regexp)
{ search: [] },
{ replace: [] },
{ split: [] },
{ substr: [] },
{ concat: [] },
{ slice: [] }],

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
