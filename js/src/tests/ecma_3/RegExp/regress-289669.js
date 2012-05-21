// |reftest| random -- BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 289669;
var summary = 'O(N^2) behavior on String.replace(/RegExp/, ...)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


var data = {X: [], Y:[]};

function replace(str) {
  var stra=str.replace(new RegExp('<a>','g'),"<span id=\"neurodna\" style=\"background-color:blue\"/>");
  stra=stra.replace(new RegExp('</a>','g'),"</span><br>");
}

function runTest() {
  for (var j = 1000; j <= 10000; j += 1000)
  {
    neurodna(j);
  }
}

function neurodna(limit) {
  var prepare="<go>";
  for(var i=0;i<limit;i++) {
    prepare += "<a>neurodna</a>";
  }
  prepare+="</go>";
  var da1=new Date();
  replace(prepare);
  var da2=new Date();
  data.X.push(limit);
  data.Y.push(da2-da1);
  gc();
}

runTest();

var order = BigO(data);

var msg = '';
for (var p = 0; p < data.X.length; p++)
{
  msg += '(' + data.X[p] + ', ' + data.Y[p] + '); ';
}
printStatus(msg);
printStatus('Order: ' + order);
reportCompare(true, order < 2, summary + ' BigO ' + order + ' < 2');
