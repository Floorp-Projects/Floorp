/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346773;
var summary = 'Do not crash compiling with misplaced brances in function';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  try
  {
    var src =
      '    var it = {foo:"5"};' +
      '    it.__iterator__ =' +
      '      function(valsOnly)' +
      '      {' +
      '        var gen =' +
      '        function()' +
      '        {' +
      '          for (var i = 0; i < keys.length; i++)' +
      '          {' +
      '            if (valsOnly)' +
      '              yield vals[i];' +
      '            else' +
      '              yield [keys[i], vals[i]];' +
      '          }' +
      '          return gen();' +
      '        }' +
      '      }';
    eval(src);
  }
  catch(ex)
  {
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
