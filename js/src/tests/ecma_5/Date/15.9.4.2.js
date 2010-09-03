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
var BUGNUMBER = 430930;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function iso(d){
  return new Date(d).toISOString();
}

function check(s, millis){
  description = "Date.parse('"+s+"') == '"+iso(millis)+"'";
  expected = millis;
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
  check("",                              NaN);

  // one field too big
  check("2009-13-23T19:53:21.001+12:00", NaN);
  check("2009-07-32T19:53:21.001+12:00", NaN);
  check("2009-07-23T25:53:21.001+12:00", NaN);
  check("2009-07-23T19:60:21.001+12:00", NaN);
  check("2009-07-23T19:53:60.001+12:00", NaN);
  check("2009-07-23T19:53:21.001+24:00", NaN);
  check("2009-07-23T19:53:21.001+12:60", NaN);

  // other month ends
  check("2009-06-30T19:53:21.001+12:00", dd(2009,6,30,7,53,21,1));
  check("2009-06-31T19:53:21.001+12:00", NaN);
  check("2009-02-28T19:53:21.001+12:00", dd(2009,2,28,7,53,21,1));
  check("2009-02-29T19:53:21.001+12:00", NaN);
  check("2008-02-29T19:53:21.001+12:00", dd(2008,2,29,7,53,21,1));
  check("2008-02-30T19:53:21.001+12:00", NaN);

  // limits of representation
  check("-271821-04-19T23:59:59.999Z", NaN);
  check("-271821-04-20", -8.64e15);
  check("+275760-09-13", 8.64e15);
  check("+275760-09-13T00:00:00.001Z", NaN);

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
