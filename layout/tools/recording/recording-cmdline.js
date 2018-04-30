/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const nsISupports                    = Ci.nsISupports;
  
const nsICommandLine                 = Ci.nsICommandLine;
const nsICommandLineHandler          = Ci.nsICommandLineHandler;
const nsISupportsString              = Ci.nsISupportsString;
const nsIWindowWatcher               = Ci.nsIWindowWatcher;

function RecordingCmdLineHandler() {}
RecordingCmdLineHandler.prototype =
{
    classID: Components.ID('{86FB70EC-90FF-45AD-A1C1-F77D3C1184E9}'),

    /* nsISupports */
    QueryInterface: ChromeUtils.generateQI([nsICommandLineHandler]),

    /* nsICommandLineHandler */
    handle : function handler_handle(cmdLine) {
        var args = { };
        args.wrappedJSObject = args;
        try {
            var uristr = cmdLine.handleFlagWithParam("recording", false);
            if (uristr == null)
                return;
            try {
                args.uri = cmdLine.resolveURI(uristr).spec;
            }
            catch (e) {
                return;
            }
        }
        catch (e) {
            cmdLine.handleFlag("recording", true);
        }

        /**
         * Manipulate preferences by adding to the *default* branch.  Adding
         * to the default branch means the changes we make won't get written
         * back to user preferences.
         *
         * We want to do this here rather than in reftest.js because it's
         * important to set the recording pref before the platform Init gets
         * called.
         */
        var prefs = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefService);
        var branch = prefs.getDefaultBranch("");

        try {
            var outputstr = cmdLine.handleFlagWithParam("recording-output", false);
            if (outputstr != null) {
                branch.setCharPref("gfx.2d.recordingfile", outputstr);
            }
        } catch (e) { }

        branch.setBoolPref("gfx.2d.recording", true);

        var wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                       .getService(nsIWindowWatcher);
        wwatch.openWindow(null, "chrome://recording/content/recording.xul", "_blank",
                          "chrome,dialog=no,all", args);
        cmdLine.preventDefault = true;
    },

    helpInfo : "  --recording <file> Record drawing for a given URL.\n" +
               "  --recording-output <file> Specify destination file for a drawing recording.\n"
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RecordingCmdLineHandler]);
