/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Lightning code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
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

const CI = Components.interfaces;

function makeTableRow(val) {
    return "<tr><td>" + val[0] + "</td><td>" + val[1] + "</td></tr>\n";
}

function makeButton(type, content) {
    return "<button type='submit' value='" + type + "'>" + content + "</button>";
}

function startForm(calendarData) {
    var form = "<form method='GET' action='moz-cal-handle-itip:'>\n";
    form += "<input type='hidden' name='preferredCalendar' value=''>\n";
    // We use escape instead of encodeURI*, because we need to deal with single quotes, sigh
    form += "<input type='hidden' name='data' value='" + escape(calendarData) + "'>\n";
    return form;
}

function ltnMimeConverter() { }

ltnMimeConverter.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(CI.nsISupports) &&
            !aIID.equals(CI.nsISimpleMimeConverter))
            throw Components.interfaces.NS_ERROR_NO_INTERFACE;

        return this;
    },

    convertToHTML: function(contentType, data) {
        // dump("converting " + contentType + " to HTML\n");

        var event = Components.classes["@mozilla.org/calendar/event;1"]
            .createInstance(CI.calIEvent);
        event.icalString = data;

        var html = "<script src='chrome://lightning/content/text-calendar-handler.js'></script>\n";
        html += "<center>\n";
        html += startForm(data);
        html += "<table bgcolor='#CCFFFF'>\n";
        var organizer = event.organizer;
        var rows = [["Invitation from", "<a href='" + organizer.id + "'>" + 
                                        organizer.commonName + "</a>"],
                    ["Topic:", event.title],
                    ["Start:", event.startDate.jsDate.toLocaleTimeString()],
                    ["End:", event.endDate.jsDate.toLocaleTimeString()]];
        html += rows.map(makeTableRow).join("\n");
        html += "<tr><td colspan='2'>";
        html += makeButton("accept", "Accept meeting") + " ";
        html += makeButton("decline", "Decline meeting") + " ";
        html += "</td></tr>\n";
        html += "</table>\n</form>\n</center>";

        // dump("Generated HTML:\n\n" + html + "\n\n");
        return html;
    },
    
};

var myModule = {
    registerSelf: function (compMgr, fileSpec, location, type) {
        debug("*** Registering Lightning text/calendar handler\n");
        compMgr = compMgr.QueryInterface(CI.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.myCID,
                                        "Lightning text/calendar handler",
                                        this.myContractID,
                                        fileSpec,
                                        location,
                                        type);

        var catman = Components.classes["@mozilla.org/categorymanager;1"]
            .getService(CI.nsICategoryManager);

        catman.addCategoryEntry("simple-mime-converters", "text/calendar",
                                this.myContractID, true, true);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    myCID: Components.ID("{c70acb08-464e-4e55-899d-b2c84c5409fa}"),

    myContractID: "@mozilla.org/lightning/mime-converter;1",

    myFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new ltnMimeConverter()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return myModule;
}


