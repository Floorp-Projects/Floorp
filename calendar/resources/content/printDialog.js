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
 *                 Matthew Willis <mattwillis@gmail.com>
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

var gTempFile = null;

var gPrintSettings = null;
var gCalendarWindow = window.opener.gCalendarWindow;

function loadCalendarPrintDialog()
{
    // set the datepickers to the currently selected dates
    var theView = window.opener.currentView();
    document.getElementById("start-date-picker").value = theView.startDay.jsDate;
    document.getElementById("end-date-picker").value = theView.endDay.jsDate;

    // Get a list of formatters
    var contractids = new Array();
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    var catenum = catman.enumerateCategory('cal-print-formatters');

    // Walk the list, adding items to the layout menupopup
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

    opener.setCursor("auto");

    refreshHtml();

    self.focus();
}

function unloadCalendarPrintDialog()
{
    gTempFile.remove(false);
}

/**
 * Gets the settings from the dialog's UI widgets.
 * @returns an Object with title, layoutCId, eventList, start, and end
 *          properties containing the appropriate values.
 */
function getDialogSettings()
{
    var settings = new Object();
    var ccalendar = window.opener.getCompositeCalendar();

    var listener = {
        mEventArray: new Array(),

        onOperationComplete: function (aCalendar, aStatus, aOperationType, aId, aDateTime) {
            settings.eventList = listener.mEventArray;
            settings.start = start;
            settings.end = end;
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
    var eventList;
    switch (document.getElementById("view-field").selectedItem.value) {
        case 'currentview':
        case '': //just in case
            var theView = window.opener.currentView();
            start = theView.startDay;
            end   = theView.endDay;
            break;
        case 'selected':
            eventList = gCalendarWindow.EventSelection.selectedEvents;
            break;
        case 'custom':
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
        settings.eventList = eventList;
        settings.start = null;
        settings.end = null;
    }

    var tempTitle = document.getElementById("title-field").value;
    settings.title = (tempTitle || calGetString("calendar", "Untitled"));
    settings.layoutCId = document.getElementById("layout-field").value;
    return settings;
}

/**
 * Looks at the selections the user has made (start date, layout, etc.), and
 * updates the HTML in the iframe accordingly. This is also called when a
 * dialog UI element has changed, since we'll want to refresh the preview.
 */
function refreshHtml()
{
    var settings = getDialogSettings();

    // I'd like to use calGetString, but it can't do "formatStringFromName".
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);

    var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
    document.title = props.formatStringFromName("PrintPreviewWindowTitle",
                                                [settings.title], 1);

    var printformatter = Components.classes[settings.layoutCId]
                                   .createInstance(Components.interfaces.calIPrintFormatter);

    // Fail-safe check to not init twice, to prevent leaking files
    if (gTempFile) {
        gTempFile.remove(false);
    }
    const nsIFile = Components.interfaces.nsIFile;
    var dirService = Components.classes["@mozilla.org/file/directory_service;1"]
                               .getService(Components.interfaces.nsIProperties);
    gTempFile = dirService.get("TmpD", nsIFile);
    gTempFile.append("calendarPrint.html");
    gTempFile.createUnique(nsIFile.NORMAL_FILE_TYPE, 0600); // 0600 = -rw-------
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    var tempUri = ioService.newFileURI(gTempFile); 

    var stream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                           .createInstance(Components.interfaces.nsIFileOutputStream);

    try {
        // 0x2A = PR_TRUNCATE, PR_CREATE_FILE, PR_WRONLY
        // 0600 = -rw-------
        stream.init(gTempFile, 0x2A, 0600, 0);
        printformatter.formatToHtml(stream, settings.start, settings.end,
                                    settings.eventList.length,
                                    settings.eventList, settings.title);
        stream.close();
    } catch (e) {
        dump("printDialog::refreshHtml:"+e+"\n");
    }

    document.getElementById("content").contentWindow.location = gTempUri.spec;
}

/**
 * Called when once a date has been selected in the datepicker.
 */
function onDatePick() {
    radioGroupSelectItem("view-field", "custom-range");
    refreshHtml();
}
