/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["WindowsSupport"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { SiteSpecificBrowserService } = ChromeUtils.import(
  "resource:///modules/SiteSpecificBrowserService.jsm"
);

const { SiteSpecificBrowserIdUtils } = ChromeUtils.import(
  "resource:///modules/SiteSpecificBrowserIdUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ImageTools: "resource:///modules/ssb/ImageTools.jsm",
});

let shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
  Ci.nsIWindowsShellService
);

const uiUtils = Cc["@mozilla.org/windows-ui-utils;1"].getService(
  Ci.nsIWindowsUIUtils
);

const taskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
  Ci.nsIWinTaskbar
);

const File = Components.Constructor(
  "@mozilla.org/file/local;1",
  Ci.nsIFile,
  "initWithPath"
);

function buildGroupId(id) {
  try {
    return `${taskbar.defaultGroupId}.ssb.${id}`;
  } catch (e) {
    return `Firefox.ssb.${id}`;
  }
}

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

    let dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    await IOUtils.makeDirectory(dir, {
      from: PathUtils.profileDir,
      ignoreExisting: true,
    });

    let iconFile = new File(PathUtils.join(dir, "icon.ico"));

    // We should be embedding multiple icon sizes, but the current icon encoder
    // does not support this. For now just embed a sensible size.
    let icon = await SiteSpecificBrowserIdUtils.getIconBySSBId(ssb.id, 128);
    if (icon) {
      let { container } = await ImageTools.loadImage(
        Services.io.newURI(icon.src)
      );
      ImageTools.saveIcon(container, 128, 128, iconFile);
    } else {
      // TODO use a default icon file.
      iconFile = null;
    }

    shellService.createShortcut(
      Services.dirsvc.get("XREExeF", Ci.nsIFile),
      ["-profile", PathUtils.profileDir, "-start-ssb", ssb.id],
      ssb.name,
      iconFile,
      0,
      buildGroupId(ssb.id),
      "Programs",
      `${ssb.name}.lnk`,
      Services.dirsvc.get("Desk", Ci.nsIFile),
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
    let link = PathUtils.join(desktop.path, `${ssb.name}.lnk`);

    try {
      await IOUtils.remove(link, { 
        recursive: true,
      });
    } catch (e) {
      console.error(e);
    }

    let dir = PathUtils.join(PathUtils.profileDir, "ssb", ssb.id);
    try {
      await IOUtils.removeDir(dir, {
        ignoreAbsent: true,
        ignorePermissions: true,
        recursive: true,
      });
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * Applies the necessary OS integration to an open SSB.
   *
   * Sets the window icon based on the available icons.
   *
   * @param {SiteSpecificBrowser} ssb the SSB.
   * @param {DOMWindow} window the window showing the SSB.
   */
  async applyOSIntegration(ssb, window) {
    const getIcon = async size => {
      let icon = await SiteSpecificBrowserIdUtils.getIconBySSBId(ssb._id, size);
      if (!icon) {
        return null;
      }

      try {
        let image = await ImageTools.loadImage(Services.io.newURI(icon.src));
        return image.container;
      } catch (e) {
        console.error(e);
        return null;
      }
    };

    if (!SiteSpecificBrowserService.useOSIntegration) {
      return;
    }

    let icons = await Promise.all([
      getIcon(uiUtils.systemSmallIconSize),
      getIcon(uiUtils.systemLargeIconSize),
    ]);

    if (icons[0] || icons[1]) {
      uiUtils.setWindowIcon(window, icons[0], icons[1]);
    }

    taskbar.setGroupIdForWindow(window, buildGroupId(ssb._id));
  },
};
