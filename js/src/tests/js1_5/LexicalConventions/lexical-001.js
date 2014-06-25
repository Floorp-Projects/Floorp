/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 * Date: 26 November 2000
 *
 *SUMMARY: Testing numeric literals that begin with 0.
 *This test arose from Bugzilla bug 49233.
 */

//-------------------------------------------------------------------------------------------------
var BUGNUMBER = '49233';
var summary = 'Testing numeric literals that begin with 0';
var statprefix = 'Testing ';
var quote = "'";
var asString = new Array();
var actual = new Array();
var expect = new Array();


  asString[0]='01'
  actual[0]=01
  expect[0]=1

  asString[1]='07'
  actual[1]=07
  expect[1]=7

  asString[4]='010'
  actual[4]=010
  expect[4]=8

  asString[5]='017'
  actual[5]=017
  expect[5]=15

  asString[12]='000000000077'
  actual[12]=000000000077
  expect[12]=63

  asString[14]='0000000000770000'
  actual[14]=0000000000770000
  expect[14]=258048

  asString[18]='0000001001007'
  actual[18]=0000001001007
  expect[18]=262663

  asString[20]='070'
  actual[20]=070
  expect[20]=56


//-------------------------------------------------------------------------------------------------
  test();
//-------------------------------------------------------------------------------------------------


function showStatus(msg)
{
  return (statprefix  + quote  +  msg  + quote);
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  for (i=0; i !=asString.length; i++)
  {
    reportCompare (expect[i], actual[i], showStatus(asString[i]));
  }

  exitFunc ('test');
}
