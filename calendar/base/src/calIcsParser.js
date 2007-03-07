/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Michiel van Leeuwen <mvl@exedo.nl>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function calIcsParser() {
    this.wrappedJSObject = this;
    this.mItems = new Array();
    this.mComponents = new Array();
    this.mProperties = new Array();
}

calIcsParser.prototype.QueryInterface =
function QueryInterface(aIID) {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.calIIcsParser)) {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
};

calIcsParser.prototype.parseString =
function ip_parseString(aICSString) {
    icsSvc = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);

    var rootComp = icsSvc.parseICS(aICSString);
    var calComp;
    // libical returns the vcalendar component if there is just one vcalendar.
    // If there are multiple vcalendars, it returns an xroot component, with
    // those vcalendar children. We need to handle both.
    if (rootComp.componentType == 'VCALENDAR') {
        calComp = rootComp;
    } else {
        calComp = rootComp.getFirstSubcomponent('VCALENDAR');
    }

    var unexpandedItems = [];
    var uid2parent = {};
    var excItems = [];

    while (calComp) {

        // Get unknown properties
        var prop = calComp.getFirstProperty("ANY");
        while (prop) {
            if (prop.propertyName != "VERSION" &&
                prop.propertyName != "PRODID") {
                this.mProperties.push(prop);
            }
            prop = calComp.getNextProperty("ANY");
        }

        var prodId = calComp.getFirstProperty("PRODID");
        var isFromOldSunbird;
        if (prodId) {
            isFromOldSunbird = prodId.value == "-//Mozilla.org/NONSGML Mozilla Calendar V1.0//EN";
        }

        var subComp = calComp.getFirstSubcomponent("ANY");
        while (subComp) {
            var item = null;
            switch (subComp.componentType) {
            case "VEVENT":
                var event = Components.classes["@mozilla.org/calendar/event;1"]
                                      .createInstance(Components.interfaces.calIEvent);
                item = event;
                break;
            case "VTODO":
                var todo = Components.classes["@mozilla.org/calendar/todo;1"]
                                     .createInstance(Components.interfaces.calITodo);
                item = todo;
                break;
            case "VTIMEZONE":
                // this should already be attached to the relevant
                // events in the calendar, so there's no need to
                // do anything with it here.
                break;
            default:
                this.mComponents.push(subComp);
            }

            if (item) {
                item.icalComponent = subComp;

                // Only try to fix ICS from Sunbird 0.2 (and earlier) if it
                // has an EXDATE.
                hasExdate = subComp.getFirstProperty("EXDATE");
                if (isFromOldSunbird && hasExdate) {
                    item = fixOldSunbirdExceptions(item);
                }

                var rid = item.recurrenceId;
                if (!rid) {
                    unexpandedItems.push(item);
                    if (item.recurrenceInfo) {
                        uid2parent[item.id] = item;
                    }
                } else {
                    // force no recurrence info:
                    item.recurrenceInfo = null;
                    excItems.push(item);
                }
            }
            subComp = calComp.getNextSubcomponent("ANY");
        }
        calComp = rootComp.getNextSubcomponent("VCALENDAR");
    }

    // tag "exceptions", i.e. items with rid:
    for each (var item in excItems) {
        var parent = uid2parent[item.id];
        if (parent) {
            item.parentItem = parent;
            parent.recurrenceInfo.modifyException(item);
        }
    }
    
    for each (var item in unexpandedItems) {
        this.mItems.push(item);
    }
};

calIcsParser.prototype.parseFromStream =
function ip_parseFromStream(aStream) {
    // Read in the string. Note that it isn't a real string at this point, 
    // because likely, the file is utf8. The multibyte chars show up as multiple
    // 'chars' in this string. So call it an array of octets for now.
    
    var octetArray = [];
    var binaryIS = Components.classes["@mozilla.org/binaryinputstream;1"]
                             .createInstance(Components.interfaces.nsIBinaryInputStream);
    binaryIS.setInputStream(aStream);
    octetArray = binaryIS.readByteArray(binaryIS.available());
    
   
    // Some other apps (most notably, sunbird 0.2) happily splits an UTF8
    // character between the octets, and adds a newline and space between them,
    // for ICS folding. Unfold manually before parsing the file as utf8.This is
    // UTF8 safe, because octets with the first bit 0 are always one-octet
    // characters. So the space or the newline never can be part of a multi-byte
    // char.
    for (var i = octetArray.length - 2; i >= 0; i--) {
        if (octetArray[i] == "\n" && octetArray[i+1] == " ") {
            octetArray = octetArray.splice(i, 2);
        }
    }

    // Interpret the byte-array as a UTF8-string, and convert into a
    // javascript string.
    var unicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
                                     .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
    // ICS files are always UTF8
    unicodeConverter.charset = "UTF-8";
    var str = unicodeConverter.convertFromByteArray(octetArray, octetArray.length);
    return this.parseString(str);
}

calIcsParser.prototype.getItems =
function ip_getItems(aCount) {
    aCount.value = this.mItems.length;
    return this.mItems.concat([]); //clone
}

calIcsParser.prototype.getProperties =
function ip_getProperties(aCount) {
    aCount.value = this.mProperties.length;
    return this.mProperties.concat([]); //clone
}

calIcsParser.prototype.getComponents =
function ip_getComponents(aCount) {
    aCount.value = this.mComponents.length;
    return this.mComponents.concat([]); //clone
}

// Helper function to deal with the busted exdates from Sunbird 0.2
// When Sunbird 0.2 (and earlier) creates EXDATEs, they are set to
// 00:00:00 floating rather than to the item's DTSTART. This fixes that.
// (bug 354073)
function fixOldSunbirdExceptions(aItem) {
    const kCalIRecurrenceDate = Components.interfaces.calIRecurrenceDate;

    var item = aItem;
    var ritems = aItem.recurrenceInfo.getRecurrenceItems({});
    for each (var ritem in ritems) {
        // EXDATEs are represented as calIRecurrenceDates, which are
        // negative and finite.
        if (ritem instanceof kCalIRecurrenceDate &&
            ritem.isNegative &&
            ritem.isFinite) {
            // Only mess with the exception if its time is wrong.
            var oldDate = aItem.startDate || aItem.entryDate;
            if (ritem.date.compare(oldDate) != 0) {
                var newRitem = ritem.clone();
                // All we want from aItem is the time and timezone.
                newRitem.date.timezone = oldDate.timezone;
                newRitem.date.hour     = oldDate.hour;
                newRitem.date.minute   = oldDate.minute;
                newRitem.date.second   = oldDate.second;
                newRitem.date.normalize();
                item.recurrenceInfo.appendRecurrenceItem(newRitem);
                item.recurrenceInfo.deleteRecurrenceItem(ritem);
            }
        }
    }
    return item;
}
