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
 * The Original Code is Mozilla Default Browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law  <law@netscape.com>
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

/* This file implements the nsICommandLineHandler interface.
 *
 * This component handles the startup command line argument of the form:
 *   -setDefaultBrowser
 * by making the current executable the "default browser."
 */

function nsSetDefaultBrowser() {
}

nsSetDefaultBrowser.prototype = {
  /* nsISupports */
  QueryInterface: function nsSetDefault_QI(iid) {
    if (!iid.equals(Components.interfaces.nsICommandLineHandler) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
  },

  /* nsICommandLineHandler */
  handle : function nsSetDefault_handle(cmdline) {
    if (cmdline.handleFlag("setDefaultBrowser", false)) {
      var shell = Components.classes["@mozilla.org/browser/shell-service;1"]
                            .getService(Components.interfaces.nsIShellService);
      shell.setDefaultBrowser(true, true);
    }
  },

  helpText : "  -setDefaultBrowser   Set this app as the default browser.\n"
}

// This Component's module and factory implementation.

const contractID = "@mozilla.org/browser/default-browser-clh;1";
const CID = Components.ID("{F57899D0-4E2C-4ac6-9E29-50C736103B0C}");

var ModuleAndFactory = {
  /* nsISupports */
  QueryInterface: function nsSetDefault_QI(iid) {
    if (!iid.equals(Components.interfaces.nsIModule) &&
        !iid.equals(Components.interfaces.nsIFactory) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
  },

  /* nsIModule */
  getClassObject: function (compMgr, cid, iid) {
    if (!cid.equals(CID))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    
    return this.QueryInterface(iid);
  },
    
  registerSelf: function mod_regself(compMgr, fileSpec, location, type) {
    var compReg =
      compMgr.QueryInterface( Components.interfaces.nsIComponentRegistrar );

    compReg.registerFactoryLocation( CID,
                                     "Default Browser Cmdline Handler",
                                     contractID,
                                     fileSpec,
                                     location,
                                     type );

    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);

    catMan.addCategoryEntry("command-line-handler",
                            "m-setdefaultbrowser",
                            contractID, true, true);
  },
    
  unregisterSelf : function mod_unregself(compMgr, location, type) {
    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);

    catMan.deleteCategoryEntry("command-line-handler",
                               "m-setdefaultbrowser", true);
  },

  canUnload: function(compMgr) {
    return true;
  },

  /* nsIFactory */
  createInstance: function mod_CI(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
  
    return new nsSetDefaultBrowser().QueryInterface(iid);
  },
    
  lockFactory : function mod_lock(lock) {
    /* no-op */
  }
}

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
  return ModuleAndFactory;
}
