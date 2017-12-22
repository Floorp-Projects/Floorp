/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465377;
var summary = 'instanceof relations between Error objects';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = actual = 'No Exception';

  try
  {
    var list = [
      "Error",
      "InternalError",
      "EvalError",
      "RangeError",
      "ReferenceError",
      "SyntaxError",
      "TypeError",
      "URIError"
      ];
    var instances = [];

    for (var i = 0; i != list.length; ++i) {
      var name = list[i];
      var constructor = this[name];
      var tmp = constructor.name;
      if (tmp !== name)
        throw "Bad value for "+name+".name: "+uneval(tmp);
      instances[i] = new constructor();
    }

    for (var i = 0; i != instances.length; ++i) {
      var instance = instances[i];
      var name = instance.name;
      var constructor = instance.constructor;
      if (constructor !== this[name])
        throw "Bad value of (new "+name+").constructor: "+uneval(tmp);
      var tmp = constructor.name;
      if (tmp !== name)
        throw "Bad value for constructor.name: "+uneval(tmp);
      if (!(instance instanceof Object))
        throw "Bad instanceof Object for "+name;
      if (!(instance instanceof Error))
        throw "Bad instanceof Error for "+name;
      if (!(instance instanceof constructor))
        throw "Bad instanceof constructor for "+name;
      if (instance instanceof Function)
        throw "Bad instanceof Function for "+name;
      for (var j = 1; j != instances.length; ++j) {
        if (i != j && instance instanceof instances[j].constructor) {
          throw "Unexpected (new "+name+") instanceof "+ instances[j].name;
        }
      }
    }

    print("OK");
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);
}
