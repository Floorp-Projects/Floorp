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
 * Michiel van Leeuwen <mvl@exedo.nl>.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

// Import

function calIcsImporter() {
    this.wrappedJSObject = this;
}

calIcsImporter.prototype.QueryInterface =
function QueryInterface(aIID) {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.calIImporter)) {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
};

calIcsImporter.prototype.getFileTypes =
function getFileTypes(aCount) {
    aCount.value = 1;
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
    return([{defaultExtension:'ics', 
             extensionFilter:'*.ics', 
             description: props.GetStringFromName('icsDesc')}]);
};

calIcsImporter.prototype.importFromStream =
function ics_importFromStream(aStream, aCount) {
    var items = new Array();

    // Read in the string. Note that it isn't a real string at this point, because
    // likely, the file is utf8. The multibyte chars show up as multiple 'chars'
    // in this string. So call it an array of octets for now.    
    
    var octetArray = [];
    var binaryInputStream = Components.classes["@mozilla.org/binaryinputstream;1"]
                                          .createInstance(Components.interfaces.nsIBinaryInputStream);
    binaryInputStream.setInputStream(aStream);
    octetArray = binaryInputStream.readByteArray(binaryInputStream.available());
    
   
    // Some other apps (most notably, sunbird 0.2) happily splits an utf8 character
    // between the octets, and adds a newline and space between them, for ics
    // folding. Unfold manually before parsing the file as utf8.
    // This is utf8 safe, because octets with the first bit 0 are always one-octet
    // characters. So the space or the newline never can be part of a multi-byte
    // char.
    for (var i=octetArray.length-2; i>=0; i--) {
        if (octetArray[i] == "\n" && octetArray[i+1] == " ") {
            octetArray = octetArray.splice(i, 2);
        }
    }

    // Interpret the byte-array as an utf8 string, and convert into a
    // javascript string.
    var unicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
                                     .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
    // ics files are always utf8
    unicodeConverter.charset = "UTF-8";
    var str = unicodeConverter.convertFromByteArray(octetArray, octetArray.length);


    icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);

    var rootComp = icssrv.parseICS(str);
    var calComp;
    // libical returns the vcalendar component if there is just
    // one vcalendar. If there are multiple vcalendars, it returns
    // an xroot component, with those vcalendar childs. We need to
    // handle both.
    if (rootComp.componentType == 'VCALENDAR') {
        calComp = rootComp;
    } else {
        calComp = rootComp.getFirstSubcomponent('VCALENDAR');
    }

    while (calComp) {
        // XXX bug 354073:
        // When Sunbird 0.2 (and earlier) creates EXDATEs, they are set to
        // 00:00:00 floating rather than to the item's DTSTART. This fixes that.
        //
        // This should really be in the migration code found in bug 349586,
        // but in the interest of getting 0.3 released, we're putting it here.
        // When bug 349586 lands, please make sure moving this to the
        // migrator is part of that checkin.
        var prodId = calComp.getFirstProperty("PRODID");
        var isFromOldSunbird;
        if (prodId) {
            isFromOldSunbird = (prodId.value.indexOf("NONSGML Mozilla Calendar V1.0") > -1);
            LOG("Old Sunbird file found...");
        }

        // Helper function to deal with the busted exdates from Sunbird 0.2
        function fixOldSunbirdExceptions(aItem) {
            const kCalIRecurrenceDate = Components.interfaces.calIRecurrenceDate;

            var ritems = aItem.recurrenceInfo.getRecurrenceItems({});
            for (var i in ritems) {
                var ritem = ritems[i];

                // EXDATEs are represented as calIRecurrenceDates, which are
                // negative and finite.
                if (ritem instanceof kCalIRecurrenceDate &&
                    ritem.isNegative && ritem.isFinite)
                {
                    // Only mess with the exception if its time is wrong.
                    var oldDate = aItem.startDate || aItem.entryDate;
                    if (ritem.date.compare(oldDate) != 0) {
                        var newRitem = ritem.clone();
                        // All we want from aItem is the time and timezone.
                        newRitem.date.timezone = oldDate.timezone;
                        newRitem.date.hour   = oldDate.hour;
                        newRitem.date.minute = oldDate.minute;
                        newRitem.date.second = oldDate.second;
                        newRitem.date.normalize();
                        aItem.recurrenceInfo.appendRecurrenceItem(newRitem);
                        aItem.recurrenceInfo.deleteRecurrenceItem(ritem);
                    }
                }
            }
            return aItem;
        }

        var subComp = calComp.getFirstSubcomponent("ANY");
        while (subComp) {
            switch (subComp.componentType) {
            case "VEVENT":
                var event = Components.classes["@mozilla.org/calendar/event;1"]
                                      .createInstance(Components.interfaces.calIEvent);
                event.icalComponent = subComp;

                // Only try to fix ICS from Sunbird 0.2 (and earlier) if it
                // has an EXDATE.
                hasExdate = subComp.getFirstProperty("EXDATE");
                if (isFromOldSunbird && hasExdate) {
                    event = fixOldSunbirdExceptions(event);
                }

                items.push(event);
                break;
            case "VTODO":
                var todo = Components.classes["@mozilla.org/calendar/todo;1"]
                                     .createInstance(Components.interfaces.calITodo);
                todo.icalComponent = subComp;

                // Only try to fix ICS from Sunbird 0.2 (and earlier) if it
                // has an EXDATE.
                hasExdate = subComp.getFirstProperty("EXDATE");
                if (isFromOldSunbird && hasExdate) {
                    todo = fixOldSunbirdExceptions(todo);
                }

                items.push(todo);
                break;
            default:
                // Nothing
            }
            subComp = calComp.getNextSubcomponent("ANY");
        }
        calComp = rootComp.getNextSubcomponent('VCALENDAR');
    }

    aCount.value = items.length;
    return items;
};


// Export

function calIcsExporter() {
    this.wrappedJSObject = this;
}

calIcsExporter.prototype.QueryInterface =
function QueryInterface(aIID) {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.calIExporter)) {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
};

calIcsExporter.prototype.getFileTypes =
function getFileTypes(aCount) {
    aCount.value = 1;
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
    return([{defaultExtension:'ics', 
             extensionFilter:'*.ics', 
             description: props.GetStringFromName('icsDesc')}]);
};

// not prototype.export. export is reserved.
calIcsExporter.prototype.exportToStream =
function ics_exportToStream(aStream, aCount, aItems) {
    icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);
    var calComp = icssrv.createIcalComponent("VCALENDAR");
    calComp.version = "2.0";
    calComp.prodid = "-//Mozilla.org/NONSGML Mozilla Calendar V1.1//EN";
    
    for each (item in aItems) {
        calComp.addSubcomponent(item.icalComponent);
        var rec = item.recurrenceInfo;
        if (rec != null) {
            var exceptions = rec.getExceptionIds({});
            for each ( var exid in exceptions ) {
                var ex = rec.getExceptionFor(exid, false);
                if (ex != null) {
                    calComp.addSubcomponent(ex.icalComponent);
                }
            }
        }    
    }
    var str = calComp.serializeToICS();

    // Convert the javascript string to an array of bytes, using the
    // utf8 encoder
    var convStream = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                               .getService(Components.interfaces.nsIConverterOutputStream);
    convStream.init(aStream, 'UTF-8', 0, 0x0000);

    convStream.writeString(str);
    convStream.close();

    return;
};
