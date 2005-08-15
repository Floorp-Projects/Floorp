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
 * The Original Code is Feedview for Firefox.
 *
 * The Initial Developer of the Original Code is
 * Tom Germeau <tom.germeau@epigoon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@mozilla.org>
 *   Myk Melez <myk@mozilla.org>
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

var FeedView = {
  /**
   * Converts a W3C-DTF (subset of ISO 8601) date string to an IETF date string.
   * @param   dateString
   *          A string that may contain a W3C-DTF date string
   * @returns an IETF date string
   */
  _W3CToIETFDate: function(dateString) {
    // W3C-DTF is described in this note: http://www.w3.org/TR/NOTE-datetime
    // IETF is obtained via the Date object's toUTCString() method.  The object's
    // toString() method is insufficient because it spells out timezones on Win32
    // (f.e. "Pacific Standard Time" instead of "PST"), which Mail doesn't grok.
    // For info, see http://lxr.mozilla.org/mozilla/source/js/src/jsdate.c#1654.
  
    const HOURS_TO_MINUTES = 60;
    const MINUTES_TO_SECONDS = 60;
    const SECONDS_TO_MILLISECONDS = 1000;
    const MINUTES_TO_MILLISECONDS = MINUTES_TO_SECONDS * SECONDS_TO_MILLISECONDS;
    const HOURS_TO_MILLISECONDS = HOURS_TO_MINUTES * MINUTES_TO_MILLISECONDS;
  
    var parts = dateString.match(/(\d\d\d\d)(-(\d\d))?(-(\d\d))?(T(\d\d):(\d\d)(:(\d\d)(\.(\d+))?)?(Z|([+-])(\d\d):(\d\d))?)?/);
  
    // Here's an example of a W3C-DTF date string and what .match returns for it.
    //  date: 2003-05-30T11:18:50.345-08:00
    //  date.match returns array values:
    //   0: 2003-05-30T11:18:50-08:00,
    //   1: 2003,
    //   2: -05,
    //   3: 05,
    //   4: -30,
    //   5: 30,
    //   6: T11:18:50-08:00,
    //   7: 11,
    //   8: 18,
    //   9: :50,
    //   10: 50,
    //   11: .345,
    //   12: 345,
    //   13: -08:00,
    //   14: -,
    //   15: 08,
    //   16: 00
  
    // Create a Date object from the date parts.  Note that the Date object
    // apparently can't deal with empty string parameters in lieu of numbers,
    // so optional values (like hours, minutes, seconds, and milliseconds)
    // must be forced to be numbers.
    var date = new Date(parts[1], parts[3]-1, parts[5], parts[7] || 0,
                        parts[8] || 0, parts[10] || 0, parts[12] || 0);
  
    // We now have a value that the Date object thinks is in the local timezone
    // but which actually represents the date/time in the remote timezone
    // (f.e. the value was "10:00 EST", and we have converted it to "10:00 PST"
    // instead of "07:00 PST").  We need to correct that.  To do so, we're going
    // to add the offset between the remote timezone and UTC (to convert the value
    // to UTC), then add the offset between UTC and the local timezone (to convert
    // the value to the local timezone).
  
    // Ironically, W3C-DTF gives us the offset between UTC and the remote timezone
    // rather than the other way around, while the getTimezoneOffset() method
    // of a Date object gives us the offset between the local timezone and UTC
    // rather than the other way around.  Both of these are the additive inverse
    // (i.e. -x for x) of what we want, so we have to invert them to use them
    // by multipying by -1
    // (f.e. if "the offset between UTC and the remote timezone" is -5 hours,
    // then "the offset between the remote timezone and UTC" is -5*-1 = 5 hours).
  
    // Note that if the timezone portion of the date/time string is absent
    // (which violates W3C-DTF, although ISO 8601 allows it), we assume the value
    // to be in UTC.
  
    // The offset between the remote timezone and UTC in milliseconds.
    var remoteToUTCOffset = 0;
    if (parts[13] && parts[13] != "Z") {
      var direction = (parts[14] == "+" ? 1 : -1);
      if (parts[15])
        remoteToUTCOffset += direction * parts[15] * HOURS_TO_MILLISECONDS;
      if (parts[16])
        remoteToUTCOffset += direction * parts[16] * MINUTES_TO_MILLISECONDS;
    }
    remoteToUTCOffset = remoteToUTCOffset * -1; // invert it
  
    // The offset between UTC and the local timezone in milliseconds.
    var UTCToLocalOffset = date.getTimezoneOffset() * MINUTES_TO_MILLISECONDS;
    UTCToLocalOffset = UTCToLocalOffset * -1; // invert it
  
    date.setTime(date.getTime() + remoteToUTCOffset + UTCToLocalOffset);
  
    return date.toUTCString();
  },
  
  /**
   * Normalizes date strings embedded in the feed into a common format. 
   */
  _initializeDates: function() {
    // Normalize date formatting for all feed entries
    var divs = document.getElementsByTagName("div");
    for (var i = 0; i < divs.length; i++) {
      var d = divs[i].getAttribute("date");
      dump("*** D = " + d + "\n");
      if (d) {
        // If the date looks like it's in W3C-DTF format, convert it into
        // an IETF standard date string.
        if (d.search(/^\d\d\d\d/) != -1)
          d = this._W3CToIETFDate(d);

        d = new Date(d);

        // If the date could be parsed...
        if (d instanceof Date) {
          // XXX It would be nicer to say day = "Today" or "Yesterday".
          var day = d.toGMTString();
          day = day.substring(0, 11);
          
          function padZeros(num) {
            return num < 10 ? "0" + num : num;
          }
          divs[i].getElementsByTagName("span")[0].textContent =
            day + " @ " + padZeros(d.getHours()) +  ":" + padZeros(d.getMinutes());
        }
      }
    }
  },
  
  /**
   * Returns the value of a boolean preference
   * @param   name
   *          The name of the preference
   * @returns The value of the preference
   */
  _getBooleanPref: function(name) {
    return document.getElementById("data").getAttribute(name) == "true";
  },
  
  /**
   * Returns the value of an integer preference
   * @param   name
   *          The name of the preference
   * @returns The value of the preference
   */
  _getIntegerPref: function(name) {
    return parseInt(document.getElementById("data").getAttribute(name));
  },
  
  /**
   * Reload the page after an interval
   */
  reload: function() {
    location.reload();
  },
  
  /**
   * Initialize the feed view
   */
  init: function() {
    // Hide the menu if the user chose to have it closed
    if (!this._getBooleanPref("showmenu")) 
      document.getElementById("menubox").style.display = "none";

    // Normalize the date formats
    this._initializeDates();

    // Set up the auto-reload timer.
    // The timer loads every browser.feedview.reloadInterval minutes.
    // It is disabled if the value of that preference is 0.
    var reloadInterval = this._getIntegerPref("reloadinterval") || 0;
    if (reloadInterval > 0)
      setTimeout("FeedView.reload()", reloadInterval * 60 * 1000);
  }
};
