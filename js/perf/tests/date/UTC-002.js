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
 * SUMMARY: Using the |UTC()| method for the |Date()| object. 
 *          Converts a date to a string, using the universal time convention.
 *          UTC takes comma-delimited date parameters and returns the number of
 *          milliseconds between January 1, 1970, 00:00:00, universal time and 
 *          the time you specified. 
 *
 *          Syntax: |Date.UTC(year, month, day[, hrs, min, sec, ms])|
 *          year - A year after 1900.  
 *          month - An integer between 0 and 11 representing the month.  
 *          date - An integer between 1 and 31 representing a day of the month.  
 *          hrs - An integer between 0 and 23 representing the hours.  
 *          min - An integer between 0 and 59 representing the minutes.  
 *          sec - An integer between 0 and 59 representing the seconds.  
 *          ms - An integer between 0 and 999 representing the milliseconds.    
 */
//-----------------------------------------------------------------------------
var UBOUND=1000000;

test();

function test()
{
  var u_bound = UBOUND;
  var status;
  var start;
  var end;
  var i;
  var year = 2002;
  var month = 1;
  var day = 1;
  var hrs = 0;
  var min = 0;
  var sec = 0;
  var ms = 0;
  
  status = "Date.UTC(year, month, day[, hrs, min, sec, ms])";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Date.UTC(year, month, day, hrs, min, sec, ms);
  }
  end = new Date();
  reportTime(status, start, end);


  // Date before January 1, 1970
  status = "Date.UTC(1900, 4, 25[, 11, 56, 12, 500])";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Date.UTC(1900, 4, 25, 11, 56, 12, 500);
  }
  end = new Date();
  reportTime(status, start, end);

  // Date before January 1, 1970
  status = "Date.UTC(1900, 4, 25[, 11, 56, 12, 500])";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Date.UTC(1900, 4, 25, 11, 56, 12, 500);
  }
  end = new Date();
  reportTime(status, start, end);

  // Future date
  status = "Date.UTC(2100, 4, 25[, 11, 56, 12, 500])";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Date.UTC(2100, 4, 25, 11, 56, 12, 500);
  }
  end = new Date();
  reportTime(status, start, end);
}
