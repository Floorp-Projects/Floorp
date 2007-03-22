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
 * The Original Code is DOM Inspector.
 *
 * The Initial Developer of the Original Code is
 * Christopher A. Aillon <christopher@aillon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
 *   L. David Baron, Mozilla Corporation <dbaron@dbaron.org> (modified for reftest)
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

// NOTE: this file implements both the seamonkey nsICmdLineHandler and
// the toolkit nsICommandLineHandler, using runtime detection.

const REFTEST_CMDLINE_CONTRACTID     = "@mozilla.org/commandlinehandler/general-startup;1?type=reftest";
const REFTEST_CMDLINE_CLSID          = Components.ID('{32530271-8c1b-4b7d-a812-218e42c6bb23}');
const CATMAN_CONTRACTID              = "@mozilla.org/categorymanager;1";
const nsISupports                    = Components.interfaces.nsISupports;
  
const nsICategoryManager             = Components.interfaces.nsICategoryManager;
const nsICmdLineHandler              = Components.interfaces.nsICmdLineHandler;
const nsICommandLine                 = Components.interfaces.nsICommandLine;
const nsICommandLineHandler          = Components.interfaces.nsICommandLineHandler;
const nsIComponentRegistrar          = Components.interfaces.nsIComponentRegistrar;
const nsISupportsString              = Components.interfaces.nsISupportsString;
const nsIWindowWatcher               = Components.interfaces.nsIWindowWatcher;

function LayoutATestCmdLineHandler() {}
LayoutATestCmdLineHandler.prototype =
{
  /* nsISupports */
  QueryInterface : function handler_QI(iid) {
    if (iid.equals(nsISupports))
      return this;

    if (nsICmdLineHandler && iid.equals(nsICmdLineHandler))
      return this;

    if (nsICommandLineHandler && iid.equals(nsICommandLineHandler))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsICmdLineHandler */
  commandLineArgument : "-reftest",
  prefNameForStartup : "general.startup.reftest",
  chromeUrlForTask : "chrome://reftest/content/reftest.xul",
  helpText : "Run layout acceptance tests on given manifest.",
  handlesArgs : true,
  defaultArgs : "",
  openWindowWithArgs : true,

  /* nsICommandLineHandler */
  handle : function handler_handle(cmdLine) {
    var args = Components.classes["@mozilla.org/supports-string;1"]
                         .createInstance(nsISupportsString);
    try {
      var uristr = cmdLine.handleFlagWithParam("reftest", false);
      if (uristr == null)
        return;
      try {
        args.data = cmdLine.resolveURI(uristr).spec;
      }
      catch (e) {
        return;
      }
    }
    catch (e) {
      cmdLine.handleFlag("reftest", true);
    }

    var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                           .getService(nsIWindowWatcher);
    wwatch.openWindow(null, "chrome://reftest/content/reftest.xul", "_blank",
                      "chrome,dialog=no,all", args);
    cmdLine.preventDefault = true;
  },

  helpInfo : "  -reftest <file>    Run layout acceptance tests on given manifest.\n"
};


var LayoutATestCmdLineFactory =
{
  createInstance : function(outer, iid)
  {
    if (outer != null) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }

    return new LayoutATestCmdLineHandler().QueryInterface(iid);
  }
};


var LayoutATestCmdLineModule =
{
  registerSelf : function(compMgr, fileSpec, location, type)
  {
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.registerFactoryLocation(REFTEST_CMDLINE_CLSID,
                                    "LayoutATest CommandLine Service",
                                    REFTEST_CMDLINE_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    var catman = Components.classes[CATMAN_CONTRACTID].getService(nsICategoryManager);
    catman.addCategoryEntry("command-line-argument-handlers",
                            "reftest command line handler",
                            REFTEST_CMDLINE_CONTRACTID, true, true);
    catman.addCategoryEntry("command-line-handler",
                            "m-reftest",
                            REFTEST_CMDLINE_CONTRACTID, true, true);
  },

  unregisterSelf : function(compMgr, fileSpec, location)
  {
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(REFTEST_CMDLINE_CLSID, fileSpec);
    catman = Components.classes[CATMAN_CONTRACTID].getService(nsICategoryManager);
    catman.deleteCategoryEntry("command-line-argument-handlers",
                               "reftest command line handler", true);
    catman.deleteCategoryEntry("command-line-handler",
                               "m-reftest", true);
  },

  getClassObject : function(compMgr, cid, iid)
  {
    if (cid.equals(REFTEST_CMDLINE_CLSID)) {
      return LayoutATestCmdLineFactory;
    }

    if (!iid.equals(Components.interfaces.nsIFactory)) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  canUnload : function(compMgr)
  {
    return true;
  }
};


function NSGetModule(compMgr, fileSpec) {
  return LayoutATestCmdLineModule;
}
