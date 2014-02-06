/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const PREF_BD_USEDOWNLOADDIR = "browser.download.useDownloadDir";
const URI_GENERIC_ICON_DOWNLOAD = "chrome://browser/skin/images/alert-downloads-30.png";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "ContentUtil", function() {
  Cu.import("resource:///modules/ContentUtil.jsm");
  return ContentUtil;
});
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

// -----------------------------------------------------------------------
// HelperApp Launcher Dialog
// -----------------------------------------------------------------------

function HelperAppLauncherDialog() { }

HelperAppLauncherDialog.prototype = {
  classID: Components.ID("{e9d277a0-268a-4ec2-bb8c-10fdf3e44611}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),

  show: function hald_show(aLauncher, aContext, aReason) {
    // Check to see if we can open this file or not
    // If the file is an executable then launchWithApplication will fail in
    // /uriloader nsMIMEInfoWin.cpp code. So always download in that case.
    if (aLauncher.MIMEInfo.hasDefaultHandler && !aLauncher.targetFileIsExecutable) {
      aLauncher.MIMEInfo.preferredAction = Ci.nsIMIMEInfo.useSystemDefault;
      aLauncher.launchWithApplication(null, false);
    } else {
      let wasClicked = false;
      this._showDownloadInfobar(aLauncher);
    }
  },

  _getDownloadSize: function dv__getDownloadSize (aSize) {
    let displaySize = DownloadUtils.convertByteUnits(aSize);
    // displaySize[0] is formatted size, displaySize[1] is units
    if (aSize > 0)
      return displaySize.join("");
    else {
      let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
      return browserBundle.GetStringFromName("downloadsUnknownSize");
    }
  },

  _getChromeWindow: function (aWindow) {
      let chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsIDocShellTreeItem)
                            .rootTreeItem
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow)
                            .QueryInterface(Ci.nsIDOMChromeWindow);
     return chromeWin;
  },

  _showDownloadInfobar: function do_showDownloadInfobar(aLauncher) {
    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

    let runButtonText =
              browserBundle.GetStringFromName("downloadOpen");
    let saveButtonText =
              browserBundle.GetStringFromName("downloadSave");
    let cancelButtonText =
              browserBundle.GetStringFromName("downloadCancel");

    let buttons = [
      {
        isDefault: true,
        label: runButtonText,
        accessKey: "",
        callback: function() {
          aLauncher.saveToDisk(null, false);
          Services.obs.notifyObservers(aLauncher.targetFile, "dl-run", "true");
        }
      },
      {
        label: saveButtonText,
        accessKey: "",
        callback: function() {
          aLauncher.saveToDisk(null, false);
          Services.obs.notifyObservers(aLauncher.targetFile, "dl-run", "false");
        }
      },
      {
        label: cancelButtonText,
        accessKey: "",
        callback: function() { aLauncher.cancel(Cr.NS_BINDING_ABORTED); }
      }
    ];

    let window = Services.wm.getMostRecentWindow("navigator:browser");
    let chromeWin = this._getChromeWindow(window).wrappedJSObject;
    let notificationBox = chromeWin.Browser.getNotificationBox();
    let document = notificationBox.ownerDocument;
    downloadSize = this._getDownloadSize(aLauncher.contentLength);

    let msg = browserBundle.GetStringFromName("alertDownloadSave2");

    let fragment =  ContentUtil.populateFragmentFromString(
                      document.createDocumentFragment(),
                      msg,
                      {
                        text: aLauncher.suggestedFileName,
                        className: "download-filename-text"
                      },
                      {
                        text: downloadSize,
                        className: "download-size-text"
                      },
                      {
                        text: aLauncher.source.host,
                        className: "download-host-text"
                      }
                    );
    let newBar = notificationBox.appendNotification("",
                                                    "save-download",
                                                    URI_GENERIC_ICON_DOWNLOAD,
                                                    notificationBox.PRIORITY_WARNING_HIGH,
                                                    buttons);
    let messageContainer = document.getAnonymousElementByAttribute(newBar, "anonid", "messageText");
    messageContainer.appendChild(fragment);
  },

  promptForSaveToFile: function hald_promptForSaveToFile(aLauncher, aContext, aDefaultFile, aSuggestedFileExt, aForcePrompt) {
    throw new Components.Exception("Async version must be used", Cr.NS_ERROR_NOT_AVAILABLE);
  },

  promptForSaveToFileAsync: function hald_promptForSaveToFileAsync(aLauncher, aContext, aDefaultFile, aSuggestedFileExt, aForcePrompt) {
    let file = null;
    let prefs = Services.prefs;

    Task.spawn(function() {
      if (!aForcePrompt) {
        // Check to see if the user wishes to auto save to the default download
        // folder without prompting. Note that preference might not be set.
        let autodownload = true;
        try {
          autodownload = prefs.getBoolPref(PREF_BD_USEDOWNLOADDIR);
        } catch (e) { }

        if (autodownload) {
          // Retrieve the user's preferred download directory
          let preferredDir = yield Downloads.getPreferredDownloadsDirectory();
          let defaultFolder = new FileUtils.File(preferredDir);

          try {
            file = this.validateLeafName(defaultFolder, aDefaultFile, aSuggestedFileExt);
          }
          catch (e) {
          }

          // Check to make sure we have a valid directory, otherwise, prompt
          if (file) {
            aLauncher.saveDestinationAvailable(file);
            return;
          }
        }
      }

      // Use file picker to show dialog.
      let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      let windowTitle = "";
      let parent = aContext.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
      picker.init(parent, windowTitle, Ci.nsIFilePicker.modeSave);
      picker.defaultString = aDefaultFile;

      if (aSuggestedFileExt) {
        // aSuggestedFileExtension includes the period, so strip it
        picker.defaultExtension = aSuggestedFileExt.substring(1);
      }
      else {
        try {
          picker.defaultExtension = aLauncher.MIMEInfo.primaryExtension;
        }
        catch (e) { }
      }

      let wildCardExtension = "*";
      if (aSuggestedFileExt) {
        wildCardExtension += aSuggestedFileExt;
        picker.appendFilter(aLauncher.MIMEInfo.description, wildCardExtension);
      }

      picker.appendFilters(Ci.nsIFilePicker.filterAll);

      // Default to lastDir if it is valid, otherwise use the user's preferred
      // downloads directory.  getPreferredDownloadsDirectory should always
      // return a valid directory string, so we can safely default to it.
      let preferredDir = yield Downloads.getPreferredDownloadsDirectory();
      picker.displayDirectory = new FileUtils.File(preferredDir);

      // The last directory preference may not exist, which will throw.
      try {
        let lastDir = prefs.getComplexValue("browser.download.lastDir", Ci.nsILocalFile);
        if (isUsableDirectory(lastDir))
          picker.displayDirectory = lastDir;
      }
      catch (e) { }

      picker.open(function(aResult) {
        if (aResult == Ci.nsIFilePicker.returnCancel) {
          // null result means user cancelled.
          aLauncher.saveDestinationAvailable(null);
          return;
        }

        // Be sure to save the directory the user chose through the Save As...
        // dialog  as the new browser.download.dir since the old one
        // didn't exist.
        file = picker.file;

        if (file) {
          try {
            // Remove the file so that it's not there when we ensure non-existence later;
            // this is safe because for the file to exist, the user would have had to
            // confirm that he wanted the file overwritten.
            if (file.exists())
              file.remove(false);
          }
          catch (e) { }
          let newDir = file.parent.QueryInterface(Ci.nsILocalFile);
          prefs.setComplexValue("browser.download.lastDir", Ci.nsILocalFile, newDir);
          file = this.validateLeafName(newDir, file.leafName, null);
        }
        aLauncher.saveDestinationAvailable(file);
      }.bind(this));
    }.bind(this));
  },

  validateLeafName: function hald_validateLeafName(aLocalFile, aLeafName, aFileExt) {
    if (!(aLocalFile && this.isUsableDirectory(aLocalFile)))
      return null;

    // Remove any leading periods, since we don't want to save hidden files
    // automatically.
    aLeafName = aLeafName.replace(/^\.+/, "");

    if (aLeafName == "")
      aLeafName = "unnamed" + (aFileExt ? "." + aFileExt : "");
    aLocalFile.append(aLeafName);

    this.makeFileUnique(aLocalFile);
    return aLocalFile;
  },

  makeFileUnique: function hald_makeFileUnique(aLocalFile) {
    try {
      // Note - this code is identical to that in
      //   toolkit/content/contentAreaUtils.js.
      // If you are updating this code, update that code too! We can't share code
      // here since this is called in a js component.
      var collisionCount = 0;
      while (aLocalFile.exists()) {
        collisionCount++;
        if (collisionCount == 1) {
          // Append "(2)" before the last dot in (or at the end of) the filename
          // special case .ext.gz etc files so we don't wind up with .tar(2).gz
          if (aLocalFile.leafName.match(/\.[^\.]{1,3}\.(gz|bz2|Z)$/i))
            aLocalFile.leafName = aLocalFile.leafName.replace(/\.[^\.]{1,3}\.(gz|bz2|Z)$/i, "(2)$&");
          else
            aLocalFile.leafName = aLocalFile.leafName.replace(/(\.[^\.]*)?$/, "(2)$&");
        }
        else {
          // replace the last (n) in the filename with (n+1)
          aLocalFile.leafName = aLocalFile.leafName.replace(/^(.*\()\d+\)/, "$1" + (collisionCount+1) + ")");
        }
      }
      aLocalFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
    }
    catch (e) {
      dump("*** exception in validateLeafName: " + e + "\n");

      if (e.result == Cr.NS_ERROR_FILE_ACCESS_DENIED)
        throw e;

      if (aLocalFile.leafName == "" || aLocalFile.isDirectory()) {
        aLocalFile.append("unnamed");
        if (aLocalFile.exists())
          aLocalFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
      }
    }
  },

  isUsableDirectory: function hald_isUsableDirectory(aDirectory) {
    return aDirectory.exists() && aDirectory.isDirectory() && aDirectory.isWritable();
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HelperAppLauncherDialog]);
