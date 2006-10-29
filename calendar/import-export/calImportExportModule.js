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

const componentData =
    [
    {cid: Components.ID("{1e3e33dc-445a-49de-b2b6-15b2a050bb9d}"),
     contractid: "@mozilla.org/calendar/import;1?type=ics",
     script: "calIcsImportExport.js",
     constructor: "calIcsImporter",
     category: "cal-importers",
     categoryEntry: "cal-ics-importer",
     service: false},
    {cid: Components.ID("{a6a524ce-adff-4a0f-bb7d-d1aaad4adc60}"),
     contractid: "@mozilla.org/calendar/export;1?type=ics",
     constructor: "calIcsExporter",
     category: "cal-exporters",
     categoryEntry: "cal-ics-exporter",
     service: false},

    {cid: Components.ID("{72d9ab35-9b1b-442a-8cd0-ae49f00b159b}"),
     contractid: "@mozilla.org/calendar/export;1?type=htmllist",
     script: "calHtmlExport.js",
     constructor: "calHtmlExporter",
     category: "cal-exporters",
     categoryEntry: "cal-html-list-exporter",
     service: false},

    {cid: Components.ID("{9ae04413-fee3-45b9-8bbb-1eb39a4cbd1b}"),
     contractid: "@mozilla.org/calendar/printformatter;1?type=list",
     script: "calListFormatter.js",
     constructor: "calListFormatter",
     category: "cal-print-formatters",
     categoryEntry: "cal-list-printformatter",
     service: false},
    {cid: Components.ID("{f42d5132-92c4-487b-b5c8-38bf292d74c1}"),
     contractid: "@mozilla.org/calendar/printformatter;1?type=monthgrid",
     script: "calMonthGridPrinter.js",
     constructor: "calMonthPrinter",
     category: "cal-print-formatters",
     categoryEntry: "cal-month-printer",
     service: false},
    {cid: Components.ID("{2d6ec97b-9109-4b92-89c5-d4b4806619ce}"),
     contractid: "@mozilla.org/calendar/printformatter;1?type=weekplan",
     script: "calWeekPrinter.js",
     constructor: "calWeekPrinter",
     category: "cal-print-formatters",
     categoryEntry: "cal-week-planner-printer",
     service: false},

    {cid: Components.ID("{64a5d17a-0497-48c5-b54f-72b15c9e9a14}"),
     contractid: "@mozilla.org/calendar/import;1?type=csv",
     script: "calOutlookCSVImportExport.js",
     constructor: "calOutlookCSVImporter",
     category: "cal-importers",
     categoryEntry: "cal-outlookcsv-importer",
     service: false},
    {cid: Components.ID("{48e6d3a6-b41b-4052-9ed2-40b27800bd4b}"),
     contractid: "@mozilla.org/calendar/export;1?type=csv",
     constructor: "calOutlookCSVExporter",
     category: "cal-exporters",
     categoryEntry: "cal-outlookcsv-exporter",
     service: false},

    {cid: null,
     contractid: null,
     script: "calUtils.js",
     constructor: null,
     category: null,
     categoryEntry: null,
     service: false},

    ];

var calImportExportModule = {
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
    return calImportExportModule;
}
