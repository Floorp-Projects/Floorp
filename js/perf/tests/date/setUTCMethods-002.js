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
 * The Original Code is JavaScript Engine performance test.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mazie Lobo     mazielobo@netscape.com
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
 * ***** END LICENSE BLOCK ***** 
 *
 *
 * Date:    11 October 2002
 * SUMMARY: Calling various |setUTC()| methods for the |Date()| object.
           Sets accoriding to universal time. 
 *
 * Syntax:  
 *         |setUTCFullYear(yearValue[, monthValue, dayValue])| 
 *         yearValue - An integer specifying the numeric value of the year.  
 *         monthValue - An integer between 0 and 11 representing months 
 *         January through December.  
 *         dayValue - An integer between 1 and 31 representing the day of the 
 *         month. If you specify the dayValue parameter, you must also specify 
 *         the monthValue.
 *
 *         |setUTCHours(hoursValue[, minutesValue, secondsValue, msValue]) |
 *         hoursValue - An integer between 0 and 23, representing the hour.  
 *         minutesValue - An integer between 0 and 59, representing minutes.  
 *         secondsValue - An integer between 0 and 59, representing seconds. 
 *         If you specify the secondsValue parameter, you must also specify the 
 *         minutesValue.  
 *         msValue - A number between 0 and 999, representing the milliseconds. 
 *         If you specify the msValue parameter, you must also specify the 
 *         minutesValue and secondsValue.  
 *
 *         |setUTCMinutes(minutesValue[, secondsValue, msValue]) |
 *         minutesValue - An integer between 0 and 59, representing minutes.  
 *         secondsValue - An integer between 0 and 59, representing seconds. 
 *         If you specify the secondsValue parameter, you must also specify the 
 *         minutesValue.  
 *         msValue - A number between 0 and 999, representing milliseconds. 
 *         If you specify the msValue parameter, you must also specify the 
 *         minutesValue and secondsValue. 
 *
 *         |setUTCMonth(monthValue[, dayValue]) |
 *         monthValue - An integer between 0 and 11 (representing the months 
 *         January through December).  
 *         dayValue - An integer from 1 to 31, representing the day of the 
 *         month.  
 */
//-----------------------------------------------------------------------------
var UBOUND=100000;

test();

function test()
{
  var u_bound = UBOUND;
  var status;
  var start;
  var end;
  var i;
  var dt;
  var dayValue = 1;
  var monthValue = 1;
  var yearValue = 2002;
  var hoursValue = 0;
  var minutesValue = 0;
  var secondsValue = 0;
  var msValue = 0;
  var timeValue = 0;
  
  status = "dt.setUTCFullYear(yearValue[, monthValue, dayValue])";
  dt = new Date();
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.setUTCFullYear(yearValue, monthValue, dayValue);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.setUTCHourshoursValue[, minutesValue, secondsValue, msValue])";
  dt = new Date();
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.setUTCHours(hoursValue, minutesValue, secondsValue, msValue);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.setUTCMinutes(minutesValue[, secondsValue, msValue])";
  dt = new Date();
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.setUTCMinutes(minutesValue, secondsValue, msValue);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.setUTCMonth(monthValue[, dayValue])";
  dt = new Date();
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.setUTCMonth(monthValue, dayValue);
  }
  end = new Date();
  reportTime(status, start, end);
}
