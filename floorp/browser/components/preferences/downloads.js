/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

Preferences.addAll([

  // Downloads
  { id: "browser.download.useDownloadDir", type: "bool", inverted: true },
  { id: "browser.download.always_ask_before_handling_new_types", type: "bool" },
  { id: "browser.download.folderList", type: "int" },
  { id: "browser.download.dir", type: "file" },

  
  // Files and Applications
  { id: "pref.downloads.disable_button.edit_actions", type: "bool" },

  { id: "floorp.download.notification", type: "int" },
  { id: "floorp.legacy.dlui.enable", type: "bool" },
  { id: "floorp.downloading.red.color", type: "bool" },
  ])

  var gDownloads = {
    _pane: null,
    init() {
      this._pane = document.getElementById("paneDownloads");

      Preferences.get("browser.download.folderList").on(
        "change",
        gMainPane.displayDownloadDirPref.bind(gMainPane)
      );
      Preferences.get("browser.download.dir").on(
        "change",
        gMainPane.displayDownloadDirPref.bind(gMainPane)
      );
      gMainPane.displayDownloadDirPref();
      Preferences.addSyncFromPrefListener(
        document.getElementById("alwaysAsk"),
        gMainPane.readUseDownloadDir.bind(gMainPane)
      );
      function setEventListener(aId, aEventType, aCallback) {
        document
          .getElementById(aId)
          .addEventListener(aEventType, aCallback.bind(gMainPane));
      }
      setEventListener("filter", "command", gMainPane.filter);
      setEventListener("typeColumn", "click", gMainPane.sort);
      setEventListener("actionColumn", "click", gMainPane.sort);
      setEventListener("chooseFolder", "command", gMainPane.chooseFolder);

      // Figure out how we should be sorting the list.  We persist sort settings
    // across sessions, so we can't assume the default sort column/direction.
    // XXX should we be using the XUL sort service instead?
    if (document.getElementById("actionColumn").hasAttribute("sortDirection")) {
        this._sortColumn = document.getElementById("actionColumn");
        // The typeColumn element always has a sortDirection attribute,
        // either because it was persisted or because the default value
        // from the xul file was used.  If we are sorting on the other
        // column, we should remove it.
        document.getElementById("typeColumn").removeAttribute("sortDirection");
      } else {
        this._sortColumn = document.getElementById("typeColumn");
      }
      (async () => {
        await gMainPane.initialized;
        try {
          gMainPane._initListEventHandlers();
          gMainPane._loadData();
          await gMainPane._rebuildVisibleTypes();
          await gMainPane._rebuildView();
          await gMainPane._sortListView();
        } catch (ex) {
        }
      })();
    },
          
  };