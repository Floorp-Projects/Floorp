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
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
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

function addAlarm(event)
{
  var alarmList = document.getElementById("alarmlist");

  var alarmWidget = document.createElement("calendar-alarm-widget");
  alarmWidget.setAttribute("title", event.title);
  var time = event.startDate || event.entryDate || event.dueDate;
  alarmWidget.setAttribute("time", time.toString());
  alarmWidget.setAttribute("location", event.getProperty("LOCATION"));
  alarmWidget.addEventListener("snooze", onSnoozeAlarm, false);
  alarmWidget.addEventListener("dismiss", onDismissAlarm, false);
  alarmWidget.item = event;

  alarmList.appendChild(alarmWidget);

   var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
   var calendarPrefs = prefService.getBranch("calendar.");

   var playSound = calendarPrefs.getBoolPref("alarms.playsound");
   if (playSound) {
     try {
       var soundURL = makeURL(calendarPrefs.getCharPref("alarms.soundURL"));
       var sound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);
       sound.init();
       sound.play(soundURL);
     } catch (ex) {
       dump("unable to play sound...\n" + ex + "\n");
     }
   }
}

function onDismissAll()
{
  var now = Components.classes["@mozilla.org/calendar/datetime;1"]
                      .createInstance(Components.interfaces.calIDateTime);
  now.jsDate = new Date();
  now = now.getInTimezone("UTC");
  var box = document.getElementById("alarmlist");
  for each (kid in box.childNodes) {
    // We want the parent item, otherwise we're going to accidentally create an
    // exception.  We've relnoted (for 0.1) the slightly odd behavior this can
    // cause if you move an event after dismissing an alarm
    item = kid.item.parentItem.clone();
    item.alarmLastAck = now;
    item.calendar.modifyItem(item, kid.item, null);
  }
  return true;
}

function onSnoozeAlarm(event)
{
  // i hate xbl..
  var alarmWidget = event.target;

  var alarmService = Components.classes["@mozilla.org/calendar/alarm-service;1"].getService(Components.interfaces.calIAlarmService);
  
  var duration = Components.classes["@mozilla.org/calendar/duration;1"]
                 .createInstance(Components.interfaces.calIDuration);
  //XXX figure out a nice UI way to offer other length options
  duration.minutes = 5;

  alarmService.snoozeEvent(alarmWidget.item, duration);

  var parent = alarmWidget.parentNode;
  parent.removeChild(alarmWidget);
  if (!parent.hasChildNodes()) {
    // If this was the last alarm, close the window.
    window.close();
  }
}

function onDismissAlarm(event)
{
  var alarmWidget = event.target;
  var now = Components.classes["@mozilla.org/calendar/datetime;1"]
                      .createInstance(Components.interfaces.calIDateTime);
  now.jsDate = new Date();
  now = now.getInTimezone("UTC");
  var item = alarmWidget.item.clone();
  item.alarmLastAck = now;
  item.calendar.modifyItem(item, alarmWidget.item, null);

  var parent = alarmWidget.parentNode;
  parent.removeChild(alarmWidget);

  if (!parent.hasChildNodes()) {
    // If this was the last alarm, close the window.
    window.close();
  }

}

