/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mitesh Shah <mitesh@netscape.com>
 *   Brian Nesse <bnesse@netscape.com>
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

const nsILDAPURL = Components.interfaces.nsILDAPURL;
const LDAPURLContractID = "@mozilla.org/network/ldap-url;1";
const nsILDAPSyncQuery = Components.interfaces.nsILDAPSyncQuery;
const LDAPSyncQueryContractID = "@mozilla.org/ldapsyncquery;1";
const nsIPrefService = Components.interfaces.nsIPrefService;
const PrefServiceContractID = "@mozilla.org/preferences-service;1";

// set on a platform specific basis in platform.js
platform = { value: "" };

var gVersion;

function getPrefBranch() {
    
    var prefService = Components.classes[PrefServiceContractID]
                                .getService(nsIPrefService);    
    return prefService.getBranch(null);
}

function pref(prefName, value) {

    try { 
        var prefBranch = getPrefBranch();

        if (typeof value == "string") {
            prefBranch.setCharPref(prefName, value);
        }
        else if (typeof value == "number") {
            prefBranch.setIntPref(prefName, value);
        }
        else if (typeof value == "boolean") {
            prefBranch.setBoolPref(prefName, value);
        }
    }
    catch(e) {
        displayError("pref", e);
    }
}

function defaultPref(prefName, value) {
    
    try {
        var prefService = Components.classes[PrefServiceContractID]
                                    .getService(nsIPrefService);        
        var prefBranch = prefService.getDefaultBranch(null);
        if (typeof value == "string") {
            prefBranch.setCharPref(prefName, value);
        }
        else if (typeof value == "number") {
            prefBranch.setIntPref(prefName, value);
        }
        else if (typeof value == "boolean") {
            prefBranch.setBoolPref(prefName, value);
        }
    }
    catch(e) {
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
    }
    catch(e) {
        displayError("lockPref", e);
    }
}

function unlockPref(prefName) {

    try {

        var prefBranch = getPrefBranch();
        prefBranch.unlockPref(prefName);
    }
    catch(e) {
        displayError("unlockPref", e);
    }
}

function getPref(prefName) {
    
    try {
        var prefBranch = getPrefBranch();
        
        switch (prefBranch.getPrefType(prefName)) {
            
        case prefBranch.PREF_STRING:
            return prefBranch.getCharPref(prefName);
            
        case prefBranch.PREF_INT:
            return prefBranch.getIntPref(prefName);
            
        case prefBranch.PREF_BOOL:
            return prefBranch.getBoolPref(prefName);
        default:
            return null;
        }
    }
    catch(e) {
        displayError("getPref", e);
    }
}

function clearPref(prefName) {

    try {
        var prefBranch = getPrefBranch();
            prefBranch.clearUserPref(prefName);
    }
    catch(e) {
    }
        
}

function setLDAPVersion(version) {
    gVersion = version;
}


function getLDAPAttributes(host, base, filter, attribs) {
    
    try {
        var url = Components.classes[LDAPURLContractID].createInstance(nsILDAPURL);
    
        url.spec = "ldap://" + host + "/" + base + "?" + attribs 
                   + "?sub?" +  filter;
        var ldapquery = Components.classes[LDAPSyncQueryContractID]
                                  .createInstance(nsILDAPSyncQuery);
        // default to LDAP v3
        if (!gVersion)
          gVersion = Components.interfaces.nsILDAPConnection.VERSION3
 	// user supplied method
        processLDAPValues(ldapquery.getQueryResults(url, gVersion));
    }
    catch(e) {
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
    }
    catch(e) {
        displayError("getLDAPValue", e);
    }
}

function displayError(funcname, message) {

    try {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                      .getService(Components.interfaces.nsIPromptService);
        var bundle = Components.classes["@mozilla.org/intl/stringbundle;1"]
                               .getService(Components.interfaces.nsIStringBundleService)
                               .createBundle("chrome://autoconfig/locale/autoconfig.properties");

         var title = bundle.GetStringFromName("autoConfigTitle");
         var msg = bundle.formatStringFromName("autoConfigMsg", [funcname], 1);
         promptService.alert(null, title, msg + " " + message);
    }
    catch(e) { }
}

function getenv(name) {
    try {
        var environment = Components.classes["@mozilla.org/process/environment;1"].
            getService(Components.interfaces.nsIEnvironment);
        return environment.get(name);
    }
    catch(e) {
        displayError("getEnvironment", e);
    }
}

