/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

var colors = new Array(12);
colors[0] = "#FF0000";
colors[1] = "#00FF00";
colors[2] = "#0000FF";
colors[3] = "#FF00FF";
colors[4] = "#000000";
colors[5] = "#00FFFF";
colors[6] = "#FFFFFF";
colors[7] = "#808000";
colors[8] = "#B00080";
colors[9] = "#00B080";
colors[10] = "#B0B080";
colors[11] = "#FFBB44";

var index = 0;

function changeColor()
{
  while (index < 11)
  {
	  zulucommand("panel","timebarscale","setbackgroundcolor",colors[index]);
	  zulucommand("panel","timebarscale","getbackgroundcolor");
	  zulucommand("panel","timebarscale","setforegroundcolor",colors[index+1]);
	  zulucommand("panel","timebarscale","getforegroundcolor");
	  zulucommand("panel","timebarscale","setbordercolor",colors[index+2]);
	  zulucommand("panel","timebarscale","getbordercolor");

	  zulucommand("panel","prevday","setforegroundcolor",colors[index+1]);
	  zulucommand("panel","prevday","getforegroundcolor");
	  zulucommand("panel","nextday","setforegroundcolor",colors[index+1]);
	  zulucommand("panel","nextday","getforegroundcolor");
	  zulucommand("panel","prevhour","setforegroundcolor",colors[index+1]);
	  zulucommand("panel","prevhour","getforegroundcolor");
	  zulucommand("panel","nexthour","setforegroundcolor",colors[index+1]);
	  zulucommand("panel","nexthour","getforegroundcolor");

	  index++;
	  if (index >= 10) {
		index = 0;
	  }
  }
}

changeColor();
