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
 * SUMMARY: Testing various |get()| methods for the |Date()| object. Providing
 * a specific date.
 */
//-----------------------------------------------------------------------------
var UBOUND=100000;

test();

function test()
{
  var u_bound = UBOUND;
  var status;
  var start;
  var end;
  var i;
  var dt;
  
  dt = new Date(1980, 4, 25, 11, 56, 25, 0);
  
  status = "dt.getDate()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getDate();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getDay()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getDay();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getFullYear()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getFullYear();
  }
  end = new Date();
  reportTime(status, start, end);
    
  status = "dt.getHours()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getHours();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getMilliseconds()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getMilliseconds();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getMinutes()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getMinutes();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getMonth()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getMonth();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getSeconds()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getSeconds();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getTime()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getTime();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getTimezoneOffset()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getTimezoneOffset();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getYear()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getYear();
  }
  end = new Date();
  reportTime(status, start, end); 
}
