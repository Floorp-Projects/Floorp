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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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
//-----------------------------------------------------------------------------
var SECTION = "15.9.5.7";
var VERSION = "ECMA_3";  
var TITLE   = "Date.prototype.toLocaleTimeString()"; 

var status = '';
var actual = '';  
var expect = '';
var givenDate;
var year = '';
var regexp = '';
var TimeString = '';
var reducedDateString = '';
var hopeThisIsLocaleTimeString = '';
var cnERR ='OOPS! FATAL ERROR: no regexp match in extractLocaleTimeString()';


startTest();
writeHeaderToLog( SECTION + " "+ TITLE);


//-----------------------------------------------------------------------------------------------------
   var testcases = getTestCases();
//-----------------------------------------------------------------------------------------------------
test();


function getTestCases() {
    var array = new Array();
    var item = 0;
    // first, a couple generic tests -

    status = "typeof (now.toLocaleTimeString())";  
    actual =   typeof (now.toLocaleTimeString());
    expect = "string";
    array[item++] = new TestCase(SECTION, status, expect, actual);
    

    status = "Date.prototype.toLocaleTimeString.length";   
    actual =  Date.prototype.toLocaleTimeString.length;
    expect =  0;   
    array[item++] = new TestCase(SECTION, status, expect, actual);


    // 1970
    addDateTestCase(0);
    addDateTestCase(TZ_ADJUST);


    // 1900
    addDateTestCase(TIME_1900);
    addDateTestCase(TIME_1900 - TZ_ADJUST);


    // 2000
    addDateTestCase(TIME_2000);
    addDateTestCase(TIME_2000 - TZ_ADJUST);


    // 29 Feb 2000
    addDateTestCase(UTC_29_FEB_2000);
    addDateTestCase(UTC_29_FEB_2000 - 1000);
    addDateTestCase(UTC_29_FEB_2000 - TZ_ADJUST);


    // Now
    addDateTestCase( TIME_NOW);
    addDateTestCase( TIME_NOW - TZ_ADJUST);


    // 2005
    addDateTestCase(UTC_1_JAN_2005);
    addDateTestCase(UTC_1_JAN_2005 - 1000);
    addDateTestCase(UTC_1_JAN_2005 - TZ_ADJUST);

    
    
    function addTestCase()
    {
      array[item++] = new TestCase( SECTION, status, expect, actual); 
    }


    function addDateTestCase(date_given_in_milliseconds)
    {
       givenDate = new Date(date_given_in_milliseconds);
       
       status = '().toLocaleTimeString()'; 
       
       
       actual = cnERR;
       expect = extractLocaleTimeString(givenDate);
       
       addTestCase();
    }
    return array;
}

/*
 * As of 2002-01-07, the format for JavaScript dates changed.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=118266 (SpiderMonkey)
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=118636 (Rhino)
 *
 * WAS: Mon Jan 07 13:40:34 GMT-0800 (Pacific Standard Time) 2002
 * NOW: Mon Jan 07 2002 13:40:34 GMT-0800 (Pacific Standard Time)
 *
 * So first, use a regexp of the form /date.toDateString()(.*)$/
 * to capture the TimeString into the first backreference.
 *
 * Then remove the GMT string from TimeString (see introduction above)
 */
function extractLocaleTimeString(date)
{
  regexp = new RegExp(date.toDateString() + '(.*)' + '$');
  try
  {
    TimeString = date.toString().match(regexp)[1];
  }
  catch(e)
  {
    return cnERR;
  }

  /*
   * Now remove the GMT part of the TimeString.
   * Guard against dates with two "GMT"s, like:
   * Jan 01 00:00:00 GMT+0000 (GMT Standard Time)
   */
  regexp= /([^G]*)GMT.*/;
  try
  {
    hopeThisIsLocaleTimeString = TimeString.match(regexp)[1];
  }
  catch(e)
  {
    return TimeString;
  }

  // trim any leading or trailing spaces -
  return trimL(trimR(hopeThisIsLocaleTimeString));
}


function trimL(s)
{
  if (!s) {return cnEmptyString;};
  for (var i = 0; i!=s.length; i++) {if (s[i] != ' ') {break;}}
  return s.substring(i);
}


function trimR(s)
{
  for (var i = (s.length - 1); i!=-1; i--) {if (s[i] != ' ') {break;}}
  return s.substring(0, i+1);  
}
