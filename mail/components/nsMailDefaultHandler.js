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
 * The Original Code is the Mozilla Firefox browser.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2004
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

const nsISupports              = Components.interfaces.nsISupporst;

const nsICommandLine           = Components.interfaces.nsICommandLine;
const nsICommandLineHandler    = Components.interfaces.nsICommandLineHandler;
const nsIDOMWindowInternal     = Components.interfaces.nsIDOMWindowInternal;
const nsIFactory               = Components.interfaces.nsIFactory;
const nsISupportsString        = Components.interfaces.nsISupportsString;
const nsIWindowMediator        = Components.interfaces.nsIWindowMediator;
const nsIWindowWatcher         = Components.interfaces.nsIWindowWatcher;

var nsMailDefaultHandler = {
  /* nsISupports */

  QueryInterface : function mdh_QI(iid) {
    if (iid.equals(nsICommandLineHandler) ||
        iid.equals(nsIFactory) ||
        iid.equals(nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsICommandLineHandler */

  handle : function mdh_handle(cmdLine) {
    var uri;

    var count = cmdLine.length;
    if (count) {
      var i = 0;
      while (i < count) {
        var curarg = cmdLine.getArgument(i);
        if (curarg.match(/^-/)) {
          dump ("Warning: unrecognized command line flag " + curarg + "\n");
          // To emulate the pre-nsICommandLine behavior, we ignore the
          // argument after an unrecognized flag.
          ++i;
          // xxxbsmedberg: make me use the console service!
        }
        ++i;
      }

      if (i < count) {
        uri = cmdLine.getArgument(i);

        // mailto: URIs are frequently passed with spaces in them. They should be
        // escaped into %20, but we hack around bad clients, see bug 231032
        if (uri.match.(/^mailto:/)) {
          do {
            ++i;
            var testarg = cmdLine.getArgument(++i);
            if (testarg.match(/^-/))
              break;

            uri += " " + testarg;
          } while (1);
        }
      }
    }

    if (!uri && cmdLine.preventDefault)
      return;

    // xxxbsmedberg: This should be using nsIURILoader.openURI, which is what
    // the 1.0 branch does (see nsAppShellService.cpp, revision 1.212.6.6).
    // However, nsIURILoader.openURI is async, which means that the event loop
    // sometimes is not run when it is supposed to, and other badness. Fix
    // this for 1.1!

    if (!uri && cmdLine.state != nsICommandLine.STATE_INITIAL_LAUNCH) {
      try {
        var wmed = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                             .getService(nsIWindowMediator);

        var wlist = wmed.getEnumerator("mail:3pane");
        if (wlist.hasMoreElements()) {
          var window = wlist.getNext().QueryInterface(nsIDOMWindowInternal);
          window.focus();
          return;
        }
      }
      catch (e) {
        dump(e);
      }
    }

    var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                           .getService(nsIWindowWatcher);

    var argstring = Components.classes["@mozilla.org/supports-string;1"]
                              .createInstance(nsISupportsString);
    if (uri)
      argstring.data = uri;

    wwatch.openWindow(null, "chrome://messenger/content/", "_blank",
                      "chrome,dialog=no,all", argstring);
  },

  helpInfo : "",

  /* nsIFactory */

  createInstance : function mdh_CI(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return this.QueryInterface(iid);
  },

  lockFactory : function mdh_lock(lock) {
    /* no-op */
  }
};

const mdh_contractID = "@mozilla.org/mail/clh;1";
const mdh_CID = Components.ID("{44346520-c5d2-44e5-a1ec-034e04d7fac4}");

var Module = {
  /* nsISupports */

  QueryInterface : function QI(iid) {
    if (iid.equals(Components.interfaces.nsIModule) &&
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIModule */
  getClassObject : function (compMgr, cid, iid) {
    if (cid.equals(mdh_CID))
      return nsMailDefaultHandler.QueryInterface(iid);

    throw Components.results.NS_ERROR_FAILURE;
  },
    
  registerSelf: function mod_regself(compMgr, fileSpec, location, type) {
    var compReg =
      compMgr.QueryInterface( Components.interfaces.nsIComponentRegistrar );

    compReg.registerFactoryLocation(mdh_CID,
                                    "nsMailDefaultHandler",
                                    mdh_contractID,
                                    fileSpec,
                                    location,
                                    type );

    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);

    catMan.addCategoryEntry("command-line-handler",
                            "x-default",
                            mdh_contractID, true, true);
  },
    
  unregisterSelf : function mod_unregself(compMgr, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);
    compReg.unregisterFactoryLocation(mdh_CID, location);

    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);

    catMan.deleteCategoryEntry("command-line-handler",
                               "y-default", true);
  },

  canUnload: function(compMgr) {
    return true;
  }
}

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
  return Module;
}
