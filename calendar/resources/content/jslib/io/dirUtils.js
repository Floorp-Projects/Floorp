/*** -*- Mode: Javascript; tab-width: 2;

The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Collabnet code.
The Initial Developer of the Original Code is Collabnet.

Portions created by Collabnet are Copyright (C) 2000 Collabnet.
All Rights Reserved.

Contributor(s): Pete Collins, Doug Turner, Brendan Eich, Warren Harris,
                Eric Plaster, Martin Kutschker


JS Directory Class API

  dirUtils.js

Function List


Instructions:


*/

if(typeof(JS_LIB_LOADED)=='boolean') {

const JS_DIRUTILS_FILE              = "dirUtils.js";
const JS_DIRUTILS_LOADED            = true;

const JS_DIRUTILS_FILE_LOCAL_CID    = "@mozilla.org/file/local;1";
const JS_DIRUTILS_FILE_DIR_CID      = "@mozilla.org/file/directory_service;1";

const JS_DIRUTILS_FILE_I_LOCAL_FILE = "nsILocalFile";
const JS_DIRUTILS_INIT_W_PATH       = "initWithPath";
const JS_DIRUTILS_I_PROPS           = "nsIProperties";
const JS_DIRUTILS_NSIFILE           = C.interfaces.nsIFile;

const NS_APP_PREFS_50_DIR           = "PrefD"; // /root/.mozilla/Default User/k1m30xaf.slt
const NS_APP_CHROME_DIR             = "AChrom"; // /usr/src/mozilla/dist/bin/chrome
const NS_APP_USER_PROFILES_ROOT_DIR = "DefProfRt";  // /root/.mozilla
const NS_APP_USER_PROFILE_50_DIR    = "ProfD";      // /root/.mozilla/Default User/k1m30xaf.slt

const NS_APP_APPLICATION_REGISTRY_DIR  = "AppRegD"; // /root/.mozilla
const NS_APP_APPLICATION_REGISTRY_FILE = "AppRegF"; // /root/.mozilla/appreg
const NS_APP_DEFAULTS_50_DIR           = "DefRt";   // /usr/src/mozilla/dist/bin/defaults 
const NS_APP_PREF_DEFAULTS_50_DIR      = "PrfDef";  // /usr/src/mozilla/dist/bin/defaults/pref
const NS_APP_PROFILE_DEFAULTS_50_DIR   = "profDef"; // /usr/src/mozilla/dist/bin/defaults/profile/US
const NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR = "ProfDefNoLoc"; // /usr/src/mozilla/dist/bin/defaults/profile 
const NS_APP_RES_DIR                      = "ARes"; // /usr/src/mozilla/dist/bin/res
const NS_APP_PLUGINS_DIR                  = "APlugns"; // /usr/src/mozilla/dist/bin/plugins
const NS_APP_SEARCH_DIR                   = "SrchPlugns"; // /usr/src/mozilla/dist/bin/searchplugins
const NS_APP_PREFS_50_FILE                = "PrefF"; // /root/.mozilla/Default User/k1m30xaf.slt/prefs.js
const NS_APP_USER_CHROME_DIR              = "UChrm"; // /root/.mozilla/Default User/k1m30xaf.slt/chrome
const NS_APP_LOCALSTORE_50_FILE           = "LclSt"; // /root/.mozilla/Default User/k1m30xaf.slt/localstore.rdf
const NS_APP_HISTORY_50_FILE              = "UHist"; // /root/.mozilla/Default User/k1m30xaf.slt/history.dat
const NS_APP_USER_PANELS_50_FILE          = "UPnls"; // /root/.mozilla/Default User/k1m30xaf.slt/panels.rdf
const NS_APP_USER_MIMETYPES_50_FILE       = "UMimTyp"; // /root/.mozilla/Default User/k1m30xaf.slt/mimeTypes.rdf
const NS_APP_BOOKMARKS_50_FILE            = "BMarks"; // /root/.mozilla/Default User/k1m30xaf.slt/bookmarks.html 
const NS_APP_SEARCH_50_FILE               = "SrchF"; // /root/.mozilla/Default User/k1m30xaf.slt/search.rdf
const NS_APP_MAIL_50_DIR                  = "MailD"; // /root/.mozilla/Default User/k1m30xaf.slt/Mail
const NS_APP_IMAP_MAIL_50_DIR             = "IMapMD"; // /root/.mozilla/Default User/k1m30xaf.slt/ImapMail
const NS_APP_NEWS_50_DIR                  = "NewsD"; // /root/.mozilla/Default User/k1m30xaf.slt/News
const NS_APP_MESSENGER_FOLDER_CACHE_50_DIR = "MFCaD"; // /root/.mozilla/Default User/k1m30xaf.slt/panacea.dat

const JS_DIRUTILS_FilePath  = new C.Constructor(JS_DIRUTILS_FILE_LOCAL_CID,
                                                JS_DIRUTILS_FILE_I_LOCAL_FILE,
                                                JS_DIRUTILS_INIT_W_PATH);

const JS_DIRUTILS_DIR       = new C.Constructor(JS_DIRUTILS_FILE_DIR_CID,
                                                JS_DIRUTILS_I_PROPS);

// constructor
function DirUtils(){}

DirUtils.prototype = {

getPath   : function (aAppID) {

  if(!aAppID) {
    jslibError(null, "(no arg defined)", "NS_ERROR_INVALID_ARG", JS_FILE_FILE+":getPath");
    return null;
  }

  var rv;

  try { 
    rv=(new JS_DIRUTILS_DIR()).get(aAppID, JS_DIRUTILS_NSIFILE).path; 
  } catch (e) {
    jslibError(e, "(unexpected error)", "NS_ERROR_FAILURE", JS_DIRUTILS_FILE+":getPath");
    rv=null;
  }

  return rv;
},

getPrefsDir : function () { return this.getPath(NS_APP_PREFS_50_DIR); },
getChromeDir : function () { return this.getPath(NS_APP_CHROME_DIR); },
getMozHomeDir : function () { return this.getPath(NS_APP_USER_PROFILES_ROOT_DIR); },
getMozUserHomeDir : function () { return this.getPath(NS_APP_USER_PROFILE_50_DIR); },
getAppRegDir : function () { return this.getPath(NS_APP_APPLICATION_REGISTRY_FILE); },
getAppDefaultDir : function () { return this.getPath(NS_APP_DEFAULTS_50_DIR); },
getAppDefaultPrefDir : function () { return this.getPath(NS_APP_PREF_DEFAULTS_50_DIR); },
getProfileDefaultsLocDir : function () { return this.getPath(NS_APP_PROFILE_DEFAULTS_50_DIR); },
getProfileDefaultsDir : function () { return this.getPath(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR); },
getAppResDir : function () { return this.getPath(NS_APP_RES_DIR); },
getAppPluginsDir : function () { return this.getPath(NS_APP_PLUGINS_DIR); },
getSearchPluginsDir : function () { return this.getPath(NS_APP_SEARCH_DIR); },
getPrefsFile : function () { return this.getPath(NS_APP_PREFS_50_FILE); },
getUserChromeDir : function () { return this.getPath(NS_APP_USER_CHROME_DIR); },
getLocalStore : function () { return this.getPath(NS_APP_LOCALSTORE_50_FILE); },
getHistoryFile : function () { return this.getPath(NS_APP_HISTORY_50_FILE); },
getPanelsFile : function () { return this.getPath(NS_APP_USER_PANELS_50_FILE); },
getMimeTypes : function () { return this.getPath(NS_APP_USER_MIMETYPES_50_FILE); },
getBookmarks : function () { return this.getPath(NS_APP_BOOKMARKS_50_FILE); },
getSearchFile : function () { return this.getPath(NS_APP_SEARCH_50_FILE); },
getUserMailDir : function () { return this.getPath(NS_APP_MAIL_50_DIR); },
getUserImapDir : function () { return this.getPath(NS_APP_IMAP_MAIL_50_DIR); },
getUserNewsDir : function () { return this.getPath(NS_APP_NEWS_50_DIR); },
getMessengerFolderCache : function () { return this.getPath(NS_APP_MESSENGER_FOLDER_CACHE_50_DIR); },

get help() {
  const help =

    "\n\nFunction and Attribute List:\n"    +
    "\n"                                    +
    "    getPrefsDir()\n"                       +
    "    getChromeDir()\n"                      +
    "    getMozHomeDir()\n"                     +
    "    getMozUserHomeDir()\n"                 +
    "    getAppRegDir()\n"                      +
    "    getAppDefaultDir()\n"                  +
    "    getAppDefaultPrefDir()\n"              +
    "    getProfileDefaultsLocDir()\n"          +
    "    getProfileDefaultsDir()\n"             +
    "    getAppResDir()\n"                      +
    "    getAppPluginsDir()\n"                  +
    "    getSearchPluginsDir()\n"               +
    "    getPrefsFile()\n"                      +
    "    getUserChromeDir()\n"                  +
    "    getLocalStore()\n"                     +
    "    getHistoryFile()\n"                    +
    "    getPanelsFile()\n"                     +
    "    getMimeTypes()\n"                      +
    "    getBookmarks()\n"                      +
    "    getSearchFile()\n"                     +
    "    getUserMailDir()\n"                    +
    "    getUserImapDir()\n"                    +
    "    getUserNewsDir()\n"                    +
    "    getMessengerFolderCache()\n\n";

  return help;
}

}; //END CLASS 

jslibDebug('*** load: '+JS_DIRUTILS_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

else {
    dump("JSLIB library not loaded:\n"                                  +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include(jslib_dirutils);\n\n");
}

