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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

const NS_IACTIVEXSECURITYPOLICY_CONTRACTID =
    "@mozilla.org/nsactivexsecuritypolicy;1";
const NS_IACTIVEXSECURITYPOLICY_CID =
    Components.ID("{B9BDB43B-109E-44b8-858C-B68C6C3DF86F}");

const nsISupports               = Components.interfaces.nsISupports;
const nsIObserver               = Components.interfaces.nsIObserver;
const nsIActiveXSecurityPolicy  = Components.interfaces.nsIActiveXSecurityPolicy;

///////////////////////////////////////////////////////////////////////////////
// Constants representing some default flag combinations of varying degrees
// of safety.

// No controls at all
const kTotalSecurityHostingFlags =
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_HOST_NOTHING;

// Host only safe controls, no downloading or scripting
const kHighSecurityHostingFlags =
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_HOST_SAFE_OBJECTS;

// Host and script safe controls and allow downloads
const kMediumSecurityGlobalHostingFlags =
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_HOST_SAFE_OBJECTS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_DOWNLOAD_CONTROLS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_SCRIPT_SAFE_OBJECTS;

// Host any control and script safe controls
const kLowSecurityHostFlags =
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_HOST_SAFE_OBJECTS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_DOWNLOAD_CONTROLS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_SCRIPT_SAFE_OBJECTS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_HOST_ALL_OBJECTS;

// Goodbye cruel world
const kNoSecurityHostingFlags =
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_HOST_SAFE_OBJECTS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_DOWNLOAD_CONTROLS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_SCRIPT_SAFE_OBJECTS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_SCRIPT_ALL_OBJECTS |
    nsIActiveXSecurityPolicy.HOSTING_FLAGS_HOST_ALL_OBJECTS;

///////////////////////////////////////////////////////////////////////////////
// Constants representing the default hosting flag values when there is
// no pref value. Note that these should be as tight as possible except for
// testing purposes.

const kDefaultGlobalHostingFlags = kMediumSecurityGlobalHostingFlags;
const kDefaultOtherHostingFlags  = kMediumSecurityGlobalHostingFlags;

// Preferences security policy reads from
const kHostingPrefPart1 = "security.xpconnect.activex.";
const kHostingPrefPart2 = ".hosting_flags";
const kGlobalHostingFlagsPref = kHostingPrefPart1 + "global" + kHostingPrefPart2;

var gPref = null;

function addPrefListener(observer, prefStr)
{
    try {
        if (gPref == null) {
            var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                        .getService(Components.interfaces.nsIPrefService);
            gPref = prefService.getBranch(null);
        }
        var pbi = gPref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
        pbi.addObserver(prefStr, observer, false);
    } catch(ex) {
        dump("Failed to observe prefs: " + ex + "\n");
    }
}

function AxSecurityPolicy()
{
    addPrefListener(this, kGlobalHostingFlagsPref);
    this.syncPrefs();
    this.globalHostingFlags = kDefaultGlobalHostingFlags;
}

AxSecurityPolicy.prototype = {
    syncPrefs: function()
    {
        var hostingFlags = this.globalHostingFlags;
        try {
            if (gPref == null) {
                var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                            .getService(Components.interfaces.nsIPrefService);
                gPref = prefService.getBranch(null);
            }
            hostingFlags = gPref.getIntPref(kGlobalHostingFlagsPref);
        }
        catch (ex) {
            dump("Failed to read control hosting flags from \"" + kGlobalHostingFlagsPref + "\"\n");
            hostingFlags = kDefaultGlobalHostingFlags;
        }
        if (hostingFlags != this.globalHostingFlags)
        {
            dump("Global activex hosting flags have changed value to " + hostingFlags + "\n");
            this.globalHostingFlags = hostingFlags
        }
    },

    // nsIActiveXSecurityPolicy
    getHostingFlags: function(context)
    {
        var hostingFlags;
        if (context == null || context == "global") {
            hostingFlags = this.globalHostingFlags;
        }
        else {
            try {
                var prefName = kHostingPrefPart1 + context + kHostingPrefPart2;
                hostingFlags = gPref.getIntPref(prefName);
            }
            catch (ex) {
                dump("Failed to read control hosting prefs for \"" + context + "\" from \"" + prefName + "\" pref\n");
                hostingFlags = kDefaultOtherHostingFlags;
            }
            hostingFlags = kDefaultOtherHostingFlags;
        }
        return hostingFlags;
    },
    // nsIObserver
    observe: function(subject, topic, prefName)
    {
        if (topic != "nsPref:changed")
            return;
        this.syncPrefs();

    },
    // nsISupports
    QueryInterface: function(iid) {
        if (iid.equals(nsISupports) ||
            iid.equals(nsIActiveXSecurityPolicy) ||
            iid.equals(nsIObserver))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    }
};

/* factory for AxSecurityPolicy */
var AxSecurityPolicyFactory = {
    createInstance: function (outer, iid)
    {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
        if (!iid.equals(nsISupports) &&
            !iid.equals(nsIActiveXSecurityPolicy) &&
            !iid.equals(nsIObserver))
            throw Components.results.NS_ERROR_INVALID_ARG;
        return new AxSecurityPolicy();
    }
};


var AxSecurityPolicyModule = {
    registerSelf: function (compMgr, fileSpec, location, type)
    {
        debug("*** Registering axsecurity policy.\n");
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(
            NS_IACTIVEXSECURITYPOLICY_CID ,
            "ActiveX Security Policy Service",
            NS_IACTIVEXSECURITYPOLICY_CONTRACTID,
            fileSpec,
            location,
            type);
    },
    unregisterSelf: function(compMgr, fileSpec, location)
    {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.unregisterFactoryLocation(NS_IACTIVEXSECURITYPOLICY_CID, fileSpec);
    },
    getClassObject: function(compMgr, cid, iid)
    {
        if (cid.equals(NS_IACTIVEXSECURITYPOLICY_CID))
            return AxSecurityPolicyFactory;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },
    canUnload: function(compMgr)
    {
        return true;
    }
};

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return AxSecurityPolicyModule;
}


