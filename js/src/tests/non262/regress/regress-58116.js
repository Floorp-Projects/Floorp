// |reftest| skip-if(this.hasOwnProperty("Intl")) -- Requires inaccurate historic time zone data.
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Bob Clary
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 58116;
var summary = 'Compute Daylight savings time correctly regardless of year';
var actual = '';
var expect = '';
var status;

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = (new Date(2005, 7, 1).getTimezoneOffset());

status = summary + ' ' + inSection(1) + ' 1970-07-1 ';
actual = (new Date(1970, 7, 1).getTimezoneOffset());
reportCompare(expect, actual, status);
 
status = summary + ' ' + inSection(2) + ' 1965-07-1 ';
actual = (new Date(1965, 7, 1).getTimezoneOffset());
reportCompare(expect, actual, status);
 
status = summary + ' ' + inSection(3) + ' 0000-07-1 ';
actual = (new Date(0, 7, 1).getTimezoneOffset());
reportCompare(expect, actual, status);
