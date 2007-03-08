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

function getIcsFileTypes(aCount) {
    aCount.value = 1;
    var wildmat = '*.ics';
    var label = calGetString("calendar", 'filterIcs', [wildmat]);
    return([{defaultExtension:'ics', 
             extensionFilter: wildmat, 
             description: label}]);
}

calIcsImporter.prototype.getFileTypes = getIcsFileTypes;

calIcsImporter.prototype.importFromStream =
function ics_importFromStream(aStream, aCount) {
    var parser = Components.classes["@mozilla.org/calendar/ics-parser;1"].
                            createInstance(Components.interfaces.calIIcsParser);
    parser.parseFromStream(aStream);
    return parser.getItems(aCount);
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

calIcsExporter.prototype.getFileTypes = getIcsFileTypes;

// not prototype.export. export is reserved.
calIcsExporter.prototype.exportToStream =
function ics_exportToStream(aStream, aCount, aItems) {
    var serializer = Components.classes["@mozilla.org/calendar/ics-serializer;1"].
                                createInstance(Components.interfaces.calIIcsSerializer);
    serializer.addItems(aItems, aItems.length);
    serializer.serializeToStream(aStream);
};
