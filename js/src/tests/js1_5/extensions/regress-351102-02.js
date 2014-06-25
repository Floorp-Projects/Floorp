/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351102;
var summary = 'try/catch-guard/finally GC issues';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f;
  f = function ()
    {
      var a = null;
      try {  
        a();
      } catch (e) {
      }
      return false;
    };

  try {  
    throw 1;
  } catch (e if f()) {
  } catch (e if e == 1) {
    print("GOOD");
  } catch (e) {
    print("BAD: "+e);
  }

  reportCompare(expect, actual, summary + ': 2');
  exitFunc ('test');
}
