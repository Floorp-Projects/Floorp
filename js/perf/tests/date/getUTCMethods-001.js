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
 * SUMMARY: Testing various |getUTC()| methods for the |Date()| object. 
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
  
  dt = new Date();

  status = "dt.getUTCDate()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCDate();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getUTCDay()\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCDay();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getUTCFullYear()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCFullYear();
  }
  end = new Date();
  reportTime(status, start, end);
    
  status = "dt.getUTCHours()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCHours();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getUTCMilliseconds()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCMilliseconds();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getUTCMinutes()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCMinutes();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getUTCMonth()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCMonth();
  }
  end = new Date();
  reportTime(status, start, end);

  status = "dt.getUTCSeconds()";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    dt.getUTCSeconds();
  }
  end = new Date();
  reportTime(status, start, end);
}
