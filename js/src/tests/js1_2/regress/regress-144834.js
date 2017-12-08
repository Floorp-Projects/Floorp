/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    05 July 2002
 * SUMMARY: Testing local var having same name as switch label inside function
 *
 * The code below crashed while compiling in JS1.1 or JS1.2
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=144834
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 144834;
var summary = 'Local var having same name as switch label inside function';

print(BUGNUMBER);
print(summary);


function RedrawSched()
{
  var MinBound;

  switch (i)
  {
  case MinBound :
  }
}


/*
 * Also try eval scope -
 */
var s = '';
s += 'function RedrawSched()';
s += '{';
s += '  var MinBound;';
s += '';
s += '  switch (i)';
s += '  {';
s += '    case MinBound :';
s += '  }';
s += '}';
eval(s);

AddTestCase('Do not crash', 'No Crash', 'No Crash');
test();
