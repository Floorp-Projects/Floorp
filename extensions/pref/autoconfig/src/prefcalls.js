/* global processLDAPValues */
/* -*- tab-width: 4; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals gSandbox */

const nsILDAPURL = Ci.nsILDAPURL;
const LDAPURLContractID = "@mozilla.org/network/ldap-url;1";
const nsILDAPSyncQuery = Ci.nsILDAPSyncQuery;
const LDAPSyncQueryContractID = "@mozilla.org/ldapsyncquery;1";
const nsIPrefService = Ci.nsIPrefService;
const PrefServiceContractID = "@mozilla.org/preferences-service;1";

// ChromeUtils isn't available here, so we can't use Services.*
/* eslint-disable mozilla/use-services */

var gVersion;
var gIsUTF8;

function getPrefBranch() {

    var prefService = Cc[PrefServiceContractID]
                        .getService(nsIPrefService);
    return prefService.getBranch(null);
}

function pref(prefName, value) {

    try {
        var prefBranch = getPrefBranch();

        if (typeof value == "string") {
            if (gIsUTF8) {
                prefBranch.setStringPref(prefName, value);
                return;
            }
            prefBranch.setCharPref(prefName, value);
        } else if (typeof value == "number") {
            prefBranch.setIntPref(prefName, value);
        } else if (typeof value == "boolean") {
            prefBranch.setBoolPref(prefName, value);
        }
    } catch (e) {
        displayError("pref", e);
    }
}

function defaultPref(prefName, value) {

    try {
        var prefService = Cc[PrefServiceContractID]
                            .getService(nsIPrefService);
        var prefBranch = prefService.getDefaultBranch(null);
        if (typeof value == "string") {
            if (gIsUTF8) {
                prefBranch.setStringPref(prefName, value);
                return;
            }
            prefBranch.setCharPref(prefName, value);
        } else if (typeof value == "number") {
            prefBranch.setIntPref(prefName, value);
        } else if (typeof value == "boolean") {
            prefBranch.setBoolPref(prefName, value);
        }
    } catch (e) {
        displayError("defaultPref", e);
    }
}

function lockPref(prefName, value) {

    try {
        var prefBranch = getPrefBranch();

        if (prefBranch.prefIsLocked(prefName))
            prefBranch.unlockPref(prefName);

        defaultPref(prefName, value);

        prefBranch.lockPref(prefName);
    } catch (e) {
        displayError("lockPref", e);
    }
}

function unlockPref(prefName) {

    try {

        var prefBranch = getPrefBranch();
        prefBranch.unlockPref(prefName);
    } catch (e) {
        displayError("unlockPref", e);
    }
}

function getPref(prefName) {

    try {
        var prefBranch = getPrefBranch();

        switch (prefBranch.getPrefType(prefName)) {

        case prefBranch.PREF_STRING:
            if (gIsUTF8) {
                return prefBranch.getStringPref(prefName);
            }
            return prefBranch.getCharPref(prefName);

        case prefBranch.PREF_INT:
            return prefBranch.getIntPref(prefName);

        case prefBranch.PREF_BOOL:
            return prefBranch.getBoolPref(prefName);
        default:
            return null;
        }
    } catch (e) {
        displayError("getPref", e);
    }
    return undefined;
}

function clearPref(prefName) {

    try {
        var prefBranch = getPrefBranch();
            prefBranch.clearUserPref(prefName);
    } catch (e) {
    }

}

function setLDAPVersion(version) {
    gVersion = version;
}


function getLDAPAttributes(host, base, filter, attribs, isSecure) {

    try {
        var urlSpec = "ldap" + (isSecure ? "s" : "") + "://" + host + (isSecure ? ":636" : "") + "/" + base + "?" + attribs + "?sub?" +
                      filter;

        var url = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService)
                    .newURI(urlSpec)
                    .QueryInterface(Ci.nsILDAPURL);

        var ldapquery = Cc[LDAPSyncQueryContractID]
                          .createInstance(nsILDAPSyncQuery);
        // default to LDAP v3
        if (!gVersion)
          gVersion = Ci.nsILDAPConnection.VERSION3;
        // user supplied method
        processLDAPValues(ldapquery.getQueryResults(url, gVersion));
    } catch (e) {
        displayError("getLDAPAttibutes", e);
    }
}

function getLDAPValue(str, key) {

    try {
        if (str == null || key == null)
            return null;

        var search_key = "\n" + key + "=";

        var start_pos = str.indexOf(search_key);
        if (start_pos == -1)
            return null;

        start_pos += search_key.length;

        var end_pos = str.indexOf("\n", start_pos);
        if (end_pos == -1)
            end_pos = str.length;

        return str.substring(start_pos, end_pos);
    } catch (e) {
        displayError("getLDAPValue", e);
    }
    return undefined;
}

function displayError(funcname, message) {

    try {
        var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"]
                              .getService(Ci.nsIPromptService);
        var bundle = Cc["@mozilla.org/intl/stringbundle;1"]
                       .getService(Ci.nsIStringBundleService)
                       .createBundle("chrome://autoconfig/locale/autoconfig.properties");

         var title = bundle.GetStringFromName("autoConfigTitle");
         var msg = bundle.formatStringFromName("autoConfigMsg", [funcname], 1);
         promptService.alert(null, title, msg + " " + message);
    } catch (e) { }
}

function getenv(name) {
    try {
        var environment = Cc["@mozilla.org/process/environment;1"].
            getService(Ci.nsIEnvironment);
        return environment.get(name);
    } catch (e) {
        displayError("getEnvironment", e);
    }
    return undefined;
}

var APIs = {
    pref,
    defaultPref,
    lockPref,
    unlockPref,
    getPref,
    clearPref,
    setLDAPVersion,
    getLDAPAttributes,
    getLDAPValue,
    displayError,
    getenv,
};

for (let [defineAs, func] of Object.entries(APIs)) {
    Cu.exportFunction(func, gSandbox, {defineAs});
}

Object.defineProperty(Cu.waiveXrays(gSandbox), "gIsUTF8", {
  get: Cu.exportFunction(() => gIsUTF8, gSandbox),
});
