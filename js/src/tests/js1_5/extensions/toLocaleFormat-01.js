/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 291494;
var summary = 'Date.prototype.toLocaleFormat extension';
var actual = '';
var expect = '';
var temp;

/*
 * SpiderMonkey only.
 *
 * When the output of toLocaleFormat exceeds 100 bytes toLocaleFormat
 * defaults to using toString to produce the result.
*/

enterFunc ('test');
printBugNumber(BUGNUMBER);
printStatus (summary);
 
var date = new Date("06/05/2005 00:00:00 GMT-0000");

expect = date.getTimezoneOffset() > 0 ? 'Sat' : 'Sun';
actual = date.toLocaleFormat('%a');
reportCompare(expect, actual, 'Date.toLocaleFormat("%a")');

expect = date.getTimezoneOffset() > 0 ? 'Saturday' : 'Sunday';
actual = date.toLocaleFormat('%A');
reportCompare(expect, actual, 'Date.toLocaleFormat("%A")');

expect = 'Jun';
actual = date.toLocaleFormat('%b');
reportCompare(expect, actual, 'Date.toLocaleFormat("%b")');

expect = 'June';
actual = date.toLocaleFormat('%B');
reportCompare(expect, actual, 'Date.toLocaleFormat("%B")');

expect = (date.getTimezoneOffset() > 0) ? '04' : '05';
actual = date.toLocaleFormat('%d');
reportCompare(expect, actual, 'Date.toLocaleFormat("%d")');

expect = '0';
actual = String((Number(date.toLocaleFormat('%H')) +
                 date.getTimezoneOffset()/60) % 24);
reportCompare(expect, actual, 'Date.toLocaleFormat(%H)');

expect = '12';
actual = String(Number(date.toLocaleFormat('%I')) +
                date.getTimezoneOffset()/60);
reportCompare(expect, actual, 'Date.toLocaleFormat(%I)');

expect = String(155 + ((date.getTimezoneOffset() > 0) ? 0 : 1));
actual = date.toLocaleFormat('%j');
reportCompare(expect, actual, 'Date.toLocaleFormat("%j")');

expect = '06';
actual = date.toLocaleFormat('%m');
reportCompare(expect, actual, 'Date.toLocaleFormat("%m")');

expect = '00';
actual = date.toLocaleFormat('%M');
reportCompare(expect, actual, 'Date.toLocaleFormat("%M")');

expect = true;
temp   = date.toLocaleFormat('%p');
actual = temp == 'AM' || date.toLocaleFormat('%p') == 'PM';
reportCompare(expect, actual, 'Date.toLocaleFormat("%p") is AM or PM');

expect = '00';
actual = date.toLocaleFormat('%S');
reportCompare(expect, actual, 'Date.toLocaleFormat("%S")');

expect = String(22 + ((date.getTimezoneOffset() > 0) ? 0 : 1));
actual = date.toLocaleFormat('%U');
reportCompare(expect, actual, 'Date.toLocaleFormat("%U")');

expect = String((6 + ((date.getTimezoneOffset() > 0) ? 0 : 1))%7);
actual = date.toLocaleFormat('%w');
reportCompare(expect, actual, 'Date.toLocaleFormat("%w")');

expect = '22';
actual = date.toLocaleFormat('%W');
reportCompare(expect, actual, 'Date.toLocaleFormat("%W")');

expect = date.toLocaleTimeString();
actual = date.toLocaleFormat('%X');
reportCompare(expect, actual, 'Date.toLocaleTimeString() == ' +
              'Date.toLocaleFormat("%X")');

expect = '05';
actual = date.toLocaleFormat('%y');
reportCompare(expect, actual, 'Date.toLocaleFormat("%y")');

expect = '2005';
actual = date.toLocaleFormat('%Y');
reportCompare(expect, actual, 'Date.toLocaleFormat("%Y")');

expect = '%';
actual = date.toLocaleFormat('%%');
reportCompare(expect, actual, 'Date.toLocaleFormat("%%")');


expect = '1899 99';
temp='%Y %y';
actual = new Date(0, 0, 0, 13, 14, 15, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '1899189918991899189918991899189918991899189918991899189918991899189918991899189918991899';
temp = '%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(0, 0, 0, 13, 14, 15, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = 'xxx189918991899189918991899189918991899189918991899189918991899189918991899189918991899189918991899';
temp = 'xxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(0, 0, 0, 13, 14, 15, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = new Date(0, 0, 0, 13, 14, 15, 0).toString();
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(0, 0, 0, 13, 14, 15, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = 'xxxx189918991899189918991899189918991899';
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(0, 0, 0, 13, 14, 15, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');


expect = '-51 49';
temp = '%Y %y';
actual = new Date(-51, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51';
temp = '%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(-51, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = 'xxx-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51-51';
temp = 'xxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(-51, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = new Date(-51, 0).toString();
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(-51, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');


expect = '1851 51';
temp = '%Y %y';
actual = new Date(1851, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '1851185118511851185118511851185118511851185118511851185118511851185118511851185118511851';
temp = '%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(1851, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = 'xxx185118511851185118511851185118511851185118511851185118511851185118511851185118511851185118511851';
temp = 'xxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(1851, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = new Date(1851, 0).toString();
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(1851, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');


expect = '-1 99';
temp = '%Y %y';
actual = new Date(-1, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '-100 00';
temp = '%Y %y';
actual = new Date(-100, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '1900 00';
temp = '%Y %y';
actual = new Date(0, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '1901 01';
temp = '%Y %y';
actual = new Date(1, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '1970 70';
temp = '%Y %y';
actual = new Date(1970, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');


expect = new Date(32767, 0).toString();
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(32767, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = '32767327673276732767327673276732767327673276732767327673276732767327673276732767327673276732767';
temp = '%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(32767, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = 'xxxx32767327673276732767327673276732767327673276732767327673276732767327673276732767327673276732767';
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(32767, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = new Date(32767, 0).toString();
temp = 'xxxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(32767, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');


expect = '-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999';
temp = '%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(-9999, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = 'xxxx-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999-9999';
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(-9999, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');

expect = new Date(-9999, 0).toString();
temp = 'xxxx%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y';
actual = new Date(-9999, 0).toLocaleFormat(temp);
reportCompare(expect, actual, 'Date.toLocaleFormat("'+temp+'")');
