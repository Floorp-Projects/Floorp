/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["CustomizableWidgets"];

Cu.import("resource:///modules/CustomizableUI.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentlyClosedTabsAndWindowsMenuUtils",
  "resource:///modules/sessionstore/RecentlyClosedTabsAndWindowsMenuUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "CharsetManager",
                                   "@mozilla.org/charset-converter-manager;1",
                                   "nsICharsetConverterManager");
XPCOMUtils.defineLazyGetter(this, "BrandBundle", function() {
  const kBrandBundle = "chrome://branding/locale/brand.properties";
  return Services.strings.createBundle(kBrandBundle);
});

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kPrefCustomizationDebug = "browser.uiCustomization.debug";
const kWidePanelItemClass = "panel-wide-item";

let gModuleName = "[CustomizableWidgets]";
#include logging.js

function setAttributes(aNode, aAttrs) {
  for (let [name, value] of Iterator(aAttrs)) {
    if (!value) {
      if (aNode.hasAttribute(name))
        aNode.removeAttribute(name);
    } else {
      if (name == "label" || name == "tooltiptext")
        value = CustomizableUI.getLocalizedProperty({id: aAttrs.id}, name);
      aNode.setAttribute(name, value);
    }
  }
}

function updateCombinedWidgetStyle(aNode, aArea, aModifyAutoclose) {
  let inPanel = (aArea == CustomizableUI.AREA_PANEL);
  let cls = inPanel ? "panel-combined-button" : "toolbarbutton-1";
  let attrs = {class: cls};
  if (aModifyAutoclose) {
    attrs.noautoclose = inPanel ? true : null;
  }
  for (let i = 0, l = aNode.childNodes.length; i < l; ++i) {
    if (aNode.childNodes[i].localName == "separator")
      continue;
    setAttributes(aNode.childNodes[i], attrs);
  }
}

const CustomizableWidgets = [{
    id: "history-panelmenu",
    type: "view",
    viewId: "PanelUI-history",
    shortcutId: "key_gotoHistory",
    defaultArea: CustomizableUI.AREA_PANEL,
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
          let onHistoryVisit = function (aUri, aEvent, aItem) {
            doc.defaultView.openUILink(aUri, aEvent);
            CustomizableUI.hidePanelForNode(aItem);
          };
          let fragment = doc.createDocumentFragment();
          for (let row, i = 0; (row = aResultSet.getNextRow()); i++) {
            try {
              let uri = row.getResultByIndex(1);
              let title = row.getResultByIndex(2);
              let icon = row.getResultByIndex(6);

              let item = doc.createElementNS(kNSXUL, "toolbarbutton");
              item.setAttribute("label", title || uri);
              item.setAttribute("tabindex", "0");
              item.setAttribute("targetURI", uri);
              item.addEventListener("command", function (aEvent) {
                onHistoryVisit(uri, aEvent, item);
              });
              item.addEventListener("click", function (aEvent) {
                onHistoryVisit(uri, aEvent, item);
              });
              if (icon)
                item.setAttribute("image", "moz-anno:favicon:" + icon);
              fragment.appendChild(item);
            } catch (e) {
              ERROR("Error while showing history subview: " + e);
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

      let recentlyClosedTabs = doc.getElementById("PanelUI-recentlyClosedTabs");
      while (recentlyClosedTabs.firstChild) {
        recentlyClosedTabs.removeChild(recentlyClosedTabs.firstChild);
      }

      let recentlyClosedWindows = doc.getElementById("PanelUI-recentlyClosedWindows");
      while (recentlyClosedWindows.firstChild) {
        recentlyClosedWindows.removeChild(recentlyClosedWindows.firstChild);
      }

#ifdef MOZ_SERVICES_SYNC
      let tabsFromOtherComputers = doc.getElementById("sync-tabs-menuitem2");
      if (PlacesUIUtils.shouldShowTabsFromOtherComputersMenuitem()) {
        tabsFromOtherComputers.removeAttribute("hidden");
      } else {
        tabsFromOtherComputers.setAttribute("hidden", true);
      }

      if (PlacesUIUtils.shouldEnableTabsFromOtherComputersMenuitem()) {
        tabsFromOtherComputers.removeAttribute("disabled");
      } else {
        tabsFromOtherComputers.setAttribute("disabled", true);
      }
#endif

      let tabsFragment = RecentlyClosedTabsAndWindowsMenuUtils.getTabsFragment(doc.defaultView, "toolbarbutton");
      let separator = doc.getElementById("PanelUI-recentlyClosedTabs-separator");
      separator.hidden = !tabsFragment.childElementCount;
      recentlyClosedTabs.appendChild(tabsFragment);

      let windowsFragment = RecentlyClosedTabsAndWindowsMenuUtils.getWindowsFragment(doc.defaultView, "toolbarbutton");
      separator = doc.getElementById("PanelUI-recentlyClosedWindows-separator");
      separator.hidden = !windowsFragment.childElementCount;
      recentlyClosedWindows.appendChild(windowsFragment);
    },
    onViewHiding: function(aEvent) {
      LOG("History view is being hidden!");
    }
  }, {
    id: "privatebrowsing-button",
    shortcutId: "key_privatebrowsing",
    defaultArea: CustomizableUI.AREA_PANEL,
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
    shortcutId: "key_savePage",
    defaultArea: CustomizableUI.AREA_PANEL,
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
    shortcutId: "key_find",
    defaultArea: CustomizableUI.AREA_PANEL,
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
    shortcutId: "openFileKb",
    defaultArea: CustomizableUI.AREA_PANEL,
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
    type: "view",
    viewId: "PanelUI-developer",
    shortcutId: "key_devToolboxMenuItem",
    defaultArea: CustomizableUI.AREA_PANEL,
    onViewShowing: function(aEvent) {
      // Populate the subview with whatever menuitems are in the developer
      // menu. We skip menu elements, because the menu panel has no way
      // of dealing with those right now.
      let doc = aEvent.target.ownerDocument;
      let win = doc.defaultView;

      let items = doc.getElementById("PanelUI-developerItems");
      let menu = doc.getElementById("menuWebDeveloperPopup");
      let attrs = ["oncommand", "onclick", "label", "key", "disabled",
                   "command", "observes"];

      let fragment = doc.createDocumentFragment();
      let itemsToDisplay = [...menu.children];
      // Hardcode the addition of the "work offline" menuitem at the bottom:
      itemsToDisplay.push({localName: "menuseparator", getAttribute: () => {}});
      itemsToDisplay.push(doc.getElementById("goOfflineMenuitem"));
      for (let node of itemsToDisplay) {
        if (node.hidden)
          continue;

        let item;
        if (node.localName == "menuseparator") {
          item = doc.createElementNS(kNSXUL, "menuseparator");
        } else if (node.localName == "menuitem") {
          item = doc.createElementNS(kNSXUL, "toolbarbutton");
          item.setAttribute("tabindex", "0");
        } else {
          continue;
        }
        for (let attr of attrs) {
          let attrVal = node.getAttribute(attr);
          if (attrVal)
            item.setAttribute(attr, attrVal);
        }
        fragment.appendChild(item);
      }
      items.appendChild(fragment);

      aEvent.target.addEventListener("command", win.PanelUI.onCommandHandler);
    },
    onViewHiding: function(aEvent) {
      let doc = aEvent.target.ownerDocument;
      let win = doc.defaultView;
      let items = doc.getElementById("PanelUI-developerItems");
      let parent = items.parentNode;
      // We'll take the container out of the document before cleaning it out
      // to avoid reflowing each time we remove something.
      parent.removeChild(items);

      while (items.firstChild) {
        items.firstChild.remove();
      }

      parent.appendChild(items);
      aEvent.target.removeEventListener("command",
                                        win.PanelUI.onCommandHandler);
    }
  }, {
    id: "add-ons-button",
    shortcutId: "key_openAddons",
    defaultArea: CustomizableUI.AREA_PANEL,
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
    defaultArea: CustomizableUI.AREA_PANEL,
#ifdef XP_WIN
    label: "preferences-button.labelWin",
    tooltiptext: "preferences-button.tooltipWin",
#endif
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && typeof win.openPreferences == "function") {
        win.openPreferences();
      }
    }
  }, {
    id: "zoom-controls",
    type: "custom",
    defaultArea: CustomizableUI.AREA_PANEL,
    onBuild: function(aDocument) {
      const kPanelId = "PanelUI-popup";
      let areaType = CustomizableUI.getAreaType(this.currentArea);
      let inPanel = areaType == CustomizableUI.TYPE_MENU_PANEL;
      let inToolbar = areaType == CustomizableUI.TYPE_TOOLBAR;
      let noautoclose = inPanel ? "true" : null;
      let cls = inPanel ? "panel-combined-button" : "toolbarbutton-1";

      if (!this.currentArea)
        cls = null;

      let buttons = [{
        id: "zoom-out-button",
        noautoclose: noautoclose,
        command: "cmd_fullZoomReduce",
        class: cls,
        label: true,
        tooltiptext: true
      }, {
        id: "zoom-reset-button",
        noautoclose: noautoclose,
        command: "cmd_fullZoomReset",
        class: cls,
        tooltiptext: true
      }, {
        id: "zoom-in-button",
        noautoclose: noautoclose,
        command: "cmd_fullZoomEnlarge",
        class: cls,
        label: true,
        tooltiptext: true
      }];

      let node = aDocument.createElementNS(kNSXUL, "toolbaritem");
      node.setAttribute("id", "zoom-controls");
      node.setAttribute("title", CustomizableUI.getLocalizedProperty(this, "tooltiptext"));
      // Set this as an attribute in addition to the property to make sure we can style correctly.
      node.setAttribute("removable", "true");
      node.classList.add("chromeclass-toolbar-additional");
      node.classList.add("toolbaritem-combined-buttons");
      node.classList.add(kWidePanelItemClass);

      buttons.forEach(function(aButton, aIndex) {
        if (aIndex != 0)
          node.appendChild(aDocument.createElementNS(kNSXUL, "separator"));
        let btnNode = aDocument.createElementNS(kNSXUL, "toolbarbutton");
        setAttributes(btnNode, aButton);
        if (inPanel)
          btnNode.setAttribute("tabindex", "0");
        node.appendChild(btnNode);
      });

      // The middle node is the 'Reset Zoom' button.
      let zoomResetButton = node.childNodes[2];
      let window = aDocument.defaultView;
      function updateZoomResetButton() {
        //XXXgijs in some tests we get called very early, and there's no docShell on the
        // tabbrowser. This breaks the zoom toolkit code (see bug 897410). Don't let that happen:
        let zoomFactor = 100;
        try {
          zoomFactor = Math.floor(window.ZoomManager.zoom * 100);
        } catch (e) {}
        zoomResetButton.setAttribute("label", CustomizableUI.getLocalizedProperty(
          buttons[1], "label", [zoomFactor]
        ));
      };

      // Register ourselves with the service so we know when the zoom prefs change.
      Services.obs.addObserver(updateZoomResetButton, "browser-fullZoom:zoomChange", false);
      Services.obs.addObserver(updateZoomResetButton, "browser-fullZoom:zoomReset", false);

      if (inPanel) {
        let panel = aDocument.getElementById(kPanelId);
        panel.addEventListener("popupshowing", updateZoomResetButton);
      } else {
        if (inToolbar) {
          let container = window.gBrowser.tabContainer;
          container.addEventListener("TabSelect", updateZoomResetButton);
        }
        updateZoomResetButton();
      }

      let listener = {
        onWidgetAdded: function(aWidgetId, aArea, aPosition) {
          if (aWidgetId != this.id)
            return;

          updateCombinedWidgetStyle(node, aArea, true);
          updateZoomResetButton();

          let areaType = CustomizableUI.getAreaType(aArea);
          if (areaType == CustomizableUI.TYPE_MENU_PANEL) {
            let panel = aDocument.getElementById(kPanelId);
            panel.addEventListener("popupshowing", updateZoomResetButton);
          } else if (areaType == CustomizableUI.TYPE_TOOLBAR) {
            let container = window.gBrowser.tabContainer;
            container.addEventListener("TabSelect", updateZoomResetButton);
          }
        }.bind(this),

        onWidgetRemoved: function(aWidgetId, aPrevArea) {
          if (aWidgetId != this.id)
            return;

          let areaType = CustomizableUI.getAreaType(aPrevArea);
          if (areaType == CustomizableUI.TYPE_MENU_PANEL) {
            let panel = aDocument.getElementById(kPanelId);
            panel.removeEventListener("popupshowing", updateZoomResetButton);
          } else if (areaType == CustomizableUI.TYPE_TOOLBAR) {
            let container = window.gBrowser.tabContainer;
            container.removeEventListener("TabSelect", updateZoomResetButton);
          }

          // When a widget is demoted to the palette ('removed'), it's visual
          // style should change.
          updateCombinedWidgetStyle(node, null, true);
          updateZoomResetButton();
        }.bind(this),

        onWidgetReset: function(aWidgetNode) {
          if (aWidgetNode != node)
            return;
          updateCombinedWidgetStyle(node, this.currentArea, true);
          updateZoomResetButton();
        }.bind(this),

        onWidgetMoved: function(aWidgetId, aArea) {
          if (aWidgetId != this.id)
            return;
          updateCombinedWidgetStyle(node, aArea, true);
          updateZoomResetButton();
        }.bind(this),

        onWidgetInstanceRemoved: function(aWidgetId, aDoc) {
          if (aWidgetId != this.id || aDoc != aDocument)
            return;

          CustomizableUI.removeListener(listener);
          Services.obs.removeObserver(updateZoomResetButton, "browser-fullZoom:zoomChange");
          Services.obs.removeObserver(updateZoomResetButton, "browser-fullZoom:zoomReset");
          let panel = aDoc.getElementById(kPanelId);
          panel.removeEventListener("popupshowing", updateZoomResetButton);
          let container = aDoc.defaultView.gBrowser.tabContainer;
          container.removeEventListener("TabSelect", updateZoomResetButton);
        }.bind(this),

        onWidgetDrag: function(aWidgetId, aArea) {
          if (aWidgetId != this.id)
            return;
          aArea = aArea || this.currentArea;
          updateCombinedWidgetStyle(node, aArea, true);
        }.bind(this)
      };
      CustomizableUI.addListener(listener);

      return node;
    }
  }, {
    id: "edit-controls",
    type: "custom",
    defaultArea: CustomizableUI.AREA_PANEL,
    onBuild: function(aDocument) {
      let inPanel = (this.currentArea == CustomizableUI.AREA_PANEL);
      let cls = inPanel ? "panel-combined-button" : "toolbarbutton-1";

      if (!this.currentArea)
        cls = null;

      let buttons = [{
        id: "cut-button",
        command: "cmd_cut",
        class: cls,
        label: true,
        tooltiptext: true
      }, {
        id: "copy-button",
        command: "cmd_copy",
        class: cls,
        label: true,
        tooltiptext: true
      }, {
        id: "paste-button",
        command: "cmd_paste",
        class: cls,
        label: true,
        tooltiptext: true
      }];

      let node = aDocument.createElementNS(kNSXUL, "toolbaritem");
      node.setAttribute("id", "edit-controls");
      node.setAttribute("title", CustomizableUI.getLocalizedProperty(this, "tooltiptext"));
      // Set this as an attribute in addition to the property to make sure we can style correctly.
      node.setAttribute("removable", "true");
      node.classList.add("chromeclass-toolbar-additional");
      node.classList.add("toolbaritem-combined-buttons");
      node.classList.add(kWidePanelItemClass);

      buttons.forEach(function(aButton, aIndex) {
        if (aIndex != 0)
          node.appendChild(aDocument.createElementNS(kNSXUL, "separator"));
        let btnNode = aDocument.createElementNS(kNSXUL, "toolbarbutton");
        setAttributes(btnNode, aButton);
        node.appendChild(btnNode);
      });

      let listener = {
        onWidgetAdded: function(aWidgetId, aArea, aPosition) {
          if (aWidgetId != this.id)
            return;
          updateCombinedWidgetStyle(node, aArea);
        }.bind(this),

        onWidgetRemoved: function(aWidgetId, aPrevArea) {
          if (aWidgetId != this.id)
            return;
          // When a widget is demoted to the palette ('removed'), it's visual
          // style should change.
          updateCombinedWidgetStyle(node);
        }.bind(this),

        onWidgetReset: function(aWidgetNode) {
          if (aWidgetNode != node)
            return;
          updateCombinedWidgetStyle(node, this.currentArea);
        }.bind(this),

        onWidgetMoved: function(aWidgetId, aArea) {
          if (aWidgetId != this.id)
            return;
          updateCombinedWidgetStyle(node, aArea);
        }.bind(this),

        onWidgetInstanceRemoved: function(aWidgetId, aDoc) {
          if (aWidgetId != this.id || aDoc != aDocument)
            return;
          CustomizableUI.removeListener(listener);
        }.bind(this),

        onWidgetDrag: function(aWidgetId, aArea) {
          if (aWidgetId != this.id)
            return;
          aArea = aArea || this.currentArea;
          updateCombinedWidgetStyle(node, aArea);
        }.bind(this)
      };
      CustomizableUI.addListener(listener);

      return node;
    }
  },
  {
    id: "feed-button",
    type: "view",
    viewId: "PanelUI-feeds",
    defaultArea: CustomizableUI.AREA_PANEL,
    onClick: function(aEvent) {
      let win = aEvent.target.ownerDocument.defaultView;
      let feeds = win.gBrowser.selectedBrowser.feeds;

      // Here, we only care about the case where we have exactly 1 feed and the
      // user clicked...
      let isClick = (aEvent.button == 0 || aEvent.button == 1);
      if (feeds && feeds.length == 1 && isClick) {
        aEvent.preventDefault();
        aEvent.stopPropagation();
        win.FeedHandler.subscribeToFeed(feeds[0].href, aEvent);
        CustomizableUI.hidePanelForNode(aEvent.target);
      }
    },
    onViewShowing: function(aEvent) {
      let doc = aEvent.detail.ownerDocument;
      let container = doc.getElementById("PanelUI-feeds");
      let gotView = doc.defaultView.FeedHandler.buildFeedList(container, true);

      // For no feeds or only a single one, don't show the panel.
      if (!gotView) {
        aEvent.preventDefault();
        aEvent.stopPropagation();
        return;
      }
    },
    onCreated: function(node) {
      let win = node.ownerDocument.defaultView;
      let selectedBrowser = win.gBrowser.selectedBrowser;
      let feeds = selectedBrowser && selectedBrowser.feeds;
      if (!feeds || !feeds.length) {
        node.setAttribute("disabled", "true");
      }
    }
  }, {
    id: "characterencoding-button",
    type: "view",
    viewId: "PanelUI-characterEncodingView",
    defaultArea: CustomizableUI.AREA_PANEL,
    maybeDisableMenu: function(aDocument) {
      let window = aDocument.defaultView;
      return !(window.gBrowser &&
               window.gBrowser.docShell &&
               window.gBrowser.docShell.mayEnableCharacterEncodingMenu);
    },
    getCharsetList: function(aSection, aDocument) {
      let currCharset = aDocument.defaultView.content.document.characterSet;

      let list = "";
      try {
        let pref = "intl.charsetmenu.browser." + aSection;
        list = Services.prefs.getComplexValue(pref,
                                              Ci.nsIPrefLocalizedString).data;
      } catch (e) {}

      list = list.trim();
      if (!list)
        return [];

      list = list.split(",");

      let items = [];
      for (let charset of list) {
        charset = charset.trim();

        let notForBrowser = false;
        try {
          notForBrowser = CharsetManager.getCharsetData(charset,
                                                        "notForBrowser");
        } catch (e) {}

        if (notForBrowser)
          continue;

        let title = charset;
        try {
          title = CharsetManager.getCharsetTitle(charset);
        } catch (e) {}

        items.push({value: charset, name: title, current: charset == currCharset});
      }

      return items;
    },
    getAutoDetectors: function(aDocument) {
      let detectorEnum = CharsetManager.GetCharsetDetectorList();
      let currDetector;
      try {
        currDetector = Services.prefs.getComplexValue(
          "intl.charset.detector", Ci.nsIPrefLocalizedString).data;
      } catch (e) {}
      if (!currDetector)
        currDetector = "off";
      currDetector = "chardet." + currDetector;

      let items = [];

      while (detectorEnum.hasMore()) {
        let detector = detectorEnum.getNext();

        let title = detector;
        try {
          title = CharsetManager.getCharsetTitle(detector);
        } catch (e) {}

        items.push({value: detector, name: title, current: detector == currDetector});
      }

      items.sort((aItem1, aItem2) => {
        return aItem1.name.localeCompare(aItem2.name);
      });

      return items;
    },
    populateList: function(aDocument, aContainerId, aSection) {
      let containerElem = aDocument.getElementById(aContainerId);

      while (containerElem.firstChild) {
        containerElem.removeChild(containerElem.firstChild);
      }

      containerElem.addEventListener("command", this.onCommand, false);

      let list = [];
      if (aSection == "autodetect") {
        list = this.getAutoDetectors(aDocument);
      } else if (aSection == "browser") {
        let staticList = this.getCharsetList("static", aDocument);
        let cacheList = this.getCharsetList("cache", aDocument);
        // Combine lists, and de-duplicate.
        let checkedIn = new Set();
        for (let item of staticList.concat(cacheList)) {
          let itemName = item.name.toLowerCase();
          if (!checkedIn.has(itemName)) {
            list.push(item);
            checkedIn.add(itemName);
          }
        }
      }

      // Update the appearance of the buttons when it's not possible to
      // customize encoding.
      let disabled = this.maybeDisableMenu(aDocument);
      for (let item of list) {
        let elem = aDocument.createElementNS(kNSXUL, "toolbarbutton");
        elem.setAttribute("label", item.name);
        elem.section = aSection;
        elem.value = item.value;
        if (item.current)
          elem.setAttribute("current", "true");
        if (disabled)
          elem.setAttribute("disabled", "true");
        containerElem.appendChild(elem);
      }
    },
    onViewShowing: function(aEvent) {
      let document = aEvent.target.ownerDocument;

      this.populateList(document,
                        "PanelUI-characterEncodingView-customlist",
                        "browser");
      this.populateList(document,
                        "PanelUI-characterEncodingView-autodetect",
                        "autodetect");
    },
    onCommand: function(aEvent) {
      let node = aEvent.target;
      if (!node.hasAttribute || !node.section) {
        return;
      }

      CustomizableUI.hidePanelForNode(node);
      let window = node.ownerDocument.defaultView;
      let section = node.section;
      let value = node.value;

      // The behavior as implemented here is directly based off of the
      // `MultiplexHandler()` method in browser.js.
      if (section == "browser") {
        window.BrowserSetForcedCharacterSet(value);
      } else if (section == "autodetect") {
        value = value.replace(/^chardet\./, "");
        if (value == "off") {
          value = "";
        }
        // Set the detector pref.
        try {
          let str = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString);
          str.data = value;
          Services.prefs.setComplexValue("intl.charset.detector", Ci.nsISupportsString, str);
        } catch (e) {
          Cu.reportError("Failed to set the intl.charset.detector preference.");
        }
        // Prepare a browser page reload with a changed charset.
        window.BrowserCharsetReload();
      }
    },
    onCreated: function(aNode) {
      const kPanelId = "PanelUI-popup";
      let document = aNode.ownerDocument;

      let updateButton = () => {
        if (this.maybeDisableMenu(document))
          aNode.setAttribute("disabled", "true");
        else
          aNode.removeAttribute("disabled");
      };

      if (this.currentArea == CustomizableUI.AREA_PANEL) {
        let panel = document.getElementById(kPanelId);
        panel.addEventListener("popupshowing", updateButton);
      }

      let listener = {
        onWidgetAdded: (aWidgetId, aArea) => {
          if (aWidgetId != this.id)
            return;
          if (aArea == CustomizableUI.AREA_PANEL) {
            let panel = document.getElementById(kPanelId);
            panel.addEventListener("popupshowing", updateButton);
          }
        },
        onWidgetRemoved: (aWidgetId, aPrevArea) => {
          if (aWidgetId != this.id)
            return;
          if (aPrevArea == CustomizableUI.AREA_PANEL) {
            let panel = document.getElementById(kPanelId);
            panel.removeEventListener("popupshowing", updateButton);
          }
        },
        onWidgetInstanceRemoved: (aWidgetId, aDoc) => {
          if (aWidgetId != this.id || aDoc != document)
            return;

          CustomizableUI.removeListener(listener);
          let panel = aDoc.getElementById(kPanelId);
          panel.removeEventListener("popupshowing", updateButton);
        }
      };
      CustomizableUI.addListener(listener);
    }
  }, {
    id: "email-link-button",
    onCommand: function(aEvent) {
      let win = aEvent.view;
      win.MailIntegration.sendLinkForWindow(win.content);
    }
  }];

#ifdef XP_WIN
#ifdef MOZ_METRO
if (Services.sysinfo.getProperty("hasWindowsTouchInterface")) {
  let widgetArgs = {tooltiptext: "switch-to-metro-button2.tooltiptext"};
  let brandShortName = BrandBundle.GetStringFromName("brandShortName");
  let metroTooltip = CustomizableUI.getLocalizedProperty(widgetArgs, "tooltiptext",
                                                         [brandShortName]);
  CustomizableWidgets.push({
    id: "switch-to-metro-button",
    label: "switch-to-metro-button2.label",
    tooltiptext: metroTooltip,
    defaultArea: CustomizableUI.AREA_PANEL,
    showInPrivateBrowsing: false, /* See bug 928068 */
    onCommand: function(aEvent) {
      let win = aEvent.view;
      if (win && typeof win.SwitchToMetro == "function") {
        win.SwitchToMetro();
      }
    }
  });
}
#endif
#endif
