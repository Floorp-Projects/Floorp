/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const HOURS_TO_MINUTES = 60;
const MINUTES_TO_SECONDS = 60;
const SECONDS_TO_MILLISECONDS = 1000;
const MINUTES_TO_MILLISECONDS = MINUTES_TO_SECONDS * SECONDS_TO_MILLISECONDS;
const HOURS_TO_MILLISECONDS = HOURS_TO_MINUTES * MINUTES_TO_MILLISECONDS;

this.EXPORTED_SYMBOLS = ["ISO8601DateUtils"];

debug("*** loading ISO8601DateUtils\n");

this.ISO8601DateUtils = {

  /**
  * XXX Thunderbird's W3C-DTF function
  *
  * Converts a W3C-DTF (subset of ISO 8601) date string to a Javascript
  * date object. W3C-DTF is described in this note:
  * http://www.w3.org/TR/NOTE-datetime IETF is obtained via the Date
  * object's toUTCString() method.  The object's toString() method is
  * insufficient because it spells out timezones on Win32
  * (f.e. "Pacific Standard Time" instead of "PST"), which Mail doesn't
  * grok.  For info, see
  * http://lxr.mozilla.org/mozilla/source/js/src/jsdate.c#1526.
  */
  parse: function ISO8601_parse(aDateString) {
    var dateString = aDateString;
    if (!dateString.match('-')) {
      // Workaround for server sending
      // dates such as: 20030530T11:18:50-08:00
      // instead of: 2003-05-30T11:18:50-08:00
      var year = dateString.slice(0, 4);
      var month = dateString.slice(4, 6);
      var rest = dateString.slice(6, dateString.length);
      dateString = year + "-" + month + "-" + rest;
    }

    var parts = dateString.match(/(\d{4})(-(\d{2,3}))?(-(\d{2}))?(T(\d{2}):(\d{2})(:(\d{2})(\.(\d+))?)?(Z|([+-])(\d{2}):(\d{2}))?)?/);

    // Here's an example of a W3C-DTF date string and what .match returns for it.
    //
    // date: 2003-05-30T11:18:50.345-08:00
    // date.match returns array values:
    //
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

    // Create a Date object from the date parts.  Note that the Date
    // object apparently can't deal with empty string parameters in lieu
    // of numbers, so optional values (like hours, minutes, seconds, and
    // milliseconds) must be forced to be numbers.
    var date = new Date(parts[1], parts[3] - 1, parts[5], parts[7] || 0,
      parts[8] || 0, parts[10] || 0, parts[12] || 0);

    // We now have a value that the Date object thinks is in the local
    // timezone but which actually represents the date/time in the
    // remote timezone (f.e. the value was "10:00 EST", and we have
    // converted it to "10:00 PST" instead of "07:00 PST").  We need to
    // correct that.  To do so, we're going to add the offset between
    // the remote timezone and UTC (to convert the value to UTC), then
    // add the offset between UTC and the local timezone //(to convert
    // the value to the local timezone).

    // Ironically, W3C-DTF gives us the offset between UTC and the
    // remote timezone rather than the other way around, while the
    // getTimezoneOffset() method of a Date object gives us the offset
    // between the local timezone and UTC rather than the other way
    // around.  Both of these are the additive inverse (i.e. -x for x)
    // of what we want, so we have to invert them to use them by
    // multipying by -1 (f.e. if "the offset between UTC and the remote
    // timezone" is -5 hours, then "the offset between the remote
    // timezone and UTC" is -5*-1 = 5 hours).

    // Note that if the timezone portion of the date/time string is
    // absent (which violates W3C-DTF, although ISO 8601 allows it), we
    // assume the value to be in UTC.

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

    return date;
  },

  create: function ISO8601_create(aDate) {
    function zeropad (s, l) {
      s = s.toString(); // force it to a string
      while (s.length < l) {
        s = '0' + s;
      }
      return s;
    }

    var myDate;
    // if d is a number, turn it into a date
    if (typeof aDate == 'number') {
      myDate = new Date()
      myDate.setTime(aDate);
    } else {
      myDate = aDate;
    }

    // YYYY-MM-DDThh:mm:ssZ
    var result = zeropad(myDate.getUTCFullYear (), 4) +
                 zeropad(myDate.getUTCMonth () + 1, 2) +
                 zeropad(myDate.getUTCDate (), 2) + 'T' +
                 zeropad(myDate.getUTCHours (), 2) + ':' +
                 zeropad(myDate.getUTCMinutes (), 2) + ':' +
                 zeropad(myDate.getUTCSeconds (), 2) + 'Z';

    return result;
  }
}
