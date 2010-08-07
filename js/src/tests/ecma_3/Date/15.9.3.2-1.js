/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Bob Clary
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 273292;
var summary = '15.9.3.2  new Date(value)';
var actual = '';
var expect = '';
var date1;
var date2;
var i;
var validDateStrings = [
  "11/69/2004",
  "11/70/2004",
  "69/69/2004",
  "69/69/69",
  "69/69/1969",
  "70/69/70",
  "70/69/1970",
  "70/69/2004"
  ];

var invalidDateStrings = [
  "70/70/70",
  "70/70/1970",
  "70/70/2004"
  ];

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 0;

for (i = 0; i < validDateStrings.length; i++)
{
  date1 = new Date(validDateStrings[i]);
  date2 = new Date(date1.toDateString());
  actual = date2 - date1;

  reportCompare(expect, actual, inSection(i) + ' ' +
		validDateStrings[i]);
}

expect = true;

var offset = validDateStrings.length;

for (i = 0; i < invalidDateStrings.length; i++)
{
  date1 = new Date(invalidDateStrings[i]);
  actual = isNaN(date1);

  reportCompare(expect, actual, inSection(i + offset) + ' ' +
		invalidDateStrings[i] + ' is invalid.');
}

