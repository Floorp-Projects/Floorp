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
  this.alphaMonths = null;
  this.probeSucceeded = false;
  
  // SHORT NUMERIC DATE, such as 2002-03-04, 4/3/2002, or CE2002Y03M04D.
  // Made of digits & nonDigits.  [^\d\W]* is letters-only optional prefix 
  // for era, as in CE2002Y03M04D, used in chinese short dates to distinguish 
  // western Common-Era year from year in chinese calendar.
  var parseShortDateRegex = /^\s*[^\d\W]*(\d+)\D(\d+)\D(\d+)\D?\s*$/;
  var probeDate = new Date(2002,3-1,4); // month is 0-based
  var probeString = this.getShortFormatedDate(probeDate);

  var probeArray = parseShortDateRegex.exec(probeString);

  if (probeArray != null) { 
    // Numeric month format
    for (var i = 1; i <= 3; i++) { 
      switch (Number(probeArray[i])) {
        case 02:    this.twoDigitYear = true; // fall thru
        case 2002:  this.yearIndex = i;       break;
        case 3:     this.monthIndex = i;      break;
        case 4:     this.dayIndex = i;        break;
      }
    }
    //All three indexes are set (not -1) at this point.
    this.probeSucceeded = true;
  } else {
    // SHORT DATE WITH ALPHABETIC MONTH, such as "dd MMM yy" or "MMMM dd, yyyy"
    // (\d+|[^\d\W]) is digits or letters, not both together.
    // Allows 31dec1999 (no delimiters between parts) if OS does (w2k does not).
    // Allows Dec 31, 1999 (comma and space between parts)
    parseShortDateRegex = /^\s*(\d+|[^\d\W]+)\W{0,2}(\d+|[^\d\W]+)\W{0,2}(\d+|[^\d\W]+)\s*$/;
    probeArray = parseShortDateRegex.exec(probeString);
    if (probeArray != null) {
      for (var j = 1; j <= 3; j++) { 
        switch (Number(probeArray[j])) {
          case 02:    this.twoDigitYear = true; // fall thru
          case 2002:  this.yearIndex = j;       break;
          case 4:     this.dayIndex = j;        break;
          default:    this.monthIndex = j;      break;
        }
      }
      if (this.yearIndex != -1 && this.dayIndex != -1 && this.monthIndex != -1) {
        this.probeSucceeded = true;
        // Fill this.alphaMonths with month names.
        this.alphaMonths = new Array(12);
        for (var m = 0; m < 12; m++) {
          probeDate.setMonth(m);
          probeString = this.getShortFormatedDate(probeDate);
          probeArray = parseShortDateRegex.exec(probeString);
          if (probeArray != null) 
            this.alphaMonths[m] = probeArray[this.monthIndex].toUpperCase();
          else
            this.probeSucceeded = false;
        }
      }
    }
    if (! this.probeSucceeded)
      dump("\nOperating system short date format is not recognized: "+probeString+"\n");
  }

  // If LONG FORMATTED DATE is same as short formatted date,
  // then OS has poor extended/long date config, so use workaround.
  this.useLongDateService = true;
  try { 
    var longProbeString = this.getLongFormatedDate(probeDate);
    // On Unix extended/long date format may be created using %Ex instead of %x.
    // Some systems may not support it and return "Ex" or same as short string.
    // In that case, don't use long date service, use workaround hack instead.
    if (longProbeString == null || longProbeString.length < 4 || longProbeString == probeString)
      this.useLongDateService = false;
  } catch (error) {
    this.useLongDateService = false;
  }
}


DateFormater.prototype.getFormatedTime = function( date )
{
   return( dateService.FormatTime( "", dateService.timeFormatNoSeconds, date.getHours(), date.getMinutes(), 0 ) ); 
}


DateFormater.prototype.getFormatedDate = function( date )
{
   // Format the date using user's format preference (long or short)
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
  if (this.useLongDateService)
  {
    return( dateService.FormatDate( "", dateService.dateFormatLong, date.getFullYear(), date.getMonth()+1, date.getDate() ) );
  }
  else
  {
    // HACK We are probably on Linux and want a string in long format.
    // dateService.dateFormatLong on Linux may return a short string, so build our own
    // this should move into Mozilla or libxpical
    var oneBasedMonthNum = date.getMonth() + 1;
    var monthString = this.dateStringBundle.GetStringFromName("month." + oneBasedMonthNum + ".Mmm" );
    var oneBasedWeekDayNum = date.getDay() + 1;
    var weekDayString = this.dateStringBundle.GetStringFromName("day."+ oneBasedWeekDayNum + ".Mmm" );
    var dateString =  weekDayString+" "+date.getDate()+" "+monthString+" "+date.getFullYear();
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
 * Parameter dateString may be a date or a date time. Dates are
 * read according to locale/OS setting (d-m-y or m-d-y or ...). 
 * (see constructor).  See parseTimeOfDay for times.
 */
DateFormater.prototype.parseShortDate = function ( dateString )
{ 
  if (!this.probeSucceeded)
    return null; // avoid errors accessing uninitialized data.

  var year = Number.MIN_VALUE; var month = -1; var day = -1; var timeString = null;
  if (this.alphaMonths == null) {
    // SHORT NUMERIC DATE, such as 2002-03-04, 4/3/2002, or CE2002Y03M04D.
    // Made of digits & nonDigits.  [^\d\W]* is letters-only optional prefix
    // for era, as in CE2002Y03M04D, used in chinese short dates to distinguish
    // western Common-Era year from year in chinese calendar.
    var parseNumShortDateRegex = /^\s*[^\d\W]*(\d+)\D(\d+)\D(\d+)(.*)?$/;
    var dateNumbersArray = parseNumShortDateRegex.exec(dateString);
    if (dateNumbersArray != null) {
      year = Number(dateNumbersArray[this.yearIndex]);
      month = Number(dateNumbersArray[this.monthIndex]) - 1; // 0-based
      day = Number(dateNumbersArray[this.dayIndex]);
      timeString = dateNumbersArray[4];
    }
  } else {
    // SHORT DATE WITH ALPHABETIC MONTH, such as "dd MMM yy" or "MMMM dd, yyyy"
    // (\d+|[^\d\W]) is digits or letters, not both together.
    // Allows 31dec1999 (no delimiters between parts) if OS does (w2k does not).
    // Allows Dec 31, 1999 (comma and space between parts)
    var parseAlphShortDateRegex = /^\s*(\d+|[^\d\W]+)\W{0,2}(\d+|[^\d\W]+)\W{0,2}(\d+|[^\d\W]+)(.*)?$/;
    var datePartsArray = parseAlphShortDateRegex.exec(dateString);
    if (datePartsArray != null) {
      year = Number(datePartsArray[this.yearIndex]);
      var monthString = datePartsArray[this.monthIndex].toUpperCase();
      for (var m = 0; m < this.alphaMonths.length; m++) {
        if (monthString == this.alphaMonths[m]) {
          month = m;
          break;
        }
      }
      day = Number(datePartsArray[this.dayIndex]);
      timeString = datePartsArray[4];
    }
  }
  if (year != Number.MIN_VALUE && month != -1 && day != -1) {
    // year, month, day successfully parsed
    if (0 <= year && year < 100) {
      // If 0 <= year < 100, treat as 2-digit year(like dateService.FormatDate):
      //   parse year as up to 30 years in future or 69 years in past.
      //   (Covers 30-year mortgage and most working people's birthdate.)
      // otherwise will be treated as four digit year.
      var currentYear = new Date().getFullYear();
      var currentCentury = currentYear - currentYear % 100;
      year = currentCentury + year;
      if (year < currentYear - 69)
        year += 100;
      if (year > currentYear + 30)
        year -= 100;
    }
    // if time is also present, parse it
    var hours = 0; var minutes = 0; var seconds = 0;
    if (timeString != null) {
      var time = this.parseTimeOfDay(dateNumbersArray[4]);
      if (time != null) {
        hours = time.getHours();
        minutes = time.getMinutes();
        seconds = time.getSeconds();
      }
    }
    return new Date(year, month, day, hours, minutes, seconds, 0);
  } else { 
    return null; // did not match regex, not a valid date
  }
}

// Parse a variety of time formats so that cut and paste is likely to work.
// separator:            ':'         '.'        ' '        symbol        none
//                       "12:34:56"  "12.34.56" "12 34 56" "12h34m56s"   "123456"
// seconds optional:     "02:34"     "02.34"    "02 34"    "02h34m"      "0234"
// minutes optional:     "12"        "12"       "12"       "12h"         "12"
// 1st hr digit optional:"9:34"      " 9.34"     "9 34"     "9H34M"       "934am"
// am/pm optional        "02:34 a.m.""02.34pm"  "02 34 AM" "02H34M P.M." "0234pm"
// skip nondigit prefix  " 12:34"    "t12.34"   " 12 34"   "T12H34M"     "T0234"
// Todo: 4x fails if x is localized version of "am" or pm"
DateFormater.prototype.parseTimeOfDay = function( timeString )
{
  var parseTimeRegex =
    /^\D*(\d?\d)(\D)?(?:(\d\d)(\D)?(?:(\d\d)(\D)?)?)?([AaPp][.]?[Mm][.]?)?\s*$/;
  var timePartsArray = parseTimeRegex.exec(timeString);
  if (timePartsArray != null) {
    var hoursString   = timePartsArray[1]
    var hours         = Number(hoursString);
    var hoursSuffix   = timePartsArray[2];
    if (!(0 <= hours && hours < 24)) return null;

    var minutesString = timePartsArray[3];
    var minutes       = (minutesString == null? 0 : Number(minutesString));
    var minutesSuffix = timePartsArray[4];
    if (!(0 <= minutes && minutes < 60)) return null;

    var secondsString = timePartsArray[5];
    var seconds       = (secondsString == null? 0 : Number(secondsString));
    var secondsSuffix = timePartsArray[6];
    if (!(0 <= seconds && seconds < 60)) return null;

    var ampmString   = timePartsArray[7];
    if (ampmString != null) {
      if (!(1 <= hours && hours <= 12)) return null;
      if (ampmString.toLowerCase().charAt(0) == 'a') /* a.m. */ {
        if (hours == 12)
          hours = 0;
      } else /* p.m. */ {
        if (hours < 12) 
          hours += 12;
      }
    }
    return new Date(0, 0, 0, hours, minutes, seconds, 0);
  } else return null; // did not match regex, not valid time
}

/** Formats start/end dates and times, omitting end if same as start, 
    omitting time if allDay, omitting end date if same day.
    Calls getFormatedDate and getFormatedTime, so user preference will be used.

    startDateTime: beginning of time interval.
    endDateTime: exclusive end of timeInterval.
        If isAllDay for one day, endDateTime is midnight next day, so displayed
        end date will be one day earlier.

    Examples (with user date format yyyy-MM-dd, time format HH:mm)
      1999-12-31 00:00, 2000-01-01 00:00, true  --> "1999-12-31"
      1999-12-31 00:00, 2000-01-02 00:00, true  --> "1999-12-31--2001-01-01"
      1999-12-31 20:00, 1999-12-31 20:00, false --> "1999-12-31 20:00"
      1999-12-31 20:00, 1999-12-31 22:00, false --> "1999-12-31 20:00--22:00"
      1999-12-31 00:00, 2000-01-01 00:00, false --> "1999-12-31 00:00 -- 2001-01-01 00:00"
 **/
DateFormater.prototype.formatInterval = function( startDateTime, endDateTime, isAllDay ) {
  if (isAllDay) { 
    endDateTime = new Date(endDateTime); // don't modify parameter
    endDateTime.setDate(endDateTime.getDate() - 1);
  }
  var sameDay = (startDateTime.getFullYear() == endDateTime.getFullYear() &&
                 startDateTime.getMonth() == endDateTime.getMonth() &&
                 startDateTime.getDay() == endDateTime.getDay());
  var sameTime = (startDateTime.getHours() == endDateTime.getHours() &&
                  startDateTime.getMinutes() == endDateTime.getMinutes());
  return (isAllDay
          ? (sameDay
             // just one day
             ? this.getFormatedDate(startDateTime)
             // range of days
             : this.makeRange(this.getFormatedDate(startDateTime),
                              this.getFormatedDate(endDateTime)))
          : (sameDay
             ? (sameTime
                // just one date time
                ? this.formatDateTime(startDateTime)
                // range of times on same day
                : this.formatDateTime(startDateTime, false, 
                                      this.makeRange(this.getFormatedTime(startDateTime),
                                                     this.getFormatedTime(endDateTime))))
             // range across different days
             : this.makeRange(this.formatDateTime(startDateTime),
                              this.formatDateTime(endDateTime))));
}

/** PRIVATE makeRange takes two strings and concatenates them with
    "--" in the middle if they have no spaces, or " -- " if they do. 

    Range dash should look different from hyphen used in dates like 1999-12-31.
    Western typeset text uses an &ndash;.  (Far eastern text uses no spaces.)
    Plain text convention is to use - for hyphen and minus, -- for ndash,
    and --- for mdash.  For now use -- so works with plain text email.
    Add spaces around it only if fromDateTime or toDateTime includes space,
    e.g., "1999-12-31--2000-01-01", "1999-12-31 23:55 -- 2000.01.01 00:05".
**/
DateFormater.prototype.makeRange = function makeRange(fromString, toString) {
  if (fromString.indexOf(" ") == -1 && toString.indexOf(" ") == -1)
    return fromString + "--" + toString;
  else
    return fromString + " -- "+ toString;
}

/** PUBLIC formatDateTime formats both date and time from datetime in pref order.
    If timeString is not null (such as "All Day"), it is used instead of time. 
    if omitDateIfToday is true, date is omitted if date is today.
    If date is not today, date is more relevant than time, so default is
    formatted DATE TIME so date is visible in columns too narrow to show both,
    Override by setting preference calendar.date.formatTimeBeforeDate=true.

   "DDD, DD MMM YYYY" "HH:mm"
    Tue, 31 Dec 1999 21:00
    Tue, 31 Dec 1999 All Day      (called with "All Day" timeString)
    Tue, 31 Dec 1999 21:00--22:00 (called by formatInterval with "21:00--22:00" timeString)
    21:00   (called with datetime 21:00 today, with omitDateIfToday true, and no timeString)
    All Day (called with datetime 0:00 today, with omitDateIfToday true, with timeString "All Day")
 **/
DateFormater.prototype.formatDateTime = function formatDateTime(datetime, omitDateIfToday, timeString) { 
  var formattedTime = (timeString || this.getFormatedTime( datetime ));
  if (omitDateIfToday) { 
    var today = new Date();
    if (datetime.getFullYear() == today.getFullYear() &&
        datetime.getMonth() == today.getMonth() &&
        datetime.getDate() == today.getDate()) {
      // if today, just show time (or "All day")
      return formattedTime;
    }
  }
  var formattedDate = this.getFormatedDate( datetime );
  if ( getBoolPref(gCalendarWindow.calendarPreferences.calendarPref, "date.formatTimeBeforeDate", false )) { 
    return formattedTime+" "+formattedDate;
  } else { // default
    return formattedDate+" "+formattedTime; 
  }
}

