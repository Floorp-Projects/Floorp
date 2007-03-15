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
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Mike Potter <mikep@oeone.com>
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Robin Edrenius <robin.edrenius@gmail.com>
 *                 Joey Minta <jminta@gmail.com>
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

/*-----------------------------------------------------------------
*  C A L E N D A R     C L A S S E S
*/

/*-----------------------------------------------------------------
*   CalendarWindow Class
*
*  Maintains the three calendar views and the selection.
*
* PROPERTIES
*     selectedDate           - selected date, instance of Date 
*  
*/


/**
*   CalendarWindow Constructor.
*
* NOTES
*   There is one instance of CalendarWindow
*   WARNING: As much as we all love CalendarWindow, it's going to die soon.
*/

function CalendarWindow( )
{
   // Extension authors can tweak this array to make gCalendarWindow.switchToView 
   // play nicely with any additional views
   this.availableViews = ["day", "week", "multiweek", "month"];

   /** This object only exists to keep too many things from breaking during the
    *   switch to the new views
   **/
   this.currentView = {
       changeNumberOfWeeks: function(menuitem) {
           var mwView = document.getElementById("view-deck").selectedPanel;
           mwView.weeksInView = menuitem.value;
       }
   };

   // Get the last view that was shown before shutdown, and switch to it
   var SelectedIndex = document.getElementById("view-deck").selectedIndex;
   
   switch( SelectedIndex )
   {
      case "1":
         this.switchToView('week');
         break;
      case "2":
         this.switchToView('multiweek');
         break;
      case "3":
         this.switchToView('month');
         break;
      case "0":
      default:
         this.switchToView('day');
         break;
   }
}

/** PUBLIC
*
*   Display today in the current view
*/

CalendarWindow.prototype.goToToday = function calWin_goToToday( )
{
   document.getElementById("lefthandcalendar").value = new Date();
   document.getElementById("view-deck").selectedPanel.goToDay(now());
}
/** PUBLIC
*
*   Choose a date, then go to that date in the current view.
*/

CalendarWindow.prototype.pickAndGoToDate = function calWin_pickAndGoToDate( )
{
  var args = new Object();
  args.initialDate = currentView().selectedDay.getInTimezone("floating").jsDate;
  args.onOk = function receiveAndGoToDate( pickedDate ) {
    currentView().goToDay( jsDateToDateTime(pickedDate) );
    document.getElementById( "lefthandcalendar" ).value = pickedDate;
  };
  openDialog("chrome://calendar/content/goToDateDialog.xul",
             "GoToDateDialog", // target= window name
             "chrome,modal", args);
}

/** PUBLIC
*
*   Go to today in the current view
*/

CalendarWindow.prototype.goToDay = function calWin_goToDay( newDate )
{
    var view = document.getElementById("view-deck").selectedPanel;
    var cdt = Components.classes["@mozilla.org/calendar/datetime;1"]
                        .createInstance(Components.interfaces.calIDateTime);
    cdt.year = newDate.getFullYear();
    cdt.month = newDate.getMonth();
    cdt.day = newDate.getDate();
    cdt.isDate = true;
    cdt.timezone = view.timezone;
    view.goToDay(cdt);
}

/** PRIVATE
*
*   Helper function to switch to a view
*
* PARAMETERS
*   newView  - MUST be one of the three CalendarView instances created in the constructor
*/

CalendarWindow.prototype.switchToView = function calWin_switchToView( newView )
{
    // Set up the commands
    for each (var view in this.availableViews) {
        var command = document.getElementById(view+"_view_command");
        if (view == newView) {
            command.setAttribute("checked", true);
        } else {
            command.removeAttribute("checked");
        }
    }

    var mwWeeksCommand = document.getElementById("menu-numberofweeks-inview")
    if (newView == "multiweek") {
        mwWeeksCommand.removeAttribute("disabled");
    } else {
        mwWeeksCommand.setAttribute("disabled", true);
    }

    // Call the common view switching code in calendar-views.js
    switchToView(newView);

    var labelAttribute = "label-"+newView+"-view";
    var prevCommand = document.getElementById("calendar-go-menu-previous");
    prevCommand.setAttribute("label", prevCommand.getAttribute(labelAttribute));
    var nextCommand = document.getElementById("calendar-go-menu-next");
    nextCommand.setAttribute("label", nextCommand.getAttribute(labelAttribute));
}
