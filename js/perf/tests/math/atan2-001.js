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
 * Date:    12 October 2002
 * SUMMARY: Testing the |atan2()| method for the Math object. The method 
 * returns a numeric value between -pi and pi representing the angle theta of
 * an (x,y) point. This is the counterclockwise angle, measured in radians, 
 * between the positive X axis, and the point (x,y). Note that the arguments 
 * to this function pass the y-coordinate first and the x-coordinate second. 
 * |atan2()| is passed separate x and y arguments, and |atan2()| is passed the
 * ratio of those two  arguments. Because |atan2()| is a static method of Math,
 * you always use it as |Math.atan2()|, rather than as a method of a Math 
 * object you created. 
 *
 * Syntax:  |atan2(x)|  where x is a number.
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

  status = "Math.atan2(0,0)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.atan2(0,0);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.atan2(0,180)"
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.atan2(0,180);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.atan2(120,0)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.atan2(120,0);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.atan2(90,90)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.atan2(90,90);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.atan2(-120,0)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.atan2(-120,0);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.atan2(0,-180)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.atan2(0,-180);
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.atan2(-120,-120)";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.atan2(-120,-120);
  }
  end = new Date();
  reportTime(status, start, end);
}
