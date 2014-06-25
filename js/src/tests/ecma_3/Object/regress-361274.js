/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 361274;
var summary = 'Embedded nulls in property names';
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
 
  var x='123'+'\0'+'456';
  var y='123'+'\0'+'789';
  var a={};
  a[x]=1;
  a[y]=2;

  reportCompare(1, a[x], summary + ': 123\\0456');
  reportCompare(2, a[y], summary + ': 123\\0789');

  exitFunc ('test');
}
