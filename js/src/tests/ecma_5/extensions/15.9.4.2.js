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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bruce Hoult
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
var BUGNUMBER = 682754;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function iso(d)
{
  return new Date(d).toISOString();
}

function check(s, millis){
  description = "Date.parse('"+s+"') == '"+iso(millis)+"'";
  expected = millis;
  actual = Date.parse(s);
  reportCompare(expected, actual, description);
}

function checkInvalid(s)
{
  description = "Date.parse('"+s+"') produces invalid date";
  expected = NaN;
  actual = Date.parse(s);
  reportCompare(expected, actual, description);
}

function dd(year, month, day, hour, minute, second, millis){
  return Date.UTC(year, month-1, day, hour, minute, second, millis);
}

function TZAtDate(d){
  return d.getTimezoneOffset() * 60000;
}

function TZInMonth(month){
  return TZAtDate(new Date(dd(2009,month,1,0,0,0,0)));
}

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);

  JanTZ = TZInMonth(1);
  JulTZ = TZInMonth(7);
  CurrTZ = TZAtDate(new Date());

  // Allow non-standard "-0700" as timezone, not just "-07:00"
  check("2009-07-23T00:53:21.001-0700", dd(2009,7,23,7,53,21,1));

  exitFunc ('test');
}
