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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Srilatha Moturi <srilatha@netscape.com>
 *   Mark Banner <mark@standard8.demon.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* components defined in this file */

const NS_LDAPPREFSSERVICE_CONTRACTID =
    "@mozilla.org/ldapprefs-service;1";
const NS_LDAPPREFSSERVICE_CID =
    Components.ID("{37418e42-b5fc-442c-a599-4c8b3131205e}");

/* interfaces used in this file */
const nsISupports        = Components.interfaces.nsISupports;
const nsISupportsString  = Components.interfaces.nsISupportsString;
const nsIPrefBranch      = Components.interfaces.nsIPrefBranch;
const nsILDAPURL         = Components.interfaces.nsILDAPURL;
const nsILDAPPrefsService = Components.interfaces.nsILDAPPrefsService;
const kDefaultLDAPPort = 389;
const kDefaultSecureLDAPPort = 636;

/* pref branches used in this file */
const prefRoot = "ldap_2.servers";
const parent = "ldap_2.servers.";

/* nsLDAPPrefs service */
function nsLDAPPrefsService() {}

nsLDAPPrefsService.prototype.prefs_migrated = false;

nsLDAPPrefsService.prototype.QueryInterface =
function (iid) {

    if (iid.equals(nsISupports) ||
        iid.equals(nsILDAPPrefsService))
        return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
}

nsLDAPPrefsService.prototype.getServerList =
function (prefBranch, aCount) {
  var prefCount = {value:0};

  // get all the preferences with prefix ldap_2.servers
  var directoriesList = prefBranch.getChildList(prefRoot, prefCount);

  var childList = new Array();
  var count = 0;
  if (directoriesList) {
    directoriesList.sort();
    var prefixLen;
    // lastDirectory contains the last entry that is added to the
    // array childList.
    var lastDirectory = "";

    // only add toplevel prefnames to the list,
    // i.e. add ldap_2.servers.<server-name>
    // but not ldap_2.servers.<server-name>.foo
    for(var i=0; i<prefCount.value; i++) {
      // Assign the prefix ldap_2.servers.<server-name> to directoriesList
      prefixLen = directoriesList[i].indexOf(".", parent.length);
      if (prefixLen != -1) {
        directoriesList[i] = directoriesList[i].substr(0, prefixLen);
        if (directoriesList[i] != lastDirectory) {
          // add the entry to childList
          // only if it is not added yet
          lastDirectory = directoriesList[i];
          childList[count] = directoriesList[i];
          count++;
        }
      }
    }
  }

  if (!count)
  // no preferences with the prefix ldap_2.servers
    throw Components.results.NS_ERROR_FAILURE;

  aCount.value = count;
  return childList;
}

/* migrate 4.x ldap prefs to mozilla format.
   Converts hostname, basedn, port to uri (nsLDAPURL).
 */
nsLDAPPrefsService.prototype.migratePrefsIfNeeded =
function () {
  // Have we already migrated prefs during this instance?
  if (this.prefs_migrated) return;

  // Now check if the prefs have been migrated.
  var prefInt = null;
  var pref_string;
  var ldapUrl = null;
  var enable = false;
  var host;

  try {
    prefInt = Components.classes["@mozilla.org/preferences-service;1"];
    prefInt = prefInt.getService(Components.interfaces.nsIPrefBranch);
  }
  catch (ex) {
    dump("nsLDAPPrefsService: failed to get prefs service!\n");
    return;
  }

  var migrated = false;
  try{
    migrated = prefInt.getBoolPref("ldap_2.prefs_migrated");
  }
  catch(ex){}
  if (migrated){
    this.prefs_migrated = true;
    return;
  }

  var useDirectory = null;
  try{
    useDirectory = prefInt.getBoolPref("ldap_2.servers.useDirectory");
  }
  catch(ex) {}

  /* generate the list of directory servers from preferences */
  var prefCount = {value:0};
  var availDirectories = null;
  var arrayOfDirectories;

  try {
    arrayOfDirectories = this.getServerList(prefInt, prefCount);
  }
  catch (ex) {
    arrayOfDirectories = null;
  }
  if (arrayOfDirectories) {
    var position;
    var description;
    var dirType;
    var j = 0;

    availDirectories = new Array();
    for (var i = 0; i < prefCount.value; i++)
    {
      if ((arrayOfDirectories[i] != "ldap_2.servers.pab") &&
        (arrayOfDirectories[i] != "ldap_2.servers.history")) {
        try{
          position = prefInt.getIntPref(arrayOfDirectories[i]+".position");
        }
        catch(ex){
          position = 1;
        }
        try{
          dirType = prefInt.getIntPref(arrayOfDirectories[i]+".dirType");
        }
        catch(ex){
          dirType = 1;
        }
        if ((position != 0) && (dirType == 1)) {
          try{
            description = prefInt.getComplexValue(arrayOfDirectories[i]+".description",
                                                  Components.interfaces.nsISupportsString).data;
          }
          catch(ex){
            description = null;
          }
          if (description) {
            availDirectories[j] = new Array(2);
            availDirectories[j][0] = arrayOfDirectories[i];
            availDirectories[j][1] = description;
            j++;
          }
        }
      }
    }
  }

  for (var i = 0; i < availDirectories.length; i++) {
    pref_string = availDirectories[i][0];
    try{
      host = prefInt.getCharPref(pref_string + ".serverName");
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
        ldapUrl.dn = prefInt.getComplexValue(pref_string + ".searchBase",
                                             nsISupportsString).data;
      }
      catch (ex) {
      }
      var secure = false;
      try {
        secure = prefInt.getBoolPref(pref_string + ".isSecure");
      }
      catch(ex) {// if this preference does not exist its ok
      }
      var port;
      if (secure) {
        ldapUrl.options |= ldapurl.OPT_SECURE;
        port = kDefaultSecureLDAPPort;
      }
      else
        port = kDefaultLDAPPort;
      try {
        port = prefInt.getIntPref(pref_string + ".port");
      }
      catch(ex) {
	    // if this preference does not exist we will use default values.
      }
      ldapUrl.port = port;
      ldapUrl.scope = 2;

      var uri = Components.classes["@mozilla.org/supports-string;1"]
                      .createInstance(Components.interfaces.nsISupportsString);
      uri.data = ldapUrl.spec;
      prefInt.setComplexValue(pref_string + ".uri", Components.interfaces.nsISupportsString, uri);

      /* is this server selected for autocompletion?
         if yes, convert the preference to mozilla format.
         Atmost one server is selected for autocompletion.
       */
      if (useDirectory && !enable){
        try {
         enable = prefInt.getBoolPref(pref_string + ".autocomplete.enabled");
        }
        catch(ex) {}
        if (enable) {
          prefInt.setCharPref("ldap_2.servers.directoryServer", pref_string);
        }
      }
    }
  }
  try {
    prefInt.setBoolPref("ldap_2.prefs_migrated", true);
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

    if (!iid.equals(nsISupports) && !iid.equals(nsILDAPPrefsService))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new nsLDAPPrefsService();
}

var nsLDAPPrefsModule = new Object();
nsLDAPPrefsModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(NS_LDAPPREFSSERVICE_CID,
                                    "nsLDAPPrefs Service",
                                    NS_LDAPPREFSSERVICE_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);
}

nsLDAPPrefsModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    compMgr.unregisterFactoryLocation(NS_LDAPPREFSSERVICE_CID, fileSpec);
}

nsLDAPPrefsModule.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(nsILDAPPrefsService))
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
