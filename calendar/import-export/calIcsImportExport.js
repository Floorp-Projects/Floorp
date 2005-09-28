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
    return([{extension:'ics',description:'iCalendar'}]);
};

calIcsImporter.prototype.importFromStream =
function ics_importFromStream(aStream, aCount) {
    var items = new Array();

    // Interpret the byte-array as an utf8 string, and convert into a
    // javascript string.
    var convStream = Components.classes["@mozilla.org/intl/converter-input-stream;1"]
                               .getService(Components.interfaces.nsIConverterInputStream);
    convStream.init(aStream, 'UTF-8', 0, 0x0000);

    var tmpStr = {};
    var str = "";
    while (convStream.readString(-1, tmpStr)) {
        str += tmpStr.value;
    }

    icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);
    var calComp = icssrv.parseICS(str);
    var subComp = calComp.getFirstSubcomponent("ANY");
    while (subComp) {
        switch (subComp.componentType) {
        case "VEVENT":
            var event = Components.classes["@mozilla.org/calendar/event;1"]
                                  .createInstance(Components.interfaces.calIEvent);
            event.icalComponent = subComp;
            items.push(event);
            break;
        case "VTODO":
            var todo = Components.classes["@mozilla.org/calendar/todo;1"]
                                 .createInstance(Components.interfaces.calITodo);
            todo.icalComponent = subComp;
            items.push(todo);
            break;
        default:
            // Nothing
        }
        subComp = calComp.getNextSubcomponent("ANY");
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
    return([{extension:'ics',description:'iCalendar'}]);
};

// not prototype.export. export is reserved.
calIcsExporter.prototype.exportToStream =
function ics_exportToStream(aStream, aCount, aItems) {
    icssrv = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);
    var calComp = icssrv.createIcalComponent("VCALENDAR");
    calComp.version = "2.0";
    calComp.prodid = "-//Mozilla.org/NONSGML Mozilla Calendar V1.0//EN";
    
    for each (item in aItems) {
        calComp.addSubcomponent(item.icalComponent);
    }
    var str = calComp.serializeToICS();

    // Convert the javascript string to an araay of bytes, using the
    // utf8 encoder
    var convStream = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                               .getService(Components.interfaces.nsIConverterOutputStream);
    convStream.init(aStream, 'UTF-8', 0, 0x0000);

    convStream.writeString(str);
    convStream.close();

    return;
};
