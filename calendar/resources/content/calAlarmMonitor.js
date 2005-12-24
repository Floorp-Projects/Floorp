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
 *   Joey Minta       <jminta@gmail.com>
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
    onAlarm: function( event ) {

        //If an event has alarm email address, we assume the alarm is of email
        //type.  See similar note in eventDialog.js
        if(event.alarmEmailAddress && event.alarmEmailAddress != "") {
            //XXX Re-implement this!
            return;
        }

        //The alarm dialog will play a sound, but if it isn't opened, we need
        //to play a sound on our own here.
        var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefService);
        var calendarPrefs = prefService.getBranch("calendar.alarms.");
        if(!calendarPrefs.getBoolPref("show") && 
           calendarPrefs.getBoolPref("playsound")) {
            playSound();
            return;
        }

        //Check to see if an alarm window already exists.  It not, create it.
        var wWatcher = Components.classes[
                       "@mozilla.org/embedcomp/window-watcher;1"]
                       .getService( Components.interfaces.nsIWindowWatcher );
        var gAlarmWindow = wWatcher.getWindowByName( "calendar-alarm-window", null);
        if( !gAlarmWindow ) {
            gAlarmWindow = wWatcher.openWindow(null,
                                               "chrome://calendar/content/calendar-alarm-dialog.xul",
                                               "_blank",
                                               "chrome,dialog=yes,all",
                                                null);
            gAlarmWindow.name = "calendar-alarm-window";

        }
        gAlarmWindow.focus();
        if ("addAlarm" in gAlarmWindow) {
            gAlarmWindow.addAlarm(event);
        } 
        else {
            var addAlarm = function() {
                gAlarmWindow.addAlarm( event );
            }
            gAlarmWindow.addEventListener("load", addAlarm, false);
        }
    }
};

function playSound() {
    var sound = Components.classes["@mozilla.org/sound;1"]
                .createInstance( Components.interfaces.nsISound );
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefService);
    var calendarPrefs = prefService.getBranch("calendar.alarms.");
    try {
        soundURL = calendarPrefs.getCharPref("soundURL");
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
        var url = ioService.newURI(soundURL, null, null);
        sound.init();
        sound.play( url );
    }
    catch (ex) {
       sound.beep();
       dump("unable to play sound...\n" + ex + "\n");
    }
}

function calAlarmMonitor() { }

calAlarmMonitor.prototype = {
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
        compMgr = compMgr.QueryInterface(CI.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.myCID,
                                        "Calendar Alarm Monitor",
                                        this.myContractID,
                                        fileSpec,
                                        location,
                                        type);

        var catman = Components.classes["@mozilla.org/categorymanager;1"]
            .getService(CI.nsICategoryManager);

        catman.addCategoryEntry("alarm-service-startup", "calendar",
                                "service," + this.myContractID, true, true);
        catman.addCategoryEntry("alarm-service-shutdown", "calendar",
                                "service," + this.myContractID, true, true);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(CI.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    myCID: Components.ID("{4b7ae030-ed79-11d9-8cd6-0800200c9a66}"),

    myContractID: "@mozilla.org/calendar/alarm-monitor;1",

    myFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new calAlarmMonitor()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return myModule;
}


