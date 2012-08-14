/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = [ "CmdCommands" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let prefSvc = "@mozilla.org/preferences-service;1";
XPCOMUtils.defineLazyGetter(this, "prefBranch", function() {
  let prefService = Cc[prefSvc].getService(Ci.nsIPrefService);
  return prefService.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);
});

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource:///modules/devtools/Console.jsm");

/**
 * A place to store the names of the commands that we have added as a result of
 * calling refreshAutoCommands(). Used by refreshAutoCommands to remove the
 * added commands.
 */
let commands = [];

/**
 * Exported API
 */
let CmdCommands = {
  /**
   * Called to look in a directory pointed at by the devtools.commands.dir pref
   * for *.mozcmd files which are then loaded.
   * @param nsIPrincipal aSandboxPrincipal Scope object for the Sandbox in which
   * we eval the script from the .mozcmd file. This should be a chrome window.
   */
  refreshAutoCommands: function GC_refreshAutoCommands(aSandboxPrincipal) {
    // First get rid of the last set of commands
    commands.forEach(function(name) {
      gcli.removeCommand(name);
    });

    let dirName = prefBranch.getComplexValue("devtools.commands.dir",
                                             Ci.nsISupportsString).data;
    if (dirName == "") {
      return;
    }

    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    dir.initWithPath(dirName);
    if (!dir.exists() || !dir.isDirectory()) {
      throw new Error('\'' + dirName + '\' is not a directory.');
    }

    let en = dir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);

    while (true) {
      let file = en.nextFile;
      if (!file) {
        break;
      }
      if (file.leafName.match(/.*\.mozcmd$/) && file.isFile() && file.isReadable()) {
        loadCommandFile(file, aSandboxPrincipal);
      }
    }
  },
};

/**
 * Load the commands from a single file
 * @param nsIFile aFile The file containing the commands that we should read
 * @param nsIPrincipal aSandboxPrincipal Scope object for the Sandbox in which
 * we eval the script from the .mozcmd file. This should be a chrome window.
 */
function loadCommandFile(aFile, aSandboxPrincipal) {
  NetUtil.asyncFetch(aFile, function refresh_fetch(aStream, aStatus) {
    if (!Components.isSuccessCode(aStatus)) {
      console.error("NetUtil.asyncFetch(" + aFile.path + ",..) failed. Status=" + aStatus);
      return;
    }

    let source = NetUtil.readInputStreamToString(aStream, aStream.available());
    aStream.close();

    let sandbox = new Cu.Sandbox(aSandboxPrincipal, {
      sandboxPrototype: aSandboxPrincipal,
      wantXrays: false,
      sandboxName: aFile.path
    });
    let data = Cu.evalInSandbox(source, sandbox, "1.8", aFile.leafName, 1);

    if (!Array.isArray(data)) {
      console.error("Command file '" + aFile.leafName + "' does not have top level array.");
      return;
    }

    data.forEach(function(commandSpec) {
      gcli.addCommand(commandSpec);
      commands.push(commandSpec.name);
    });
  }.bind(this));
}

/**
 * 'cmd' command
 */
gcli.addCommand({
  name: "cmd",
  description: gcli.lookup("cmdDesc"),
  hidden: true
});

/**
 * 'cmd refresh' command
 */
gcli.addCommand({
  name: "cmd refresh",
  description: gcli.lookup("cmdRefreshDesc"),
  hidden: true,
  exec: function Command_cmdRefresh(args, context) {
    GcliCmdCommands.refreshAutoCommands(context.environment.chromeDocument.defaultView);
  }
});
