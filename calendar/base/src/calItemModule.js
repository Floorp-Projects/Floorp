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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Mike Shaver <shaver@off.net>
 *   Matthew Willis <lilmatt@mozilla.com>
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

const kCalRecurrenceInfoContractID = "@mozilla.org/calendar/recurrence-info;1";
const kCalIRecurrenceInfo = Components.interfaces.calIRecurrenceInfo;

const kCalRecurrenceRuleContractID = "@mozilla.org/calendar/recurrence-rule;1";
const kCalIRecurrenceRule = Components.interfaces.calIRecurrenceRule;

const kCalRecurrenceDateSetContractID = "@mozilla.org/calendar/recurrence-date-set;1";
const kCalIRecurrenceDateSet = Components.interfaces.calIRecurrenceDateSet;

const kCalRecurrenceDateContractID = "@mozilla.org/calendar/recurrence-date;1";
const kCalIRecurrenceDate = Components.interfaces.calIRecurrenceDate;

/* Constructor helpers, lazily initialized to avoid registration races. */
var CalRecurrenceInfo = null;
var CalRecurrenceRule = null;
var CalRecurrenceDateSet = null;
var CalRecurrenceDate = null;
var CalDateTime = null;
var CalDuration = null;
var CalAttendee = null;
var CalItipItem = null;

var componentInitRun = false;
function initBaseComponent()
{
    CalRecurrenceInfo = new Components.Constructor(kCalRecurrenceInfoContractID, kCalIRecurrenceInfo);
    CalRecurrenceRule = new Components.Constructor(kCalRecurrenceRuleContractID, kCalIRecurrenceRule);
    CalRecurrenceDateSet = new Components.Constructor(kCalRecurrenceDateSetContractID, kCalIRecurrenceDateSet);
    CalRecurrenceDate = new Components.Constructor(kCalRecurrenceDateContractID, kCalIRecurrenceDate);

    CalDateTime = new Components.Constructor("@mozilla.org/calendar/datetime;1",
                                             Components.interfaces.calIDateTime);
    CalDuration = new Components.Constructor("@mozilla.org/calendar/duration;1",
                                             Components.interfaces.calIDateTime);
    CalAttendee = new Components.Constructor("@mozilla.org/calendar/attendee;1",
                                             Components.interfaces.calIAttendee);
    CalItipItem = new Components.Constructor("@mozilla.org/calendar/itip-item;1",
                                             Components.interfaces.calIItipItem);
}


/* Update these in calBaseCID.h */
const componentData =
    [
     /* calItemBase and calUtils must be first: later scripts depend on them */
    {cid: null,
     contractid: null,
     script: "calItemBase.js",
     constructor: null},

    {cid: null,
     contractid: null,
     script: "calUtils.js",
     constructor: null},

    {cid: Components.ID("{f42585e7-e736-4600-985d-9624c1c51992}"),
     contractid: "@mozilla.org/calendar/manager;1",
     script: "calCalendarManager.js",
     constructor: "calCalendarManager",
     onComponentLoad: "onCalCalendarManagerLoad()"},

    {cid: Components.ID("{7a9200dd-6a64-4fff-a798-c5802186e2cc}"),
     contractid: "@mozilla.org/calendar/alarm-service;1",
     script: "calAlarmService.js",
     constructor: "calAlarmService",
     service: true},

    {cid: Components.ID("{974339d5-ab86-4491-aaaf-2b2ca177c12b}"),
     contractid: "@mozilla.org/calendar/event;1",
     script: "calEvent.js",
     constructor: "calEvent"},

    {cid: Components.ID("{7af51168-6abe-4a31-984d-6f8a3989212d}"),
     contractid: "@mozilla.org/calendar/todo;1",
     script: "calTodo.js",
     constructor: "calTodo"},

    {cid: Components.ID("{5c8dcaa3-170c-4a73-8142-d531156f664d}"),
     contractid: "@mozilla.org/calendar/attendee;1",
     script: "calAttendee.js",
     constructor: "calAttendee"},

    {cid: Components.ID("{5f76b352-ab75-4c2b-82c9-9206dbbf8571}"),
     contractid: "@mozilla.org/calendar/attachment;1",
     script: "calAttachment.js",
     constructor: "calAttachment"},

    {cid: Components.ID("{04027036-5884-4a30-b4af-f2cad79f6edf}"),
     contractid: "@mozilla.org/calendar/recurrence-info;1",
     script: "calRecurrenceInfo.js",
     constructor: "calRecurrenceInfo"},

    {cid: Components.ID("{4123da9a-f047-42da-a7d0-cc4175b9f36a}"),
     contractid: "@mozilla.org/calendar/datetime-formatter;1",
     script: "calDateTimeFormatter.js",
     constructor: "calDateTimeFormatter"},

    {cid: Components.ID("{6877bbdd-f336-46f5-98ce-fe86d0285cc1}"),
     contractid: "@mozilla.org/calendar/weektitle-service;1",
     script: "calWeekTitleService.js",
     constructor: "calWeekTitleService"},

    {cid: Components.ID("{f41392ab-dcad-4bad-818f-b3d1631c4d93}"),
     contractid: "@mozilla.org/calendar/itip-item;1",
     script: "calItipItem.js",
     constructor: "calItipItem"},

    {cid: Components.ID("{9787876b-0780-4464-8282-b7f86fb221e8}"),
     contractid: "@mozilla.org/calendar/itip-processor;1",
     script: "calItipProcessor.js",
     constructor: "calItipProcessor"},

    {cid: Components.ID("{1e2fc0e2-bf5f-4d60-9f1e-5e92cf517c0b}"),
     contractid: "@mozilla.org/network/protocol;1?name=webcal",
     script: "calProtocolHandler.js",
     constructor: "calProtocolHandler"},

    {cid: Components.ID("{6fe88047-75b6-4874-80e8-5f5800f14984}"),
     contractid: "@mozilla.org/calendar/ics-parser;1",
     script: "calIcsParser.js",
     constructor: "calIcsParser"},

    {cid: Components.ID("{207a6682-8ff1-4203-9160-729ec28c8766}"),
     contractid: "@mozilla.org/calendar/ics-serializer;1",
     script: "calIcsSerializer.js",
     constructor: "calIcsSerializer"}
    ];

var calItemModule = {
    mScriptsLoaded: false,
    loadScripts: function () {
        if (this.mScriptsLoaded)
            return;

        const jssslContractID = "@mozilla.org/moz/jssubscript-loader;1";
        const jssslIID = Components.interfaces.mozIJSSubScriptLoader;

        const dirsvcContractID = "@mozilla.org/file/directory_service;1";
        const propsIID = Components.interfaces.nsIProperties;

        const iosvcContractID = "@mozilla.org/network/io-service;1";
        const iosvcIID = Components.interfaces.nsIIOService;

        var loader = Components.classes[jssslContractID].getService(jssslIID);
        var dirsvc = Components.classes[dirsvcContractID].getService(propsIID);
        var iosvc = Components.classes[iosvcContractID].getService(iosvcIID);

        // Note that unintuitively, __LOCATION__.parent == .
        // We expect to find the subscripts in ./../js
        var appdir = __LOCATION__.parent.parent;
        appdir.append("js");

        for (var i = 0; i < componentData.length; i++) {
            var scriptName = componentData[i].script;
            if (!scriptName)
                continue;

            var f = appdir.clone();
            f.append(scriptName);

            try {
                var fileurl = iosvc.newFileURI(f);
                loader.loadSubScript(fileurl.spec, null);
            } catch (e) {
                dump("Error while loading " + fileurl.spec + "\n");
                throw e;
            }
        }

        this.mScriptsLoaded = true;
    },

    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

        var catman = Components.classes["@mozilla.org/categorymanager;1"]
            .getService(Components.interfaces.nsICategoryManager);
        for (var i = 0; i < componentData.length; i++) {
            var comp = componentData[i];
            if (!comp.cid)
                continue;
            compMgr.registerFactoryLocation(comp.cid,
                                            "",
                                            comp.contractid,
                                            fileSpec,
                                            location,
                                            type);

            if (comp.category) {
                var contractid;
                if (comp.service)
                    contractid = "service," + comp.contractid;
                else
                    contractid = comp.contractid;
                catman.addCategoryEntry(comp.category, comp.categoryEntry,
                                        contractid, true, true);
            }
        }
    },

    makeFactoryFor: function(constructor) {
        var factory = {
            QueryInterface: function (aIID) {
                if (!aIID.equals(Components.interfaces.nsISupports) &&
                    !aIID.equals(Components.interfaces.nsIFactory))
                    throw Components.results.NS_ERROR_NO_INTERFACE;
                return this;
            },

            createInstance: function (outer, iid) {
                if (outer != null)
                    throw Components.results.NS_ERROR_NO_AGGREGATION;
                return (new constructor()).QueryInterface(iid);
            }
        };

        return factory;
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        if (!this.mScriptsLoaded)
            this.loadScripts();

        if (!componentInitRun) {
            initBaseComponent();
            componentInitRun = true;
        }

        for (var i = 0; i < componentData.length; i++) {
            if (cid.equals(componentData[i].cid)) {
                if (componentData[i].onComponentLoad) {
                    eval(componentData[i].onComponentLoad);
                }
                // eval to get usual scope-walking
                return this.makeFactoryFor(eval(componentData[i].constructor));
            }
        }

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return calItemModule;
}
