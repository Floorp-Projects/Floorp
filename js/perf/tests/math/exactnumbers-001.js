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
 * SUMMARY: Testing the |LN2| method for the Math object. The natural
 *          logarithm of 2, approximately 0.693.
 *          Testing the |LN10| method for the Math object. The natural
 *          logarithm of 10, approximately 2.302.
 *          Testing the |LOG2E| method for the Math object. The base 2 
 *          logarithm of E (approximately 1.442).
 *          Testing the |LOG10E| method for the Math object. The base 10
 *          logarithm of E (approximately 0.434).
 *          Testing the |PI| method for the Math object. The ratio of the
 *          circumference of a circle to its diameter, approximately 3.14159.
 *          Testing the |SQRT2| method for the Math object. The square root of
 *          2, approximately 1.414.
 */
//-----------------------------------------------------------------------------
var UBOUND=10000000;

test();

function test()
{
  var u_bound = UBOUND;
  var status;
  var start;
  var end;
  var i;
  
  status = "Math.LN2";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.LN2;
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.LN10";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.LN10;
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.LOG2E";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.LOG2E;
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.LOG10E";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.LOG10E;
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.PI\t";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.PI
  }
  end = new Date();
  reportTime(status, start, end);

  status = "Math.SQRT2";
  start = new Date();
  for(i=0; i<u_bound; i++)
  {
    Math.SQRT2;
  }
  end = new Date();
  reportTime(status, start, end);
}
