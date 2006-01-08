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
 *  OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Garth Smedley <garths@oeone.com>
 *  Mike Potter <mikep@oeone.com>
 *  Eric Belhaire <belhaire@ief.u-psud.fr>
 *  Michiel van Leeuwen <mvl@exedo.nl>
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

const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;

function calDateTimeFormatter() {
    var strBundleService = 
        Components.classes["@mozilla.org/intl/stringbundle;1"]
                  .getService(Components.interfaces.nsIStringBundleService);
    this.mDateStringBundle =  strBundleService.createBundle("chrome://calendar/locale/dateFormat.properties");

    this.mDateService = 
        Components.classes["@mozilla.org/intl/scriptabledateformat;1"]
                  .getService(nsIScriptableDateFormat);

    // If LONG FORMATTED DATE is same as short formatted date,
    // then OS has poor extended/long date config, so use workaround.
    this.mUseLongDateService = true;
    var probeDate = 
        Components.classes["@mozilla.org/calendar/datetime;1"]
                  .createInstance(Components.interfaces.calIDateTime);
    probeDate.jsDate = new Date(2002,3,4);
    try { 
        var longProbeString = this.formatDateLong(probeDate);
        // On Unix extended/long date format may be created using %Ex instead
        // of %x. Some systems may not support it and return "Ex" or same as
        // short string. In that case, don't use long date service, use a
        // workaround hack instead.
        if (longProbeString == null ||
            longProbeString.length < 4 ||
            longProbeString == probeString)
           this.mUseLongDateService = false;
    } catch (e) {
        this.mUseLongDateService = false;
    }

}

calDateTimeFormatter.prototype.QueryInterface =
function QueryInterface(aIID) {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.calIDateTimeFormatter)) {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
};

calDateTimeFormatter.prototype.formatDate =
function formatDate(aDate) {
    // Format the date using user's format preference (long or short)
    var format;
    var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefBranch);
    try {     
        format = prefBranch.getIntPref("calendar.date.format");
    } catch(e) {
        format = 0;
    }

    if (format == 0)
        return this.formatDateLong(aDate);
    else
        return this.formatDateShort(aDate);
};

calDateTimeFormatter.prototype.formatDateShort =
function formatDateShort(aDate) {
    return this.mDateService.FormatDate("",
                                        nsIScriptableDateFormat.dateFormatShort,
                                        aDate.year,
                                        aDate.month + 1,
                                        aDate.day);
};

calDateTimeFormatter.prototype.formatDateLong =
function formatDateLong(aDate) {
    if (this.mUseLongDateService) {
        return this.mDateService.FormatDate("",
                                            nsIScriptableDateFormat.dateFormatLong,
                                            aDate.year,
                                            aDate.month + 1,
                                            aDate.day);
    } else {
        // HACK We are probably on Linux and want a string in long format.
        // dateService.dateFormatLong on Linux may return a short string, so
        // build our own.
        return this.shortDayName(aDate.weekday) + " " +
               aDate.day + " " +
               this.shortMonthName(aDate.month) + " " +
               aDate.year;
    }
};

calDateTimeFormatter.prototype.formatDateWithoutYear =
function formatDateWithoutYear(aDate) {
    // XXX Using a hardcoded format, because nsIScriptableDateFormat doesn't
    //     have a way to leave out the year.
    return this.shortMonthName(aDate.month) + " " + aDate.day;
};


calDateTimeFormatter.prototype.formatTime =
function formatTime(aDate) {
    return this.mDateService.FormatTime("",
                                        nsIScriptableDateFormat.timeFormatNoSeconds,
                                        aDate.hour,
                                        aDate.minute,
                                        0);
};

calDateTimeFormatter.prototype.formatDateTime =
function formatDateTime(aDate) {
    var formattedDate = this.formatDate(aDate);
    var formattedTime = this.formatTime(aDate);

    var timeBeforeDate;
    var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefBranch);
    try {     
        timeBeforeDate = prefBranch.getBoolPref("calendar.date.formatTimeBeforeDate");
    } catch(e) {
        timeBeforeDate = 0;
    }

    if (timeBeforeDate)
        return formattedTime+" "+formattedDate;
    else
        return formattedDate+" "+formattedTime;
};

calDateTimeFormatter.prototype.formatInterval =
function formatInterval(aStartDate, aEndDate, aStartString, aEndString) {
    endDate = aEndDate.clone();
    // EndDate is exclusive. For all-day events, we ened to substract one day,
    // to get into a format that's understandable.
    if (aStartDate.isDate) {
        endDate.day -= 1;
        endDate.normalize();
    }

    testdate = aStartDate.clone();
    testdate.isDate = true;
    testdate.normalize();
    var sameDay = (testdate.compare(endDate) == 0);
    
    if (aStartDate.isDate) {
        // All-day interval, so we should leave out the time part
        if (sameDay) {
            // Start and end on the same day: only give return the start
            // date.
            // "5 Jan 2006"
            aStartString.value = this.formatDate(aStartDate);
            aEndString.value = "";
        } else {
            // Spanning multiple days, return both dates
            // "5 Jan 2006 - 7 Jan 2006"
            aStartString.value = this.formatDate(aStartDate);
            aEndString.value = this.formatDate(endDate);
        }
    } else {
        // non-allday, so need to return date and time
        if (sameDay) {
            // End is on the same day as start, so we can leave out the
            // end date (but still include end time)
            // "5 Jan 2006 13:00 - 17:00"
            aStartString.value = this.formatDate(aStartDate)+" "+this.formatTime(aStartDate);
            aEndString.value = this.formatTime(endDate);
        } else {
            // Spanning multiple days, so need to include date and time
            // for start and end
            // "5 Jan 2006 13:00 - 7 Jan 2006 9:00"
            aStartString.value = this.formatDateTime(aStartDate);
            aEndString.value = this.formatDateTime(endDate);
        }
    }
    return 1;
};

calDateTimeFormatter.prototype.monthName =
function monthName(aMonthIndex) {
    var oneBasedMonthIndex = aMonthIndex + 1;
    return this.mDateStringBundle.GetStringFromName("month." + oneBasedMonthIndex + ".name" );
};

calDateTimeFormatter.prototype.shortMonthName =
function shortMonthName(aMonthIndex) {
    var oneBasedMonthIndex = aMonthIndex + 1;
    return this.mDateStringBundle.GetStringFromName("month." + oneBasedMonthIndex + ".Mmm" );
};

calDateTimeFormatter.prototype.dayName =
function monthName(aDayIndex) {
    var oneBasedDayIndex = aDayIndex + 1;
    return this.mDateStringBundle.GetStringFromName("day." + oneBasedDayIndex + ".name" );
};

calDateTimeFormatter.prototype.shortDayName =
function shortMonthName(aDayIndex) {
    var oneBasedDayIndex = aDayIndex + 1;
    return this.mDateStringBundle.GetStringFromName("day." + oneBasedDayIndex + ".Mmm" );
};
