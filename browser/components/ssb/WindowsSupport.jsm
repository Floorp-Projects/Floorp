/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["WindowsSupport"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SiteSpecificBrowserService } = ChromeUtils.import(
  "resource:///modules/SiteSpecificBrowserService.jsm"
);

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

const shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
  Ci.nsIWindowsShellService
);

const File = Components.Constructor(
  "@mozilla.org/file/local;1",
  Ci.nsIFile,
  "initWithPath"
);

const WindowsSupport = {
  /**
   * Installs an SSB by creating a shortcut to launch it on the user's desktop.
   *
   * @param {SiteSpecificBrowser} ssb the SSB to install.
   */
  async install(ssb) {
    if (!SiteSpecificBrowserService.useOSIntegration) {
      return;
    }

    let desktop = Services.dirsvc.get("Desk", Ci.nsIFile);
    let link = OS.Path.join(desktop.path, `${ssb.name}.lnk`);

    shellService.createShortcut(
      Services.dirsvc.get("XREExeF", Ci.nsIFile),
      ["-profile", OS.Constants.Path.profileDir, "-start-ssb", ssb.id],
      ssb.name,
      new File(link)
    );
  },

  /**
   * Uninstalls an SSB by deleting its shortcut from the user's desktop.
   *
   * @param {SiteSpecificBrowser} ssb the SSB to uninstall.
   */
  async uninstall(ssb) {
    if (!SiteSpecificBrowserService.useOSIntegration) {
      return;
    }

    let desktop = Services.dirsvc.get("Desk", Ci.nsIFile);
    let link = OS.Path.join(desktop.path, `${ssb.name}.lnk`);

    try {
      await OS.File.remove(link);
    } catch (e) {
      console.error(e);
    }
  },
};
