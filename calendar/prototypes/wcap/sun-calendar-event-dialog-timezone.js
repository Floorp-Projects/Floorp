/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Buettner <michael.buettner@sun.com>
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

//////////////////////////////////////////////////////////////////////////////
// onLoad
//////////////////////////////////////////////////////////////////////////////

function onLoad() {

  var args = window.arguments[0];
  window.time = args.time;
  window.onAcceptCallback = args.onOk;

  var tzname = timezoneString(window.time.timezone);
  var menulist = document.getElementById("timezone-menulist");
  var index = findTimezone(tzname);
  if(index < 0) {
    var kDefaultTimezone = calendarDefaultTimezone();
    tzname = timezoneString(window.time.getInTimezone(kDefaultTimezone).timezone);
    index = findTimezone(tzname);
  }
  
  var menulist = document.getElementById("timezone-menulist");
  menulist.selectedIndex = index;
  
  updateTimezone();

  opener.setCursor("auto");
}

//////////////////////////////////////////////////////////////////////////////
// findTimezone
//////////////////////////////////////////////////////////////////////////////

function findTimezone(timezone)
{
  var menulist = document.getElementById("timezone-menulist");
  var numChilds = menulist.childNodes[0].childNodes.length;
  for(var i=0; i<numChilds; i++) {
    var menuitem = menulist.childNodes[0].childNodes[i];
    if(timezoneString(menuitem.getAttribute("value")) == timezone) {
      return i;
    }
  }
  return -1;
}

//////////////////////////////////////////////////////////////////////////////
// timezoneString
//////////////////////////////////////////////////////////////////////////////

function timezoneString(timezone)
{
  var fragments = timezone.split('/');
  var num = fragments.length;
  if(num <= 1)
    return fragments[0];
  return fragments[num-2]+'/'+fragments[num-1];
}

//////////////////////////////////////////////////////////////////////////////
// updateTimezone
//////////////////////////////////////////////////////////////////////////////

function updateTimezone() {

  var menulist = document.getElementById("timezone-menulist");
  var menuitem = menulist.selectedItem;
  var someTZ = menuitem.getAttribute("value");

  // convert the date/time to the currently selected timezone
  // and display the result in the appropriate control.
  // before feeding the date/time value into the control we need
  // to set the timezone to 'floating' in order to avoid the
  // automatic conversion back into the OS timezone.
  var datetime = document.getElementById("timezone-time");
  var time = window.time.getInTimezone(someTZ);
  time.timezone = "floating";
  datetime.value = time.jsDate;
  
  var icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                     .getService(Components.interfaces.calIICSService);
  
  var comp = icssrv.getTimezone(someTZ);
  var subComp = comp.getFirstSubcomponent("VTIMEZONE");
  var standard = subComp.getFirstSubcomponent("STANDARD");
  var standardTZOffset = standard.getFirstProperty("TZOFFSETTO").valueAsIcalString;
  
  var stack = document.getElementById("timezone-stack");
  var numChilds = stack.childNodes.length;
  for(var i=0; i<numChilds; i++) {
    var image = stack.childNodes[i];
    if(image.hasAttribute("tzid")) {
      var offset = image.getAttribute("tzid");
      if(offset == standardTZOffset) {
        image.removeAttribute("hidden");
      } else {
        image.setAttribute("hidden","true");
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// onAccept
//////////////////////////////////////////////////////////////////////////////

function onAccept()
{
  var menulist = document.getElementById("timezone-menulist");
  var menuitem = menulist.selectedItem;
  var timezone = menuitem.getAttribute("value");
  var datetime = window.time.getInTimezone(timezone);  
  window.onAcceptCallback(datetime);
  
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// onCancel
//////////////////////////////////////////////////////////////////////////////

function onCancel()
{
}

//////////////////////////////////////////////////////////////////////////////
// End of file
//////////////////////////////////////////////////////////////////////////////
