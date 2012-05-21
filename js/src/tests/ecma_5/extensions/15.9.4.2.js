/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
