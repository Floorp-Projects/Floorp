/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 * Srilatha Moturi <srilatha@netscape.com>
 */

/* components defined in this file */

const NS_LDAPPREFSSERVICE_CONTRACTID =
    "@mozilla.org/ldapprefs-service;1";
const NS_LDAPPREFSSERVICE_CID =
    Components.ID("{5a4911e0-44cd-11d5-9074-0010a4b26cda}");
const NS_LDAPPREFSSERVICE_IID = Components.interfaces.nsILDAPPrefsService;

/* interfaces used in this file */
const nsISupports        = Components.interfaces.nsISupports;
const nsIPrefBranch      = Components.interfaces.nsIPrefBranch;
const nsILDAPURL         = Components.interfaces.nsILDAPURL;
const nsILDAPService     = Components.interfaces.nsILDAPService;

/* nsLDAPPrefs service */
function nsLDAPPrefsService() {
  var arrayOfDirectories;
  var j = 0;
  try {
    gPrefInt = Components.classes["@mozilla.org/preferences-service;1"];
    gPrefInt = gPrefInt.getService(nsIPrefBranch);
  }
  catch (ex) {
    dump("failed to get prefs service!\n");
    return;
  }
  /* generate the list of directory servers from preferences */
  var prefCount = {value:0};
  try {
    arrayOfDirectories = gPrefInt.getChildList("ldap_2.servers", prefCount);
  }
  catch (ex) {
    arrayOfDirectories = null;
  }
  if (arrayOfDirectories) {
    this.availDirectories = new Array();
    var position;
    var description;
    for (var i = 0; i < prefCount.value; i++)
    {
      if ((arrayOfDirectories[i] != "ldap_2.servers.pab") && 
        (arrayOfDirectories[i] != "ldap_2.servers.history")) {
        try{
          position = gPrefInt.getIntPref(arrayOfDirectories[i]+".position");
        }
        catch(ex){
          position = 1;
        }
        try{
          dirType = gPrefInt.getIntPref(arrayOfDirectories[i]+".dirType");
        }
        catch(ex){
          dirType = 1;
        }
        if ((position != 0) && (dirType == 1)) {
          try{
            description = gPrefInt.getComplexValue(arrayOfDirectories[i]+".description",
                                                   Components.interfaces.nsISupportsWString);
          }
          catch(ex){
            description = null;
          }
          if (description) {
            this.availDirectories[j] = new Array(2);
            this.availDirectories[j][0] = arrayOfDirectories[i];
            this.availDirectories[j][1] = description;
            j++;
          }
        }
      }
    }
  }
  this.migrate();
}
nsLDAPPrefsService.prototype.prefs_migrated = false;
nsLDAPPrefsService.prototype.availDirectories = null;

nsLDAPPrefsService.prototype.QueryInterface =
function (iid) {

    if (!iid.equals(nsISupports) &&
        !iid.equals(NS_LDAPPREFSSERVICE_IID))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

/* migrate 4.x ldap prefs to mozilla format. 
   Converts hostname, basedn, port to uri (nsLDAPURL).    
 */
nsLDAPPrefsService.prototype.migrate = 
function () {
  var pref_string;
  var ldapUrl=null;
  var enable = false;
  if (this.prefs_migrated) return;
  var gPrefInt = null;
  var host;
  var dn;
  try {
    gPrefInt = Components.classes["@mozilla.org/preferences-service;1"];
    gPrefInt = gPrefInt.getService(Components.interfaces.nsIPrefBranch);
  }
  catch (ex) {
    dump("failed to get prefs service!\n");
    return;
  }
  var migrated = false;
  try{
    migrated = gPrefInt.getBoolPref("ldap_2.prefs_migrated");
  }
  catch(ex){}
  if (migrated){
    this.prefs_migrated = true;
    return;
  }
  try{
    var useDirectory = gPrefInt.getBoolPref("ldap_2.servers.useDirectory");
  }
  catch(ex) {}
  try {
    var ldapService = Components.classes[
        "@mozilla.org/network/ldap-service;1"].
        getService(Components.interfaces.nsILDAPService);
  }
  catch (ex)
  { 
    dump("failed to get ldap service!\n");
    ldapService = null;
  }
  for (var i=0; i < this.availDirectories.length; i++) {
    pref_string = this.availDirectories[i][0];
    try{
      host = gPrefInt.getCharPref(pref_string + ".serverName");
    }
    catch (ex) {
      host = null;
    }
    if (host) {
      try {
        ldapUrl = Components.classes["@mozilla.org/network/ldap-url;1"];
        ldapUrl = ldapUrl.createInstance().QueryInterface(nsILDAPURL);
      }
      catch (ex) {
        dump("failed to get ldap url!\n");
        return;
      }
      ldapUrl.host = host;
      try{
        dn = gPrefInt.getComplexValue(pref_string + ".searchBase",
                                      Components.interfaces.nsISupportsWString);
      }
      catch (ex) {
        dn = null;
      }
      if (dn && ldapService)
        ldapUrl.dn = ldapService.UCS2toUTF8(dn);
      try {
        var port = gPrefInt.getIntPref(pref_string + ".port");
      }
      catch(ex) {
        port = 389;
      }
      ldapUrl.port = port;
      ldapUrl.scope = 2;
      gPrefInt.setCharPref(pref_string + ".uri", ldapUrl.spec);
      /* is this server selected for autocompletion? 
         if yes, convert the preference to mozilla format.
         Atmost one server is selected for autocompletion. 
       */
      if (useDirectory && !enable){
        try {
         enable = gPrefInt.getBoolPref(pref_string + ".autocomplete.enabled");
        } 
        catch(ex) {}
        if (enable) {
          gPrefInt.setCharPref("ldap_2.servers.directoryServer", pref_string);
        }
      }
    }
  }
  try {
    gPrefInt.setBoolPref("ldap_2.prefs_migrated", true);
    var svc = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefService);
    svc.savePrefFile(null);
  }
  catch (ex) {dump ("ERROR:" + ex + "\n");}
    
  this.prefs_migrated = true;
}

/* factory for nsLDAPPrefs service (nsLDAPPrefsService) */

var nsLDAPPrefsFactory = new Object();

nsLDAPPrefsFactory.createInstance =

function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new nsLDAPPrefsService();
}

var nsLDAPPrefsModule = new Object();
nsLDAPPrefsModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    compMgr.registerComponentWithType(NS_LDAPPREFSSERVICE_CID,
                                      "nsLDAPPrefs Service",
                                      NS_LDAPPREFSSERVICE_CONTRACTID, fileSpec,
                                      location, true, true, type);
}

nsLDAPPrefsModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
    compMgr.unregisterComponentSpec(NS_LDAPPREFSSERVICE_CID, fileSpec);
}

nsLDAPPrefsModule.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(NS_LDAPPREFSSERVICE_CID))
        return nsLDAPPrefsFactory;
    throw Components.results.NS_ERROR_NO_INTERFACE;  
}

nsLDAPPrefsModule.canUnload =
function(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return nsLDAPPrefsModule;
}
