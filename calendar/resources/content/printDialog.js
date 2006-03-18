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
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Chris Charabaruk <ccharabaruk@meldstar.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Chris Allen
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Michiel van Leeuwen <mvl@exedo.nl>
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



/***** calendar/printDialog.js
* PRIMARY AUTHOR
*   Chris Allen
*   
* NOTES
*   Code for the calendar's print dialog.
*
* IMPLEMENTATION NOTES
**********
*/

var gPrintSettings = null;
var gCalendarWindow = window.opener.gCalendarWindow;

/*-----------------------------------------------------------------
 *   W I N D O W      F U N C T I O N S
 */

/*-----------------------------------------------------------------
 *   Called when the dialog is loaded.
 */

function loadCalendarPrintDialog()
{
    // set the date to the currently selected date
    document.getElementById("start-date-picker").value = 
        window.opener.document.getElementById("view-deck").selectedPanel.startDay.jsDate;
    document.getElementById("end-date-picker").value = 
        window.opener.document.getElementById("view-deck").selectedPanel.endDay.jsDate;

    // start focus on title
    var firstFocus = document.getElementById( "title-field" ).focus();

    // Get a list of formatters
    var contractids = new Array();
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    var catenum = catman.enumerateCategory('cal-print-formatters');

    // Walk the list, adding item to the layout menupopup
    var layoutList = document.getElementById("layout-field");
    while (catenum.hasMoreElements()) {
        var entry = catenum.getNext();
        entry = entry.QueryInterface(Components.interfaces.nsISupportsCString);
        var contractid = catman.getCategoryEntry('cal-print-formatters', entry);
        var formatter = Components.classes[contractid]
                                 .getService(Components.interfaces.calIPrintFormatter);
        // Use the contractid as value
        layoutList.appendItem(formatter.name, contractid);
    }
    layoutList.selectedIndex = 0;

    opener.setCursor( "auto" );

    self.focus();
}


function printCalendar() {
    var ccalendar = getDisplayComposite();
    var start;
    var end;
    var eventList;

    var listener = {
        mEventArray: new Array(),

        onOperationComplete: function (aCalendar, aStatus, aOperationType, aId, aDateTime) {
            printInitWindow(listener.mEventArray, start, end); 
        },

        onGetResult: function (aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
            for (var i = 0; i < aCount; i++) {
                listener.mEventArray.push(aItems[i]);
            }
        }
    };

    var filter = ccalendar.ITEM_FILTER_TYPE_EVENT | 
                 ccalendar.ITEM_FILTER_CLASS_OCCURRENCES;

    var start;
    var end;
    switch (document.getElementById("view-field").selectedItem.value) {
        case 'currentview':
        case '': //just in case
            start = window.opener.document.getElementById("view-deck").selectedPanel.startDay;
            end   = window.opener.document.getElementById("view-deck").selectedPanel.endDay;
            break;
        case 'selected' :
            eventList = gCalendarWindow.EventSelection.selectedEvents;
            break;
        case 'custom' :
            start = jsDateToDateTime(document.getElementById("start-date-picker").value);
            end   = jsDateToDateTime(document.getElementById("end-date-picker").value);
            break ;
        default :
            dump("Error : no case in printDialog.js::printCalendar()");
    }
    if (!eventList) {
        // end isn't exclusive, so we need to add one day
        end = end.clone();
        end.day = end.day + 1;
        end.normalize();
        ccalendar.getItems(filter, 0, start, end, listener);
    } else {
        printInitWindow(eventList, null, null);
    }
}

function printInitWindow(aEventList, aStart, aEnd) {
    var args = new Object();
    args.title = document.getElementById("title-field").value;
    args.layoutContractid = document.getElementById("layout-field").value;
    args.eventList = aEventList;
    args.start = aStart;
    args.end = aEnd;

    window.openDialog("chrome://calendar/content/calPrintEngine.xul",
                      "CalendarPrintWindow",
                      "chrome,dialog=no,all,centerscreen",
                      args);
}

/*-----------------------------------------------------------------
 *   Called when a datepicker is finished, and a date was picked.
 */

function onDatePick() {
    radioGroupSelectItem("view-field", "custom-range");
}

