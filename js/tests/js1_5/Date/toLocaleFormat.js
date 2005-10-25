/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mike Shaver
 *                 Bob Clary
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
//-----------------------------------------------------------------------------
var bug = 291494;
var summary = 'Date.prototype.toLocaleFormat extension';
var actual = '';
var expect = '';
var temp;

enterFunc ('test');
printBugNumber (bug);
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

/*
expect = date.toLocaleFormat('%a %b %e %H:%M:%S %Y');
actual = date.toLocaleFormat('%c');
reportCompare(expect, actual, 
              'Date.toLocaleFormat("%a %b %e %H:%M:%S %Y") == ' + 
              'Date.toLocaleFormat("%c")');
*/

/*
expect = '20';
actual = date.toLocaleFormat('%C');
reportCompare(expect, actual, 'Date.toLocaleFormat("%C")');
*/

/*
expect = date.toLocaleFormat('%C%y');
actual = date.toLocaleFormat('%Y');
reportCompare(expect, actual, 'Date.toLocaleFormat("%C%y") == ' + 
              'Date.toLocaleFormat("%Y")');
*/

expect = (date.getTimezoneOffset() > 0) ? '04' : '05';
actual = date.toLocaleFormat('%d');
reportCompare(expect, actual, 'Date.toLocaleFormat("%d")');

/*
expect = date.toLocaleFormat('%m/%d/%y');
actual = date.toLocaleFormat('%D');
reportCompare(expect, actual, 'Date.toLocaleFormat("%m/%d/%y") == ' + 
              'Date.toLocaleFormat("%D")');
*/

/*
expect = (date.getTimezoneOffset() > 0) ? ' 4' : ' 5';
actual = date.toLocaleFormat('%e');
reportCompare(expect, actual, 'Date.toLocaleFormat("%e")');
*/

/*
expect = date.toLocaleFormat('%Y-%m-%d');
actual = date.toLocaleFormat('%F');
reportCompare(expect, actual, 'Date.toLocaleFormat("%Y-%m-%d") == ' +
              'Date.toLocaleFormat("%F")');
*/

/*
expect = '05';
actual = date.toLocaleFormat('%g');
reportCompare(expect, actual, 'Date.toLocaleFormat("%g")');
*/

/*
expect = '2005';
actual = date.toLocaleFormat('%G');
reportCompare(expect, actual, 'Date.toLocaleFormat("%G")');
*/

/*
expect = date.toLocaleFormat('%b');
actual = date.toLocaleFormat('%h');
reportCompare(expect, actual, 'Date.toLocaleFormat("%b") == ' + 
              'Date.toLocaleFormat("%h")');
*/

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

/*
expect = '\n';
actual = date.toLocaleFormat('%n');
reportCompare(expect, actual, 'Date.toLocaleFormat("%n") == "\\n"');
*/

expect = true;
temp   = date.toLocaleFormat('%p');
actual = temp == 'AM' || date.toLocaleFormat('%p') == 'PM';
reportCompare(expect, actual, 'Date.toLocaleFormat("%p") is AM or PM');

/*
expect = date.toLocaleFormat('%I:%M:%S %p');
actual = date.toLocaleFormat('%r');
reportCompare(expect, actual, 'Date.toLocaleFormat("%I:%M:%S %p") == ' +
              'Date.toLocaleFormat("%r")');
*/

/*
expect = date.toLocaleFormat('%H:%M');
actual = date.toLocaleFormat('%R');
reportCompare(expect, actual, 'Date.toLocaleFormat("%H:%M") == ' +
              'Date.toLocaleFormat("%R")');
*/

expect = '00';
actual = date.toLocaleFormat('%S');
reportCompare(expect, actual, 'Date.toLocaleFormat("%S")');

/*
expect = '\t';
actual = date.toLocaleFormat('%t');
reportCompare(expect, actual, 'Date.toLocaleFormat("%t") == "\\t"');
*/

/*
expect = date.toLocaleFormat('%H:%M:%S');
actual = date.toLocaleFormat('%T');
reportCompare(expect, actual, 'Date.toLocaleFormat("%H:%M:%S") == ' +
              'Date.toLocaleFormat("%T")');
*/

/*
expect = String(6 + ((date.getTimezoneOffset() > 0) ? 0 : 1));
actual = date.toLocaleFormat('%u');
reportCompare(expect, actual, 'Date.toLocaleFormat("%u")');
*/

expect = String(22 + ((date.getTimezoneOffset() > 0) ? 0 : 1));
actual = date.toLocaleFormat('%U');
reportCompare(expect, actual, 'Date.toLocaleFormat("%U")');

/*
expect = '22';
actual = date.toLocaleFormat('%V');
reportCompare(expect, actual, 'Date.toLocaleFormat("%V")');
*/

expect = String((6 + ((date.getTimezoneOffset() > 0) ? 0 : 1))%7);
actual = date.toLocaleFormat('%w');
reportCompare(expect, actual, 'Date.toLocaleFormat("%w")');

expect = '22';
actual = date.toLocaleFormat('%W');
reportCompare(expect, actual, 'Date.toLocaleFormat("%W")');

/*
expect = date.toLocaleFormat('%m/%d/%y');
actual = date.toLocaleFormat('%x');
reportCompare(expect, actual, 'Date.toLocaleFormat("%m/%d/%y) == ' + 
              'Date.toLocaleFormat("%x")');
*/

expect = date.toLocaleFormat('%H:%M:%S');
actual = date.toLocaleFormat('%X');
reportCompare(expect, actual, 'Date.toLocaleFormat("%H:%M:%S) == ' + 
              'Date.toLocaleFormat("%X")');

expect = '05';
actual = date.toLocaleFormat('%y');
reportCompare(expect, actual, 'Date.toLocaleFormat("%y")');

expect = '2005';
actual = date.toLocaleFormat('%Y');
reportCompare(expect, actual, 'Date.toLocaleFormat("%Y")');

/*
expect = String(-date.getTimezoneOffset()/60); // assume integral hours
actual = Number(date.toLocaleFormat('%z'))/100;
reportCompare(expect, actual, 'Date.getTimezoneOffset() == ' +
              'Date.toLocaleFormat("%z")');
*/

expect = true;
temp   = date.toLocaleFormat('%Z');
actual = temp.indexOf('Time') > 0;
reportCompare(expect, actual, 'Date.toLocaleFormat("%Z")');

expect = '%';
actual = date.toLocaleFormat('%%');
reportCompare(expect, actual, 'Date.toLocaleFormat("%%")');
