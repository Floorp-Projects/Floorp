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
 * The Original Code is Google Calendar Provider code.
 *
 * The Initial Developer of the Original Code is
 *   Philipp Kewisch (mozilla@kewis.ch)
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

// This constant is used internally to signal a failed login to the login
// handler's response function.
const kGOOGLE_LOGIN_FAILED = 1;

var g_classInfo = {
     calGoogleCalendar: {
        getInterfaces: function cI_cGC_getInterfaces (count) {
            var ifaces = [
                Components.interfaces.nsISupports,
                Components.interfaces.calICalendar,
                Components.interfaces.nsIClassInfo
            ];
            count.value = ifaces.length;
            return ifaces;
        },

        getHelperForLanguage: function (language) {
            return null;
        },

        classDescription: "Google Calendar Provider",
        contractID: "@mozilla.org/calendar/calendar;1?type=gdata",
        classID:  Components.ID("{d1a6e988-4b4d-45a5-ba46-43e501ea96e3}"),
        implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
        constructor: "calGoogleCalendar",
        flags: 0
    }
};

var calGoogleCalendarModule = {

    mUtilsLoaded: false,

    loadUtils: function cGCM_loadUtils() {
        if (this.mUtilsLoaded)
            return;

        const scripts = ["calGoogleCalendar.js", "calGoogleSession.js",
                         "calUtils.js", "calGoogleRequest.js",
                         "calGoogleUtils.js"];

        var loader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                               .getService(Components.interfaces.mozIJSSubScriptLoader);

        var iosvc = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);

        // Note that unintuitively, __LOCATION__.parent == .
        // We expect to find the subscripts in ./../js
        var appdir = __LOCATION__.parent.parent;
        appdir.append("js");

        for each (var script in scripts) {
            var scriptName = appdir.clone();
            scriptName.append(script);

            try {
                var scriptUri = iosvc.newFileURI(scriptName);
                loader.loadSubScript(scriptUri.spec, null);
            } catch (e) {
                dump("Error while loading " + scriptUri.spec + "\n");
                throw e;
            }
        }

        this.mUtilsLoaded = true;
    },

    unregisterSelf: function cGCM_unregisterSelf(aComponentManager) {
        aComponentManager = aComponentManager
                            .QueryInterface(Components.interfaces.nsIComponentRegistrar);
        for each (var component in g_classInfo) {
            aComponentManager.unregisterFactoryLocation(component.classID);
        }
    },

    registerSelf: function cGCM_registerSelf(aComponentManager,
                                             aFileSpec,
                                             aLocation,
                                             aType) {
        aComponentManager = aComponentManager
                            .QueryInterface(Components.interfaces.nsIComponentRegistrar);

        for each (var component in g_classInfo) {
            dump("Registering " + component.classDescription + "\n");

            aComponentManager.registerFactoryLocation(
                component.classID,
                component.classDescription,
                component.contractID,
                aFileSpec,
                aLocation,
                aType);
        }
    },

    makeFactoryFor: function cGCM_makeFactoryFor(aConstructor) {
        var factory = {
            QueryInterface: function (aIID) {
                if (!aIID.equals(Components.interfaces.nsISupports) &&
                    !aIID.equals(Components.interfaces.nsIFactory))
                    throw Components.results.NS_ERROR_NO_INTERFACE;
                return this;
            },

            createInstance: function (aOuter, aIID) {
                if (aOuter != null)
                    throw Components.results.NS_ERROR_NO_AGGREGATION;
                return (new aConstructor()).QueryInterface(aIID);
            }
        };
        return factory;
    },

    getClassObject: function cGCM_getClassObject(aComponentManager,
                                                 aCID,
                                                 aIID) {
        if (!aIID.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        this.loadUtils();

        for each (var component in g_classInfo) {
            if (aCID.equals(component.classID)) {
                return this.makeFactoryFor(eval(component.constructor));
            }
        }
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    canUnload: function(aComponentManager) {
        return true;
    }
};

function NSGetModule(aComponentManager, aFileSpec) {
    return calGoogleCalendarModule;
}
