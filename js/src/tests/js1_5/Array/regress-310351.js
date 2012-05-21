/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 310351;
var summary = 'Convert host "list" objects to arrays';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var nodeList = [];
if (typeof document != 'undefined')
{
  nodeList = document.getElementsByTagName('*');
}
else
{
  printStatus('test using dummy array since no document available');
}
 
var array = Array.prototype.slice.call(nodeList, 0);

expect = 'Array';
actual = array.constructor.name;

// nodeList is live and may change
var saveLength = nodeList.length;

reportCompare(expect, actual, summary + ': constructor test');

expect = saveLength;
actual = array.length;

reportCompare(expect, actual, summary + ': length test');
expect = true;
actual = true;

for (var i = 0; i < saveLength; i++)
{
  if (array[i] != nodeList[i])
  {
    actual = false;
    summary += ' Comparison failed: array[' + i + ']=' + array[i] +
      ', nodeList[' + i + ']=' + nodeList[i];
    break;
  }
}

reportCompare(expect, actual, summary + ': identical elements test');

