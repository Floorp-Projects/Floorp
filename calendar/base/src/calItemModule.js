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

var calItemModule = {
    /* Update these in calBaseCID.h */
    mEventCID: Components.ID("{974339d5-ab86-4491-aaaf-2b2ca177c12b}"),
    mEventContractId: "@mozilla.org/calendar/event;1",

    mTodoCID: Components.ID("{7af51168-6abe-4a31-984d-6f8a3989212d}"),
    mTodoContractId: "@mozilla.org/calendar/todo;1",

    mItemOccurrenceCID: Components.ID("{bad672b3-30b8-4ecd-8075-7153313d1f2c}"),
    mItemOccurrenceContractId: "@mozilla.org/calendar/item-occurrence;1",

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

        // NS_XPCOM_COMPONENT_DIR
        var appdir = dirsvc.get("ComsD", Components.interfaces.nsIFile);
        var files = [ "calItemBase.js", "calEvent.js" ];

        for (var i = 0; i < files.length; i++) {
            var f = appdir.clone();
            f.append(files[i]);

            var fileurl = iosvc.newFileURI(f);
            loader.loadSubScript(fileurl.spec, null);
        }

        this.mScriptsLoaded = true;
    },

    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

        var cids = [ this.mEventCID, this.mTodoCID, this.mItemOccurrenceCID ];
        var contractids = [ this.mEventContractId, this.mTodoContractId, this.mItemOccurrenceContractId ];

        for (var i = 0; i < cids.length; i++) {
            dump ("calItemModule: registering " + contractids[i] + "\n");
            compMgr.registerFactoryLocation(cids[i],
                                            "",
                                            contractids[i],
                                            fileSpec,
                                            location,
                                            type);
        }
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        if (!this.mScriptsLoaded)
            this.loadScripts();

        if (cid.equals(this.mEventCID))
            return this.mEventFactory;

        if (cid.equals(this.mTodoCID))
            return this.mTodoFactory;

        if (cid.equals(this.mItemOccurrenceCID))
            return this.mItemOccurrenceFactory;

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    mEventFactory: {
        QueryInterface: function (aIID) {
            if (!aIID.equals(Components.interfaces.nsISupports) &&
                !aIID.equals(Components.interfaces.nsIFactory))
                throw Components.results.NS_ERROR_NO_INTERFACE;
            return this;
        },

        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            return (new calEvent()).QueryInterface(iid);
        }
    },

    mTodoFactory: {
        QueryInterface: function (aIID) {
            if (!aIID.equals(Components.interfaces.nsISupports) &&
                !aIID.equals(Components.interfaces.nsIFactory))
                throw Components.results.NS_ERROR_NO_INTERFACE;
            return this;
        },

        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            return (new calTodo()).QueryInterface(iid);
        }
    },

    mItemOccurrenceFactory: {
        QueryInterface: function (aIID) {
            if (!aIID.equals(Components.interfaces.nsISupports) &&
                !aIID.equals(Components.interfaces.nsIFactory))
                throw Components.results.NS_ERROR_NO_INTERFACE;
            return this;
        },

        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            return (new calItemOccurrence()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return calItemModule;
}
