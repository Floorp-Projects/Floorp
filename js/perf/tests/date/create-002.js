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
 * SUMMARY: Creation of date objects using local variables. Testing miliseconds.
 *
 * Syntax: |Date()|
 *
 * Parameters: 
 * milliseconds - Integer value representing the number of milliseconds since
 * 1 January 1970 00:00:00.  
 * dateString - String value representing a date. The string should be in a 
 * format recognized by the Date.parse method.  
 * yr_num, mo_num, day_num - Integer values representing part of a date. As an
 * integer value, the month is represented by 0 to 11 with 0=January.  
 * hr_num, min_num, sec_num, ms_num - Integer values representing part of a
 * date.  
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
  var dt;
  var ms;
  
  status = "new Date(ms) - ms = 10\t";
  ms = 10;
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt = new Date(ms);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "new Date(ms) - ms = 1000";
  ms = 1000;
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt = new Date(ms);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "new Date(ms) - ms = 1000000";
  ms = 1000000;
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt = new Date(ms);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "new Date(10)\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt = new Date(10);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "new Date(1000)\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt = new Date(1000);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "new Date(1000000)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt = new Date(1000000);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "new Date(10000000)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt = new Date(10000000);
  }
  end = new Date();
  reportTime(status, start, end);
}
