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

// Import

function calIcsSerializer() {
    this.wrappedJSObject = this;
    this.mItems = [];
    this.mProperties = [];
    this.mComponents = [];
}

calIcsSerializer.prototype.QueryInterface =
function QueryInterface(aIID) {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.calIIcsSerializer)) {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
};

calIcsSerializer.prototype.addItems =
function is_addItems(aItems, aCount) {
    if (aCount > 0) {
        this.mItems = this.mItems.concat(aItems);
    }
}

calIcsSerializer.prototype.addProperty =
function is_addProperty(aProperty) {
   this.mProperties.push(aProperty);
}

calIcsSerializer.prototype.addComponent =
function is_addComponent(aComponent) {
   this.mComponents.push(aComponent);
}

calIcsSerializer.prototype.serializeToString =
function is_serializeToString() {
    icsSvc = Components.classes["@mozilla.org/calendar/ics-service;1"]
                       .getService(Components.interfaces.calIICSService);
    var calComp = icsSvc.createIcalComponent("VCALENDAR");
    calComp.version = "2.0";
    calComp.prodid = "-//Mozilla.org/NONSGML Mozilla Calendar V1.1//EN";

    for each (var prop in this.mProperties) {
        calComp.addProperty(prop);
    }
    for each (var comp in this.mComponents) {
        calComp.addSubcomponent(comp);
    }

    for each (var item in this.mItems) {
        calComp.addSubcomponent(item.icalComponent);
        var rec = item.recurrenceInfo;
        if (rec != null) {
            var exceptions = rec.getExceptionIds({});
            for each (var exid in exceptions) {
                var ex = rec.getExceptionFor(exid, false);
                if (ex != null) {
                    calComp.addSubcomponent(ex.icalComponent);
                }
            }
        }
    }

    // do the actual serialization
    return calComp.serializeToICS();
}

calIcsSerializer.prototype.serializeToStream =
function is_serializeToStream(aStream) {
    var str = this.serializeToString();

    // Convert the javascript string to an array of bytes, using the
    // UTF8 encoder
    var convStream = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                               .getService(Components.interfaces.nsIConverterOutputStream);
    convStream.init(aStream, 'UTF-8', 0, 0x0000);

    convStream.writeString(str);
    convStream.close();

    return;
};
