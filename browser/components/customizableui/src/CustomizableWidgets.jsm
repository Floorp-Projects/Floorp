/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["CustomizableWidgets"];

Cu.import("resource:///modules/CustomizableUI.jsm");

const CustomizableWidgets = [{
    id: "history-panelmenu",
    type: "view",
    viewId: "PanelUI-history",
    name: "History...",
    description: "History repeats itself!",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL, CustomizableUI.AREA_NAVBAR],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onViewShowing: function(aEvent) {
      // Populate our list of history
      const kMaxResults = 15;
      let doc = aEvent.detail.ownerDocument;

      let options = PlacesUtils.history.getNewQueryOptions();
      options.excludeQueries = true;
      options.includeHidden = false;
      options.resultType = options.RESULTS_AS_URI;
      options.queryType = options.QUERY_TYPE_HISTORY;
      options.sortingMode = options.SORT_BY_DATE_DESCENDING;
      options.maxResults = kMaxResults;
      let query = PlacesUtils.history.getNewQuery();

      let items = doc.getElementById("PanelUI-historyItems");
      // Clear previous history items.
      while (items.firstChild) {
        items.removeChild(items.firstChild);
      }

      PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                         .asyncExecuteLegacyQueries([query], 1, options, {
        handleResult: function (aResultSet) {
          let fragment = doc.createDocumentFragment();
          for (let row, i = 0; (row = aResultSet.getNextRow()); i++) {
            try {
              let uri = row.getResultByIndex(1);
              let title = row.getResultByIndex(2);
              let icon = row.getResultByIndex(6);

              let item = doc.createElementNS(kNSXUL, "toolbarbutton");
              item.setAttribute("label", title || uri);
              item.addEventListener("click", function(aEvent) {
                if (aEvent.button == 0) {
                  doc.defaultView.openUILink(uri, aEvent);
                  doc.defaultView.PanelUI.hide();
                }
              });
              if (icon)
                item.setAttribute("image", "moz-anno:favicon:" + icon);
              fragment.appendChild(item);
            } catch (e) {
              Cu.reportError("Error while showing history subview: " + e);
            }
          }
          items.appendChild(fragment);
        },
        handleError: function (aError) {
          LOG("History view tried to show but had an error: " + aError);
        },
        handleCompletion: function (aReason) {
          LOG("History view is being shown!");
        },
      });
    },
    onViewHiding: function(aEvent) {
      LOG("History view is being hidden!");
    }
  }, {
    id: "privatebrowsing-button",
    name: "Private Browsing\u2026",
    description: "Open a new Private Browsing window",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onCommand: function(e) {
      if (e.target && e.target.ownerDocument && e.target.ownerDocument.defaultView) {
        let win = e.target.ownerDocument.defaultView;
        if (typeof win.OpenBrowserWindow == "function") {
          win.OpenBrowserWindow({private: true});
        }
      }
    }
  }, {
    id: "save-page-button",
    name: "Save Page",
    shortcut: "Ctrl+S",
    description: "Save this page",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && typeof win.saveDocument == "function") {
        win.saveDocument(win.content.document);
      }
    }
  }, {
    id: "find-button",
    name: "Find",
    shortcut: "Ctrl+F",
    description: "Find in this page",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && win.gFindBar) {
        win.gFindBar.onFindCommand();
      }
    }
  }, {
    id: "open-file-button",
    name: "Open File",
    shortcut: "Ctrl+O",
    description: "Open file",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onCommand: function(aEvent) {
      let win = aEvent.target
                && aEvent.target.ownerDocument
                && aEvent.target.ownerDocument.defaultView;
      if (win && typeof win.BrowserOpenFileWindow == "function") {
        win.BrowserOpenFileWindow();
      }
    }
  }, {
    id: "developer-button",
    name: "Developer",
    shortcut: "Shift+F11",
    description: "Toggle Developer Tools",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && win.gDevToolsBrowser) {
        win.gDevToolsBrowser.toggleToolboxCommand(win.gBrowser);
      }
    }
  }, {
    id: "add-ons-button",
    name: "Add-ons",
    shortcut: "Ctrl+Shift+A",
    description: "Add-ons Manager",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && typeof win.BrowserOpenAddonsMgr == "function") {
        win.BrowserOpenAddonsMgr();
      }
    }
  }, {
    id: "preferences-button",
    name: "Preferences",
    shortcut: "Ctrl+Shift+O",
    description: "Preferences\u2026",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && typeof win.openPreferences == "function") {
        win.openPreferences();
      }
    }
  }];
