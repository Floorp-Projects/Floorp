/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley (garths@oeone.com)
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
 * 
*/

var kDate_MillisecondsInSecond = 1000;
var kDate_SecondsInMinute      = 60;
var kDate_MinutesInHour        = 60;
var kDate_HoursInDay           = 24;
var kDate_DaysInWeek           = 7;

var kDate_SecondsInHour        = 3600;
var kDate_SecondsInDay         = 86400
var kDate_SecondsInWeek        = 604800

var kDate_MinutesInDay         = 1440;
var kDate_MinutesInWeek        = 10080;


var kDate_MillisecondsInMinute =     60000;
var kDate_MillisecondsInHour   =   3600000;
var kDate_MillisecondsInDay    =  86400000;
var kDate_MillisecondsInWeek   = 604800000;


// required includes: "chrome://global/content/strres.js" - String Bundle Code

function DateUtils()
{

}

DateUtils.getLastDayOfMonth = function( year, month  )
{
   var pastLastDate = new Date( year, month, 32 );
   var lastDayOfMonth = 32 - pastLastDate.getDate();
   
   return lastDayOfMonth;
 
}




function DateFormater()
{
   // we get the date bundle in case the locale changes, can
   // we be notified of a locale change instead, then we could
   // get the bundle once.
   
   this.dateStringBundle = srGetStrBundle("chrome://penglobal/locale/dateFormat.properties");

}


DateFormater.prototype.getFormatedTime = function( date )
{
   // Format the time using a hardcoded format for now, since everything
   // that displays the time uses this function we will be able to 
   // make a user settable time format and use it here.

   var hour = date.getHours();
   var min = date.getMinutes();
   
   // compute am and pm
   
   if( hour < 12 )
   {
      var ampm = this.dateStringBundle.GetStringFromName( "am-string" );
   }
   else
   {
      var ampm = this.dateStringBundle.GetStringFromName( "pm-string" );
   }
   
   // convert to 12 hour clock
   
   if( hour > 12 )
   {
      hour -= 12;
   }

   // make 0 be midnight
   
   if( hour == 0  )
   {
      hour = 12;
   }
   
   // put two zeros in the minute
   
   var minString = min.toString();
   
   if( min < 10 )
   {
      minString = "0" + min;
   }
   
   // Make the time to display
   
   var timeString = hour + ":" + minString + " " + ampm;
   
   return timeString;

}


DateFormater.prototype.getFormatedDate = function( date )
{
   // Format the date using a hardcoded format for now, since everything
   // that displays the date uses this function we will be able to 
   // make a user settable date format and use it here.

   var oneBasedMonthNum = date.getMonth() + 1;
   
   var monthString = this.dateStringBundle.GetStringFromName("month." + oneBasedMonthNum + ".Mmm" );
   
   var dateString =  monthString + " " + date.getDate();
   
   return dateString;
}


// 0-11 Month Index

DateFormater.prototype.getMonthName = function( monthIndex )
{

   var oneBasedMonthNum = monthIndex + 1;
   
   var monthName = this.dateStringBundle.GetStringFromName("month." + oneBasedMonthNum + ".name" );
   
   return monthName;
}


// 0-11 Month Index

DateFormater.prototype.getShortMonthName = function( monthIndex )
{

   var oneBasedMonthNum = monthIndex + 1;
   
   var monthName = this.dateStringBundle.GetStringFromName("month." + oneBasedMonthNum + ".Mmm" );
   
   return monthName;
}


// 0-6 Day index ( starts at Sun )

DateFormater.prototype.getDayName = function( dayIndex )
{

   var oneBasedDayNum = dayIndex + 1;
   
   var dayName = this.dateStringBundle.GetStringFromName("day." + oneBasedDayNum + ".name" );
   
   return dayName;
}


// 0-6 Day index ( starts at Sun )

DateFormater.prototype.getShortDayName = function( dayIndex )
{

   var oneBasedDayNum = dayIndex + 1;
   
   var dayName = this.dateStringBundle.GetStringFromName("day." + oneBasedDayNum + ".Mmm" );
   
   return dayName;
}





