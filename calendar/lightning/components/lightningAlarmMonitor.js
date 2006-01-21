/* -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
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

function getAlarmService()
{
    return Components.classes["@mozilla.org/calendar/alarm-service;1"].getService(CI.calIAlarmService);
}

var alarmServiceObserver = {
    onAlarm: function(event) {
        var wmediator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                                  .getService(Components.interfaces.nsIWindowMediator);

        var calAlarmWindow = wmediator.getMostRecentWindow("calendarAlarmWindow");
        if (!calAlarmWindow) {
            var windowWatcher = Components.classes["@mozilla.org/embedcomp/window-watcher;1"].getService(CI.nsIWindowWatcher);
            calAlarmWindow = windowWatcher.openWindow(null,
                                                    "chrome://calendar/content/calendar-alarm-dialog.xul",
                                                    "_blank",
                                                    "chrome,dialog=yes,all",
                                                    null);
        }

        if ("addAlarm" in calAlarmWindow) {
            calAlarmWindow.addAlarm(event);
        } else {
            var addAlarm = function() {
                calAlarmWindow.addAlarm(event);
            }
            calAlarmWindow.addEventListener("load", addAlarm, false);
        }
    }
};

function ltnAlarmMonitor() { }

ltnAlarmMonitor.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(CI.nsISupports) &&
            !aIID.equals(CI.nsIObserver))
            throw Components.interfaces.NS_ERROR_NO_INTERFACE;

        return this;
    },

    /* nsIObserver */
    observe: function (subject, topic, data) {
        switch (topic) {
        case "alarm-service-startup":
            getAlarmService().addObserver(alarmServiceObserver);
            break;

        case "alarm-service-shutdown":
            getAlarmService().removeObserver(alarmServiceObserver);
            break;
        }
    },
};

var myModule = {
    registerSelf: function (compMgr, fileSpec, location, type) {
        debug("*** Registering Lightning alarm monitor\n");
        compMgr = compMgr.QueryInterface(CI.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.myCID,
                                        "Lightning Alarm Monitor",
                                        this.myContractID,
                                        fileSpec,
                                        location,
                                        type);

        var catman = Components.classes["@mozilla.org/categorymanager;1"]
            .getService(CI.nsICategoryManager);

        catman.addCategoryEntry("alarm-service-startup", "lightning",
                                "service," + this.myContractID, true, true);
        catman.addCategoryEntry("alarm-service-shutdown", "lightning",
                                "service," + this.myContractID, true, true);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(CI.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    myCID: Components.ID("{70fc36e6-2658-4999-9754-49d84cfb83a1}"),

    myContractID: "@mozilla.org/lightning/alarm-monitor;1",

    myFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new ltnAlarmMonitor()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return myModule;
}


