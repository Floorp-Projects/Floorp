/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Colin Phillips (colinp@oeone.com), Garth Smedley (garths@oeone.com)
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
 * 
*/


function penroot()
{
	//setup the arrays to keep track of the current open applications
	this.OpenWindows = new Array();

   // global calendar
   this.gCalendarEventDataSource = new CalendarEventDataSource( null, "/home/mikep" );
   
}

penroot.prototype.getAppNum = function (AppName)
{
	for (NumApps = 0; NumApps < this.OpenWindows.length; NumApps++)
	{
		if ((this.OpenWindows[NumApps])&&(this.OpenWindows[NumApps].AppName == AppName))
		{
			return NumApps;
		}
	}
	return null;
}

penroot.prototype.getRootWindowAppPath = function (AppName)
{
	for (AppCount = 0; AppCount < this.OpenWindows.length; AppCount++)
   {
		if ((this.OpenWindows[AppCount])&&(this.OpenWindows[AppCount].AppName == AppName))
		{
			return (this.OpenWindows[AppCount].iframeReference);
		}
   }
	return null;
}

function penApp()
{
	this.reference = null;
	this.iframeReference = null;
	this.left = null;
	this.width = null;
	this.top = null;
	this.height = null;
	this.floating = false;
	this.id = false;
	this.src = false;
	this.AppName = null;
	this.Arguments = null;
}

