/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Frank Schönheit <frank.schoenheit@gmx.de>
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Frank Schönheit <frank.schoenheit@gmx.de>
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

const MILLISECONDS_PER_HOUR   = 60 * 60 * 1000;
const MICROSECONDS_PER_DAY    = 1000 * MILLISECONDS_PER_HOUR * 24;

function onLoad()
{
  var upperDateBox = document.getElementById("upperDate");
  // focus the upper bound control - this is where we expect most users to enter
  // a date
  upperDateBox.focus();

  // and give it an initial date - "yesterday"
  var initialDate = new Date();
  initialDate.setHours( 0 );
  initialDate.setTime( initialDate.getTime() - MILLISECONDS_PER_HOUR );
    // note that this is sufficient - though it is at the end of the previous day,
    // we convert it to a date string, and then the time part is truncated
  upperDateBox.value = convertDateToString( initialDate );
  upperDateBox.select();  // allows to start overwriting immediately
}

function onAccept()
{
  // get the times as entered by the user
  var lowerDateString = document.getElementById( "lowerDate" ).value;
  // the fallback for the lower bound, if not entered, is the "beginning of
  // time" (1970-01-01), which actually is simply 0 :)
  var prLower = lowerDateString ? convertStringToPRTime( lowerDateString ) : 0;

  var upperDateString = document.getElementById( "upperDate" ).value;
  var prUpper;
  if ( upperDateString == "" )
  {
    // for the upper bound, the fallback is "today".
    var dateThisMorning = new Date();
    dateThisMorning.setMilliseconds( 0 );
    dateThisMorning.setSeconds( 0 );
    dateThisMorning.setMinutes( 0 );
    dateThisMorning.setHours( 0 );
    // Javascript time is in milliseconds, PRTime is in microseconds
    prUpper = dateThisMorning.getTime() * 1000;
  }
  else
    prUpper = convertStringToPRTime( upperDateString );

  // for the upper date, we have to do a correction:
  // if the user enters a date, then she means (hopefully) that all messages sent
  // at this day should be marked, too, but the PRTime calculated from this would
  // point to the beginning of the day. So we need to increment it by
  // [number of micro seconds per day]. This will denote the first microsecond of
  // the next day then, which is later used as exclusive boundary
  prUpper += MICROSECONDS_PER_DAY;

  markInDatabase( prLower, prUpper );

  return true;  // allow closing
}

/** marks all headers in the database, whose time is between the two
  given times, as read.
  @param lower
    PRTime for the lower bound - this boundary is inclusive
  @param upper
    PRTime for the upper bound - this boundary is exclusive
*/
function markInDatabase( lower, upper )
{
  var messageFolder;
  var messageDatabase;
  // extract the database
  if ( window.arguments && window.arguments[0] )
  {
    messageFolder = window.arguments[0];
    messageDatabase = messageFolder.getMsgDatabase( null );
  }

  if ( !messageDatabase )
  {
    dump( "markByDate::markInDatabase: there /is/ no database to operate on!\n" );
    return;
  }

  // the headers which are going to be marked
  var headers = Components.classes["@mozilla.org/supports-array;1"].createInstance( Components.interfaces.nsISupportsArray );

  // create an enumerator for all messages in the database
  var enumerator = messageDatabase.EnumerateMessages();
  if ( enumerator )
  {
    while ( enumerator.hasMoreElements() )
    {
      var header = enumerator.getNext();
      if ( ( header instanceof Components.interfaces.nsIMsgDBHdr )
         && !header.isRead ) // don't do anything until really necessary
      {
        var messageDate = header.date;

        if ( ( lower <= messageDate ) && ( messageDate < upper ) )
          headers.AppendElement( header );
      }
      else
        dump("markByDate::markInDatabase: unexpected: the database gave us a header which is no nsIMsgDBHdr!\n" );
    }
  }

  if ( headers.Count() )
    messageFolder.markMessagesRead( headers, true );
}
