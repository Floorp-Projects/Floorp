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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Mike Potter <mikep@oeone.com>
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
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


// scriptable date formater, for pretty printing dates
var nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;

var dateService = Components.classes["@mozilla.org/intl/scriptabledateformat;1"].getService(nsIScriptableDateFormat);

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
// Number of full days corrected by the TimezoneOffset in order to adapt to 
// Daylight Saving Time changes.
DateUtils.getDifferenceInDays = function(StartDate, EndDate)
{
  var msOffset = ( StartDate.getTimezoneOffset() - EndDate.getTimezoneOffset() ) * kDate_MillisecondsInMinute ;
  return( (EndDate.getTime() + msOffset - StartDate.getTime() ) / kDate_MillisecondsInDay) ; 
}

DateUtils.getWeekNumber = function(date)
{
  //ISO Week Number(01-53) for any date
  //code adapted from http://www.pvv.org/~nsaa/ISO8601.html
  ///////////////////
  // Calculate some date for this Year
  ///////////////////
  var Year = date.getFullYear();
  var FirstOfYear = new Date(Year,0,1) ;
  var LastOfYear = new Date(Year,11,31) ;
  var FirstDayNum = FirstOfYear.getDay() ;
  // ISO weeks start on Monday (day 1) and ends on Sunday (day 7) 
  var ISOFirstDayNum = (FirstDayNum ==0)? 7 : FirstDayNum ;
  // Week 1 of any year is the week that contains the first Thursday
  // in January : true if 1 Jan = mon - thu. WeekNumber is then 1
  var IsFirstWeek = ( ( 7 - ISOFirstDayNum ) > 2 )? 1 : 0 ;
  // The first Monday after 1 Jan this Year
  var FirstMonday = 9 - ISOFirstDayNum ;
  // Number of Days from 1 Jan to date
  var msOffset = ( FirstOfYear.getTimezoneOffset() - date.getTimezoneOffset() ) * kDate_MillisecondsInMinute;
  var DaysToDate = Math.floor((date.getTime()+msOffset-FirstOfYear.getTime())/kDate_MillisecondsInDay)+1 ;
  // Number of Days in Year (either 365 or 366);
  var DaysInYear =  (LastOfYear.getTime()-FirstOfYear.getTime())/kDate_MillisecondsInDay+1 ;
  // Number of Weeks in Year. Most years have 52 weeks, but years that start on
  // a Thursday and leapyears that starts on a Wednesday or a Thursday have 53 weeks
  var NumberOfWeeksThisYear = 
          (ISOFirstDayNum==4 || (ISOFirstDayNum==3 && DaysInYear==366)) ? 53 : 52;

  // "***********************************";
  // " Calculate some data for last Year ";
  // "***********************************";
  var FirstOfLastYear = new Date(Year-1,0,1) ;
  var LastOfLastYear = new Date(Year-1,11,31) ;
  var FirstDayNumLast = FirstOfLastYear.getDay() ;
  // ISO weeks start on Monday (day 1) and ends on Sunday (day 7) 
  var ISOFirstDayNumLast = (FirstDayNumLast ==0)? 7 : FirstDayNumLast ;
  // Number of Days in Year (either 365 or 366);
  var DaysInLastYear =  (LastOfLastYear.getTime()-FirstOfLastYear.getTime())/kDate_MillisecondsInDay+1 ;
  // Number of Weeks in Year. Most years have 52 weeks, but years that start on
  // a Thursday and leapyears that starts on a Wednesday or a Thursday have 53 weeks
  var NumberOfWeeksLastYear = 
          (ISOFirstDayNumLast==4 || (ISOFirstDayNumLast==3 && DaysInLastYear==366)) ? 53 : 52;
  // "****************************";
  // " Calculates the Week Number ";
  // "****************************";
  var DateDayNum = date.getDay() ;
  var ISODateDayNum = (DateDayNum ==0)? 7 : DateDayNum ;
  // Is Date in the last Week of last Year ?
  if( (DaysToDate < FirstMonday) &&  (IsFirstWeek == 0) ) 
     return(NumberOfWeeksLastYear) ;
  // "Calculate number of Complete Weeks Between D and 1.jan";
  var ComplNumWeeks = Math.floor((DaysToDate-FirstMonday)/7);
  // "Are there remaining days?";
  var RemainingDays = ( (DaysToDate+1-(FirstMonday+7*ComplNumWeeks))>0 );

  var NumWeeks = IsFirstWeek+ComplNumWeeks+1;
  if(RemainingDays) 
  {
    if(NumWeeks>52 && NumWeeks>NumberOfWeeksThisYear )
      return( 1 );
    else
      return( NumWeeks );   
  }
  else 
    return( NumWeeks - 1 );   
}


function DateFormater( )
{
  // we get the date bundle in case the locale changes, can
  // we be notified of a locale change instead, then we could
  // get the bundle once.
  this.dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

  // probe the dateformat  
  this.yearIndex = -1;
  this.monthIndex = -1;
  this.dayIndex = -1;
  this.twoDigitYear = false;
  
  var parseShortDateRegex = /^\s*(\d+)\D(\d+)\D(\d+)\s*$/; //digits & nonDigits
  var probeDate = new Date(2002,3-1,4); // month is 0-based
  var probeString = this.getShortFormatedDate(probeDate);

  var probeArray = parseShortDateRegex.exec(probeString);
  for (var i = 1; i <= 3; i++) { 
    switch (Number(probeArray[i])) {
      case 02:    this.twoDigitYear = true; // fall thru
      case 2002:  this.yearIndex = i;       break;
      case 3:     this.monthIndex = i;      break;
      case 4:     this.dayIndex = i;        break;
    }
  }
  //All three indexes are set (not -1) at this point.
}


DateFormater.prototype.getFormatedTime = function( date )
{
   return( dateService.FormatTime( "", dateService.timeFormatNoSeconds, date.getHours(), date.getMinutes(), 0 ) ); 
}


DateFormater.prototype.getFormatedDate = function( date )
{
   // Format the date using a hardcoded format for now, since everything
   // that displays the date uses this function we will be able to 
   // make a user settable date format and use it here.

   try
   {     
      if( getIntPref(gCalendarWindow.calendarPreferences.calendarPref, "date.format", 0 ) == 0 )
         return( this.getLongFormatedDate( date ) );
      else
         return( this.getShortFormatedDate( date ) );
   }
   catch(ex)
   {
      return "";
   }
}

DateFormater.prototype.getLongFormatedDate = function( date )
{
   if( (navigator.platform.indexOf("Win") == 0) || (navigator.platform.indexOf("Mac") != -1) )
   {
      return( dateService.FormatDate( "", dateService.dateFormatLong, date.getFullYear(), date.getMonth()+1, date.getDate() ) );
   }
   else
   {
      // HACK We are probably on Linux and want a string in long format.
      // dateService.dateFormatLong on Linux returns a short string, so build our own
      // this should move into Mozilla or libxpical
      var oneBasedMonthNum = date.getMonth() + 1;
      var monthString = this.dateStringBundle.GetStringFromName("month." + oneBasedMonthNum + ".Mmm" );
      var dateString =  monthString + " " + date.getDate()+", "+date.getFullYear();
      return dateString;
   }
}


DateFormater.prototype.getShortFormatedDate = function( date )
{
   return( dateService.FormatDate( "", dateService.dateFormatShort, date.getFullYear(), date.getMonth()+1, date.getDate() ) );
}


DateFormater.prototype.getFormatedDateWithoutYear = function( date )
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

/**** parseShortDate
 * Parameter dateString may be a date or a date+time. Dates are
 * read acording to locale (d-m-y or m-d-y or ...). Times are 
 * read in 12- or 24-hour formats. Only hours and minutes are
 * taken into account (dateString may contain secs and msecs though).
 */

DateFormater.prototype.parseShortDate = function ( dateString )
{ 
  var parseShortDateRegex = /^\s*(\d+)\D(\d+)\D(\d+)(.*)?$/; 
  var parseTimeRegex = /\s*(\d+)\D(\d+)(\d|\W)*(pm|PM|am|AM|)\s*$/;
  
  // parse dateString
  var dateNumbersArray = parseShortDateRegex.exec(dateString);
  if (dateNumbersArray != null) {
    var year = Number(dateNumbersArray[this.yearIndex]);
    if (this.twoDigitYear && 0 <= year && year < 100) {
      // If 2-digit year format and 0 <= year < 100,
      //   parse year as up to 30 years in future or 69 years in past.
      //   (Covers 30-year mortgage and most working peoples birthdate.)
      var currentYear = 1900 + new Date().getYear(); // getYear 0 is 1900.
      var currentCentury = currentYear - currentYear % 100;
      year = currentCentury + year;
      if (year < currentYear - 69)
        year += 100;
      if (year > currentYear + 30)
        year -= 100;
    }
    var resultDate;
    var len = dateNumbersArray.length;
    resultDate = new Date(year, // four-digit year
                 Number(dateNumbersArray[this.monthIndex]) - 1, // 0-based
                 Number(dateNumbersArray[this.dayIndex]));

    // parse last string in dateNumbersArray to get time
    var timeNumbersArray = parseTimeRegex.exec(dateNumbersArray[4]);
    if (timeNumbersArray != null) {
      if ((timeNumbersArray[1]<12) && 
          ((timeNumbersArray[4] == "pm") || (timeNumbersArray[4] == "PM"))) 
        //12 hour clock
        resultDate.setHours(Number(timeNumbersArray[1])+12, timeNumbersArray[2]);
      else
        //24 hour clock
        resultDate.setHours(timeNumbersArray[1], timeNumbersArray[2]);
    }
    return resultDate;
  } else
    return null; // did not match regex, not a valid date
}
