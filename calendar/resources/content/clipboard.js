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
 * The Original Code is Mozilla Calendar code.
 *
 * The Initial Developer of the Original Code is
 * ArentJan Banck <ajbanck@planet.nl>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): ArentJan Banck <ajbanck@planet.nl>
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

/***** calendarClipboard
*
* NOTES 
*   TODO items
*     - Add a clipboard listener, to enable/disable menu-items depending if 
*       valid clipboard data is available.
*
******/


function getClipboard()
{
    const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
    const kClipboardIID = Components.interfaces.nsIClipboard;
    return Components.classes[kClipboardContractID].getService(kClipboardIID);
}

var Transferable = Components.Constructor("@mozilla.org/widget/transferable;1", Components.interfaces.nsITransferable);
var SupportsArray = Components.Constructor("@mozilla.org/supports-array;1", Components.interfaces.nsISupportsArray);
var SupportsCString = (("nsISupportsCString" in Components.interfaces)
                       ? Components.Constructor("@mozilla.org/supports-cstring;1", Components.interfaces.nsISupportsCString)
                       : Components.Constructor("@mozilla.org/supports-string;1", Components.interfaces.nsISupportsString)
                      );
var SupportsString = (("nsISupportsWString" in Components.interfaces)
                      ? Components.Constructor("@mozilla.org/supports-wstring;1", Components.interfaces.nsISupportsWString)
                      : Components.Constructor("@mozilla.org/supports-string;1", Components.interfaces.nsISupportsString)
                     );

/** 
* Test if the clipboard has items that can be pasted into Calendar.
* This must be of type "text/calendar" or "text/unicode"
*/

function canPaste()
{
    const kClipboardIID = Components.interfaces.nsIClipboard;

    var clipboard = getClipboard();
    var flavourArray = new SupportsArray;
    var flavours = ["text/calendar", "text/unicode"];
   
    for (var i = 0; i < flavours.length; ++i)
    {
        var kSuppString = new SupportsCString;
        kSuppString.data = flavours[i];
        flavourArray.AppendElement(kSuppString);
    }
   
    return clipboard.hasDataMatchingFlavors(flavourArray, kClipboardIID.kGlobalClipboard);
}

/** 
* Copy iCalendar data to the Clipboard, and delete the selected events.
* Does not use eventarray parameter, because DeletCcommand delete selected events.
*/

function cutToClipboard( /* calendarEventArray */)
{
    var calendarEventArray = currentView().getSelectedItems({});

    if( copyToClipboard( calendarEventArray ) )
    {
         deleteEventCommand( true ); // deletes all selected events without prompting.
    }
}


/** 
* Copy iCalendar data to the Clipboard. The data is copied to both 
* text/calendar and text/unicode. 
**/

function copyToClipboard( calendarItemArray )
{  
    if (!calendarItemArray) {
        calendarItemArray = currentView().getSelectedItems({});
    }

    if (!calendarItemArray.length) {
        dump("Tried to cut/copy 0 events");
        return false;
    }

    var icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);
    var calComp = icssrv.createIcalComponent("VCALENDAR");
    calComp.version = "2.0";
    calComp.prodid = "-//Mozilla.org/NONSGML Mozilla Calendar V1.1//EN";

    for each (item in calendarItemArray) {
        // If we copy an item and paste it again, it will have the same ID as
        // the original.  Therefore, give every item a new ID.
        var dummyItem = Components.classes["@mozilla.org/calendar/event;1"]
                                  .createInstance(Components.interfaces.calIEvent);
        var newItem = item.clone();
        newItem.id = dummyItem.id;
        calComp.addSubcomponent(newItem.icalComponent);
    }

    // XXX This might not be enough to be Outlook compatible
    var sTextiCalendar = calComp.serializeToICS();

    // 1. get the clipboard service
    var clipboard = getClipboard();

    // 2. create the transferable
    var trans = new Transferable;

    if ( trans && clipboard) {
        // 3. register the data flavors
        trans.addDataFlavor("text/calendar");
        trans.addDataFlavor("text/unicode");

        // 4. create the data objects
        var icalWrapper = new SupportsString;

        // get the data
        icalWrapper.data = sTextiCalendar;

        // 5. add data objects to transferable
        // Both Outlook 2000 client and Lotus Organizer use text/unicode 
        // when pasting iCalendar data
        trans.setTransferData("text/calendar", icalWrapper,
                              icalWrapper.data.length*2 ); // double byte data
        trans.setTransferData("text/unicode", icalWrapper, 
                              icalWrapper.data.length*2 );

        clipboard.setData(trans, null,
                          Components.interfaces.nsIClipboard.kGlobalClipboard );

        return true;         
    }
    return true;
}


/** 
* Paste iCalendar events from the clipboard, 
* or paste clipboard text into description of new event
*/

function pasteFromClipboard()
{
    if (!canPaste()) {
        dump("Attempting to paste with no useful data on the clipboard");
        return;
    }

    // 1. get the clipboard service
    var clipboard = getClipboard();

    // 2. create the transferable
    var trans = new Transferable;

    if (!trans || !clipboard) {
        dump("Failed to get either a transferable or a clipboard");
        return;
    }
    // 3. register the data flavors you want, highest fidelity first!
    trans.addDataFlavor("text/calendar");
    trans.addDataFlavor("text/unicode");

    // 4. get transferable from clipboard
    clipboard.getData ( trans, Components.interfaces.nsIClipboard.kGlobalClipboard);

    // 5. ask transferable for the best flavor. Need to create new JS
    //    objects for the out params.
    var flavour = { };
    var data = { };
    trans.getAnyTransferData(flavour, data, {});
    data = data.value.QueryInterface(Components.interfaces.nsISupportsString).data;
    var items = new Array();
    switch (flavour.value) {
        case "text/calendar":
        case "text/unicode":
            var icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                                   .getService(Components.interfaces.calIICSService);
            var calComp = icssrv.parseICS(data);
            var subComp = calComp.getFirstSubcomponent("ANY");
            while (subComp) {
                switch (subComp.componentType) {
                    case "VEVENT":
                        var event = Components.classes["@mozilla.org/calendar/event;1"]
                                              .createInstance
                                              (Components.interfaces.calIEvent);
                        event.icalComponent = subComp;
                        items.push(event);
                        break;
                    case "VTODO":
                        var todo = Components.classes["@mozilla.org/calendar/todo;1"]
                                             .createInstance
                                             (Components.interfaces.calITodo);
                        todo.icalComponent = subComp;
                        items.push(todo);
                        break;
                    default: break;
                }
                subComp = calComp.getNextSubcomponent("ANY");
            }
            // If there are multiple items on the clipboard, the earliest
            // should be set to the selected day/time and the rest adjusted.
            var earliestDate = null;
            for each(item in items) {
                var date = null;
                if (item.startDate) 
                    date = item.startDate.clone();
                else if (item.entryDate)
                    date = item.entryDate.clone();
                else if (item.dueDate)
                    date = item.dueDate.clone();

                if (!date)
                    continue;
                if (!earliestDate || date.compare(earliestDate) < 0)
                    earliestDate = date;
            }
            var destCal = ("ltnSelectedCalendar" in window) ?
              ltnSelectedCalendar() : getDefaultCalendar();
            var firstDate = currentView().selectedDay;
            if (!firstDate.isMutable) {
                firstDate = firstDate.clone();
            }
            firstDate.isDate = false;

            function makeNewStartingDate(oldDate) {
                var date = firstDate.clone();
                var offset = oldDate.subtractDate(earliestDate);
                date.addDuration(offset);

                // now that the day is set, fix the time back to the original
                date.hour = oldDate.hour;
                date.minute = oldDate.minute;
                date.second = oldDate.second;
                date.timezone = oldDate.timezone;
                date.normalize();
                if (oldDate.isDate) {
                    date.isDate = true;
                }
                return date;
            }

            startBatchTransaction();
            for each(item in items) {
                var duration = item.duration;
                var newItem = item.clone();
                if (item.startDate) {
                    newItem.startDate = makeNewStartingDate(item.startDate).clone();
                    newItem.endDate = newItem.startDate.clone();
                    newItem.endDate.addDuration(duration);
                }
                if (item.entryDate) {
                    newItem.entryDate = makeNewStartingDate(item.entryDate).clone();
                    if (item.dueDate) {
                        newItem.dueDate = newItem.entryDate.clone();
                        newItem.dueDate.addDuration(duration);
                    }
                }
                else if (item.dueDate) {
                    newItem.dueDate = makeNewStartingDate(item.dueDate).clone();
                }
                doTransaction('add', newItem, destCal, null, null);
            }
            endBatchTransaction();
            break;
        default: 
            dump("Unknown clipboard type: " + flavour.value);
    }
}
