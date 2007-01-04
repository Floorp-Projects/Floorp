/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Magnus Melin <mkmelin+mozilla@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var gSearchDateFormat = 0;
var gSearchDateSeparator;
var gSearchDateLeadingZeros;

/**
 * Get the short date format option of the current locale.
 * This supports the common case which the date separator is
 * either '/', '-', '.' and using Christian year.
 */
function initLocaleShortDateFormat()
{
  // default to mm/dd/yyyy
  gSearchDateFormat = 3;
  gSearchDateSeparator = "/";
  gSearchDateLeadingZeros = true;

  try {
    var dateFormatService = Components.classes["@mozilla.org/intl/scriptabledateformat;1"]
                                    .getService(Components.interfaces.nsIScriptableDateFormat);
    var dateString = dateFormatService.FormatDate("", 
                                                  dateFormatService.dateFormatShort, 
                                                  1999, 
                                                  12, 
                                                  1);

    // find out the separator
    var possibleSeparators = "/-.";
    var arrayOfStrings;
    for ( var i = 0; i < possibleSeparators.length; ++i )
    {
      arrayOfStrings = dateString.split( possibleSeparators[i] );
      if ( arrayOfStrings.length == 3 )
      {
        gSearchDateSeparator = possibleSeparators[i];
        break;
      }
    }

    // check the format option
    if ( arrayOfStrings.length != 3 )       // no successfull split
    {
      dump("getLocaleShortDateFormat: could not analyze the date format, defaulting to mm/dd/yyyy\n");
    }
    else
    {
      // the date will contain a zero if that system settings include leading zeros
      gSearchDateLeadingZeros = /0/.test(dateString);

      // match 1 as number, since that will match both "1" and "01"
      if ( arrayOfStrings[0] == 1 )
      {
        // 01.12.1999 or 01.1999.12
        gSearchDateFormat = arrayOfStrings[1] == "12" ? 5 : 6;
      }
      else if ( arrayOfStrings[1] == 1 )
      {
        // 12.01.1999 or 1999.01.12
        gSearchDateFormat = arrayOfStrings[0] == "12" ? 3 : 2;
      }
      else  // implies arrayOfStrings[2] == 1
      {
        // 12.1999.01 or 1999.12.01
        gSearchDateFormat = arrayOfStrings[0] == "12" ? 4 : 1;
      }
    }
  }
  catch (e)
  {
    dump("getLocaleShortDateFormat: caught an exception!\n");
  }
}

function initializeSearchDateFormat()
{
  if (gSearchDateFormat)
    return;

  // get a search date format option and a seprator
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefBranch);
    gSearchDateFormat =
               pref.getComplexValue("mailnews.search_date_format", 
                                    Components.interfaces.nsIPrefLocalizedString);
    gSearchDateFormat = parseInt(gSearchDateFormat);

    // if the option is 0 then try to use the format of the current locale
    if (gSearchDateFormat == 0)
      initLocaleShortDateFormat();
    else
    {
      // initialize the search date format based on preferences
      if ( gSearchDateFormat < 1 || gSearchDateFormat > 6 )
        gSearchDateFormat = 3;

      gSearchDateSeparator = pref.getComplexValue("mailnews.search_date_separator", 
                                  Components.interfaces.nsIPrefLocalizedString);

      gSearchDateLeadingZeros = (pref.getComplexValue("mailnews.search_date_leading_zeros", 
                      Components.interfaces.nsIPrefLocalizedString).data == "true");
    }
  }
  catch (e)
  {
    dump("initializeSearchDateFormat: caught an exception: "+e+"\n");
    // set to mm/dd/yyyy in case of error
    gSearchDateFormat = 3;
    gSearchDateSeparator = "/";
    gSearchDateLeadingZeros = true;
  }
}

function convertPRTimeToString(tm)
{
  var time = new Date();
  // PRTime is in microseconds, JavaScript time is in milliseconds
  // so divide by 1000 when converting
  time.setTime(tm / 1000);
  
  return convertDateToString(time);
}

function convertDateToString(time)
{
  initializeSearchDateFormat();

  var year = time.getFullYear();
  var month = time.getMonth() + 1;  // since js month is 0-11
  if ( gSearchDateLeadingZeros && month < 10 )
    month = "0" + month;
  var date = time.getDate();
  if ( gSearchDateLeadingZeros && date < 10 )
    date = "0" + date;

  var dateStr;
  var sep = gSearchDateSeparator;

  switch (gSearchDateFormat)
  {
    case 1:
      dateStr = year + sep + month + sep + date;
      break;
    case 2:
      dateStr = year + sep + date + sep + month;
      break;
    case 3:
     dateStr = month + sep + date + sep + year;
      break;
    case 4:
      dateStr = month + sep + year + sep + date;
      break;
    case 5:
      dateStr = date + sep + month + sep + year;
      break;
    case 6:
      dateStr = date + sep + year + sep + month;
      break;
    default:
      dump("valid search date format option is 1-6\n");
  }

  return dateStr;
}

function convertStringToPRTime(str)
{
  initializeSearchDateFormat();

  var arrayOfStrings = str.split(gSearchDateSeparator);
  var year, month, date;

  // set year, month, date based on the format option
  switch (gSearchDateFormat)
  {
    case 1:
      year = arrayOfStrings[0];
      month = arrayOfStrings[1];
      date = arrayOfStrings[2];
      break;
    case 2:
      year = arrayOfStrings[0];
      month = arrayOfStrings[2];
      date = arrayOfStrings[1];
      break;
    case 3:
      year = arrayOfStrings[2];
      month = arrayOfStrings[0];
      date = arrayOfStrings[1];
      break;
    case 4:
      year = arrayOfStrings[1];
      month = arrayOfStrings[0];
      date = arrayOfStrings[2];
      break;
    case 5:
      year = arrayOfStrings[2];
      month = arrayOfStrings[1];
      date = arrayOfStrings[0];
      break;
    case 6:
      year = arrayOfStrings[1];
      month = arrayOfStrings[2];
      date = arrayOfStrings[0];
      break;
    default:
      dump("valid search date format option is 1-6\n");
  }

  month -= 1; // since js month is 0-11

  var time = new Date(year, month, date);

  // JavaScript time is in milliseconds, PRTime is in microseconds
  // so multiply by 1000 when converting
  return (time.getTime() * 1000);
}

