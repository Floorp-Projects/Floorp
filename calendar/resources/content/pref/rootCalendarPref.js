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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mike Potter <mikep@oeone.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
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

function calendarPrefObserver( CalendarPreferences )
{
   this.CalendarPreferences = CalendarPreferences;
   try {
     var pbi = rootPrefNode.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
     pbi.addObserver("calendar", this, false);
  } catch(ex) {
    dump("Calendar: Failed to observe prefs: " + ex + "\n");
  }
}

calendarPrefObserver.prototype =
{
    domain: "calendar.",
    observe: function(subject, topic, prefName)
    {
        // when calendar pref was changed, we reinitialize 
        switch( prefName )
        {
            case "calendar.event.defaultstarthour":
            case "calendar.event.defaultendhour":
            case "calendar.weeks.inview":
            case "calendar.previousweeks.inview":
            case "calendar.week.d0sundaysoff":
            case "calendar.week.d1mondaysoff":
            case "calendar.week.d2tuesdaysoff":
            case "calendar.week.d2wednesdaysoff":
            case "calendar.week.d2thursdaysoff":
            case "calendar.week.d2fridaysoff":
            case "calendar.week.d2saturdaysoff":
                if (this.CalendarPreferences.calendarWindow.currentView != null)
                this.CalendarPreferences.calendarWindow.currentView.refresh();
                break;

            case "calendar.week.start":
                this.CalendarPreferences.calendarWindow.currentView.refresh();
                this.CalendarPreferences.calendarWindow.miniMonth.refreshDisplay(true);
                break;

            case "calendar.date.format" :
                this.CalendarPreferences.calendarWindow.currentView.refresh();
                refreshEventTree( getAndSetEventTable() );
                toDoUnifinderRefresh();
                break;

            case "calendar.alarms.showmissed":
                if( subject.getBoolPref( prefName ) ) {
                    //this triggers the alarmmanager if show missed is turned on
                    gICalLib.batchMode = true; 
                    gICalLib.batchMode = false;
                }
                break;

            default :
                break;
        }
     
        //this causes Mozilla to freeze
        //firePendingEvents(); 
    }
}

function getDefaultCategories()
{  
   try {
      var categoriesStringBundle = srGetStrBundle("chrome://calendar/locale/categories.properties");
      return categoriesStringBundle.GetStringFromName("categories" );
   }
   catch(e) { return "" }
}
   
   
function calendarPreferences( CalendarWindow )
{
   window.calendarPrefObserver = new calendarPrefObserver( this );
   
   this.calendarWindow = CalendarWindow;

   this.calendarPref = prefService.getBranch("calendar."); // preferences calendar node

   var calendarStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
   
   //go through all the preferences and set default values?
   getBoolPref( this.calendarPref, "servers.reloadonlaunch", calendarStringBundle.GetStringFromName("reloadServersOnLaunch" ) );
   getBoolPref( this.calendarPref, "alarms.show", calendarStringBundle.GetStringFromName("showAlarms" ) );
   getBoolPref( this.calendarPref, "alarms.showmissed", calendarStringBundle.GetStringFromName("showMissed" ) );
   getBoolPref( this.calendarPref, "alarms.playsound", calendarStringBundle.GetStringFromName("playAlarmSound" ) );
   GetUnicharPref( this.calendarPref, "categories.names", getDefaultCategories() );
   getCharPref( this.calendarPref, "timezone.default", calendarStringBundle.GetStringFromName("defaultzone"));
   getIntPref( this.calendarPref, "event.defaultlength", calendarStringBundle.GetStringFromName("defaultEventLength" ) );
   getIntPref( this.calendarPref, "alarms.defaultsnoozelength", calendarStringBundle.GetStringFromName("defaultSnoozeAlarmLength" ) );
   getIntPref( this.calendarPref, "date.format", calendarStringBundle.GetStringFromName("dateFormat" ) );
   getBoolPref( this.calendarPref, "dateformat.storeingmt", calendarStringBundle.GetStringFromName("storeInGmt") );
   getIntPref( this.calendarPref, "event.defaultstarthour", calendarStringBundle.GetStringFromName("defaultStartHour" ) );
   getIntPref( this.calendarPref, "event.defaultendhour", calendarStringBundle.GetStringFromName("defaultEndHour" ) );
   getIntPref( this.calendarPref, "week.start", calendarStringBundle.GetStringFromName("defaultWeekStart" ) );
   getBoolPref( this.calendarPref, "week.d0sundaysoff", "true"==calendarStringBundle.GetStringFromName("defaultWeekSundaysOff" ) );
   getBoolPref( this.calendarPref, "week.d1mondaysoff", "true"==calendarStringBundle.GetStringFromName("defaultWeekMondaysOff" ) );
   getBoolPref( this.calendarPref, "week.d2tuesdaysoff", "true"==calendarStringBundle.GetStringFromName("defaultWeekTuesdaysOff" ) );
   getBoolPref( this.calendarPref, "week.d3wednesdaysoff", "true"==calendarStringBundle.GetStringFromName("defaultWeekWednesdaysOff" ) );
   getBoolPref( this.calendarPref, "week.d4thursdaysoff", "true"==calendarStringBundle.GetStringFromName("defaultWeekThursdaysOff" ) );
   getBoolPref( this.calendarPref, "week.d5fridaysoff", "true"==calendarStringBundle.GetStringFromName("defaultWeekFridaysOff" ) );
   getBoolPref( this.calendarPref, "week.d6saturdaysoff", "true"==calendarStringBundle.GetStringFromName("defaultWeekSaturdaysOff" ) );
   getIntPref( this.calendarPref, "weeks.inview", calendarStringBundle.GetStringFromName("defaultWeeksInView" ) );
   getIntPref( this.calendarPref, "previousweeks.inview", calendarStringBundle.GetStringFromName("defaultPreviousWeeksInView" ) );
   getIntPref( this.calendarPref, "alarms.onforevents", 0 );
   getIntPref( this.calendarPref, "alarms.onfortodos", 0 );
   getCharPref( this.calendarPref, "alarms.eventalarmunit", calendarStringBundle.GetStringFromName("defaulteventalarmunit"));
   getCharPref( this.calendarPref, "alarms.todoalarmunit", calendarStringBundle.GetStringFromName("defaulttodoalarmunit"));
}

