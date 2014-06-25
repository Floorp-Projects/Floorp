/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 430930;


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

  // formats with explicit timezone
  check("2009-07-23T19:53:21.001+12:00", dd(2009,7,23,7,53,21,1));
  check("2009-07-23T19:53:21+12:00",     dd(2009,7,23,7,53,21,0));
  check("2009-07-23T19:53+12:00",        dd(2009,7,23,7,53,0,0));

  check("2009-07T19:53:21.001+12:00",    dd(2009,7,1,7,53,21,1));
  check("2009-07T19:53:21+12:00",        dd(2009,7,1,7,53,21,0));
  check("2009-07T19:53+12:00",           dd(2009,7,1,7,53,0,0));

  check("2009T19:53:21.001+12:00",       dd(2009,1,1,7,53,21,1));
  check("2009T19:53:21+12:00",           dd(2009,1,1,7,53,21,0));
  check("2009T19:53+12:00",              dd(2009,1,1,7,53,0,0));

  check("T19:53:21.001+12:00",           dd(1970,1,1,7,53,21,1));
  check("T19:53:21+12:00",               dd(1970,1,1,7,53,21,0));
  check("T19:53+12:00",                  dd(1970,1,1,7,53,0,0));

  // formats without timezone uses the timezone as at that date
  check("2009-07-23T19:53:21.001",       dd(2009,7,23,19,53,21,1)+JulTZ);
  check("2009-07-23T19:53:21",           dd(2009,7,23,19,53,21,0)+JulTZ);
  check("2009-07-23T19:53",              dd(2009,7,23,19,53,0,0)+JulTZ);

  check("2009-07T19:53:21.001",          dd(2009,7,1,19,53,21,1)+JulTZ);
  check("2009-07T19:53:21",              dd(2009,7,1,19,53,21,0)+JulTZ);
  check("2009-07T19:53",                 dd(2009,7,1,19,53,0,0)+JulTZ);

  check("2009T19:53:21.001",             dd(2009,1,1,19,53,21,1)+JanTZ);
  check("2009T19:53:21",                 dd(2009,1,1,19,53,21,0)+JanTZ);
  check("2009T19:53",                    dd(2009,1,1,19,53,0,0)+JanTZ);

  check("T19:53:21.001",                 dd(1970,1,1,19,53,21,1)+JanTZ);
  check("T19:53:21",                     dd(1970,1,1,19,53,21,0)+JanTZ);
  check("T19:53",                        dd(1970,1,1,19,53,0,0)+JanTZ);

  // with no time at all assume UTC
  check("2009-07-23",                    dd(2009,7,23,0,0,0,0));
  check("2009-07",                       dd(2009,7,1,0,0,0,0));
  check("2009",                          dd(2009,1,1,0,0,0,0));

  // one field too big
  checkInvalid("2009-13-23T19:53:21.001+12:00");
  checkInvalid("2009-07-32T19:53:21.001+12:00");
  checkInvalid("2009-07-23T25:53:21.001+12:00");
  checkInvalid("2009-07-23T19:60:21.001+12:00");
  checkInvalid("2009-07-23T19:53:60.001+12:00");
  checkInvalid("2009-07-23T19:53:21.001+24:00");
  checkInvalid("2009-07-23T19:53:21.001+12:60");

  // other month ends
  check("2009-06-30T19:53:21.001+12:00", dd(2009,6,30,7,53,21,1));
  checkInvalid("2009-06-31T19:53:21.001+12:00");
  check("2009-02-28T19:53:21.001+12:00", dd(2009,2,28,7,53,21,1));
  checkInvalid("2009-02-29T19:53:21.001+12:00");
  check("2008-02-29T19:53:21.001+12:00", dd(2008,2,29,7,53,21,1));
  checkInvalid("2008-02-30T19:53:21.001+12:00");

  // limits of representation
  checkInvalid("-271821-04-19T23:59:59.999Z");
  check("-271821-04-20", -8.64e15);
  check("+275760-09-13", 8.64e15);
  checkInvalid("+275760-09-13T00:00:00.001Z");

  check("-269845-07-23T19:53:21.001+12:00", dd(-269845,7,23,7,53,21,1));
  check("+273785-07-23T19:53:21.001+12:00", dd(273785,7,23,7,53,21,1));

  // explicit UTC
  check("2009-07-23T19:53:21.001Z", dd(2009,7,23,19,53,21,1));
  check("+002009-07-23T19:53:21.001Z", dd(2009,7,23,19,53,21,1));

  // different timezones
  check("2009-07-23T19:53:21.001+12:00", dd(2009,7,23,7,53,21,1));
  check("2009-07-23T00:53:21.001-07:00", dd(2009,7,23,7,53,21,1));

  // 00:00 and 24:00
  check("2009-07-23T00:00:00.000-07:00", dd(2009,7,23,7,0,0,0));
  check("2009-07-23T24:00:00.000-07:00", dd(2009,7,24,7,0,0,0));

  exitFunc ('test');
}
