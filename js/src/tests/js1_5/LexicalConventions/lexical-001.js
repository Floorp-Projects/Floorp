/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 * Date: 26 November 2000
 *
 *SUMMARY: Testing numeric literals that begin with 0.
 *This test arose from Bugzilla bug 49233.
 *The best explanation is from jsscan.c:               
 *
 *     "We permit 08 and 09 as decimal numbers, which makes
 *     our behaviour a superset of the ECMA numeric grammar. 
 *     We might not always be so permissive, so we warn about it."
 *
 *Thus an expression 010 will evaluate, as always, as an octal (to 8).
 *However, 018 will evaluate as a decimal, to 18. Even though the
 *user began the expression as an octal, he later used a non-octal
 *digit. We forgive this and assume he intended a decimal. If the
 *JavaScript "strict" option is set though, we will give a warning.
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

  asString[2]='08'
  actual[2]=08
  expect[2]=8

  asString[3]='09'
  actual[3]=09
  expect[3]=9

  asString[4]='010'
  actual[4]=010
  expect[4]=8

  asString[5]='017'
  actual[5]=017
  expect[5]=15

  asString[6]='018'
  actual[6]=018
  expect[6]=18

  asString[7]='019'
  actual[7]=019
  expect[7]=19

  asString[8]='079'
  actual[8]=079
  expect[8]=79

  asString[9]='0079'
  actual[9]=0079
  expect[9]=79

  asString[10]='099'
  actual[10]=099
  expect[10]=99

  asString[11]='0099'
  actual[11]=0099
  expect[11]=99

  asString[12]='000000000077'
  actual[12]=000000000077
  expect[12]=63

  asString[13]='000000000078'
  actual[13]=000000000078
  expect[13]=78

  asString[14]='0000000000770000'
  actual[14]=0000000000770000
  expect[14]=258048

  asString[15]='0000000000780000'
  actual[15]=0000000000780000
  expect[15]=780000

  asString[16]='0765432198'
  actual[16]=0765432198
  expect[16]=765432198

  asString[17]='00076543219800'
  actual[17]=00076543219800
  expect[17]=76543219800

  asString[18]='0000001001007'
  actual[18]=0000001001007
  expect[18]=262663

  asString[19]='0000001001009'
  actual[19]=0000001001009
  expect[19]=1001009

  asString[20]='070'
  actual[20]=070
  expect[20]=56

  asString[21]='080'
  actual[21]=080
  expect[21]=80



//-------------------------------------------------------------------------------------------------
  test();
//-------------------------------------------------------------------------------------------------


function showStatus(msg)
{
  return (statprefix  + quote  +  msg  + quote);
}


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  for (i=0; i !=asString.length; i++)
  {
    reportCompare (expect[i], actual[i], showStatus(asString[i]));
  }
}
