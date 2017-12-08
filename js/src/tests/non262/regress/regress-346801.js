/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346801;
var summary = 'Hang regression from bug 346021';
var actual = '';
var expect = 'No Hang';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  try
  {
    var Class = {
      create: function() {
        return function() {
          this.initialize.apply(this, arguments);
        }
      }
    }

    Object.extend = function(destination, source) {
      print("Start");
//      print(destination);
//      print(source);
      if(destination==source)
        print("Same desination and source!");
      var i = 0;
      for (property in source) {
//        print("  " + property);
        destination[property] = source[property];
        ++i;
        if (i > 1000) {
          throw "Hang";
        }
      }
      print("Finish");
      return destination;
    }

    var Ajax = {
    };

    Ajax.Base = function() {};
    Ajax.Base.prototype = {
      responseIsFailure: function() { }
    }

    Ajax.Request = Class.create();

    Ajax.Request.prototype = Object.extend(new Ajax.Base(), {});

    Ajax.Updater = Class.create();

    Object.extend(Ajax.Updater.prototype, Ajax.Request.prototype);
    actual = 'No Hang';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);
}
