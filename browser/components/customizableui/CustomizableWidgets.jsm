/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["CustomizableWidgets"];

Cu.import("resource:///modules/CustomizableUI.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUITelemetry",
  "resource:///modules/BrowserUITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentlyClosedTabsAndWindowsMenuUtils",
  "resource:///modules/sessionstore/RecentlyClosedTabsAndWindowsMenuUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Pocket",
  "resource:///modules/Pocket.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
  "resource://gre/modules/ShortcutUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CharsetMenu",
  "resource://gre/modules/CharsetMenu.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");

XPCOMUtils.defineLazyGetter(this, "CharsetBundle", function() {
  const kCharsetBundle = "chrome://global/locale/charsetMenu.properties";
  return Services.strings.createBundle(kCharsetBundle);
});
XPCOMUtils.defineLazyGetter(this, "BrandBundle", function() {
  const kBrandBundle = "chrome://branding/locale/brand.properties";
  return Services.strings.createBundle(kBrandBundle);
});
XPCOMUtils.defineLazyGetter(this, "PocketBundle", function() {
  const kPocketBundle = "chrome://browser/content/browser-pocket.properties";
  return Services.strings.createBundle(kPocketBundle);
});

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kPrefCustomizationDebug = "browser.uiCustomization.debug";
const kWidePanelItemClass = "panel-wide-item";

let gModuleName = "[CustomizableWidgets]";
#include logging.js

function setAttributes(aNode, aAttrs) {
  let doc = aNode.ownerDocument;
  for (let [name, value] of Iterator(aAttrs)) {
    if (!value) {
      if (aNode.hasAttribute(name))
        aNode.removeAttribute(name);
    } else {
      if (name == "shortcutId") {
        continue;
      }
      if (name == "label" || name == "tooltiptext") {
        let stringId = (typeof value == "string") ? value : name;
        let additionalArgs = [];
        if (aAttrs.shortcutId) {
          let shortcut = doc.getElementById(aAttrs.shortcutId);
          if (shortcut) {
            additionalArgs.push(ShortcutUtils.prettifyShortcut(shortcut));
          }
        }
        value = CustomizableUI.getLocalizedProperty({id: aAttrs.id}, stringId, additionalArgs);
      }
      aNode.setAttribute(name, value);
    }
  }
}

function updateCombinedWidgetStyle(aNode, aArea, aModifyCloseMenu) {
  let inPanel = (aArea == CustomizableUI.AREA_PANEL);
  let cls = inPanel ? "panel-combined-button" : "toolbarbutton-1 toolbarbutton-combined";
  let attrs = {class: cls};
  if (aModifyCloseMenu) {
    attrs.closemenu = inPanel ? "none" : null;
  }
  for (let i = 0, l = aNode.childNodes.length; i < l; ++i) {
    if (aNode.childNodes[i].localName == "separator")
      continue;
    setAttributes(aNode.childNodes[i], attrs);
  }
}

function fillSubviewFromMenuItems(aMenuItems, aSubview) {
  let attrs = ["oncommand", "onclick", "label", "key", "disabled",
               "command", "observes", "hidden", "class", "origin",
               "image", "checked"];

  let doc = aSubview.ownerDocument;
  let fragment = doc.createDocumentFragment();
  for (let menuChild of aMenuItems) {
    if (menuChild.hidden)
      continue;

    let subviewItem;
    if (menuChild.localName == "menuseparator") {
      // Don't insert duplicate or leading separators. This can happen if there are
      // menus (which we don't copy) above the separator.
      if (!fragment.lastChild || fragment.lastChild.localName == "menuseparator") {
        continue;
      }
      subviewItem = doc.createElementNS(kNSXUL, "menuseparator");
    } else if (menuChild.localName == "menuitem") {
      subviewItem = doc.createElementNS(kNSXUL, "toolbarbutton");
      CustomizableUI.addShortcut(menuChild, subviewItem);

      let item = menuChild;
      if (!item.hasAttribute("onclick")) {
        subviewItem.addEventListener("click", event => {
          let newEvent = new doc.defaultView.MouseEvent(event.type, event);
          item.dispatchEvent(newEvent);
        });
      }

      if (!item.hasAttribute("oncommand")) {
        subviewItem.addEventListener("command", event => {
          let newEvent = doc.createEvent("XULCommandEvent");
          newEvent.initCommandEvent(
            event.type, event.bubbles, event.cancelable, event.view,
            event.detail, event.ctrlKey, event.altKey, event.shiftKey,
            event.metaKey, event.sourceEvent);
          item.dispatchEvent(newEvent);
        });
      }
    } else {
      continue;
    }
    for (let attr of attrs) {
      let attrVal = menuChild.getAttribute(attr);
      if (attrVal)
        subviewItem.setAttribute(attr, attrVal);
    }
    // We do this after so the .subviewbutton class doesn't get overriden.
    if (menuChild.localName == "menuitem") {
      subviewItem.classList.add("subviewbutton");
    }
    fragment.appendChild(subviewItem);
  }
  aSubview.appendChild(fragment);
}

function clearSubview(aSubview) {
  let parent = aSubview.parentNode;
  // We'll take the container out of the document before cleaning it out
  // to avoid reflowing each time we remove something.
  parent.removeChild(aSubview);

  while (aSubview.firstChild) {
    aSubview.firstChild.remove();
  }

  parent.appendChild(aSubview);
}

const CustomizableWidgets = [
  {
    id: "history-panelmenu",
    type: "view",
    viewId: "PanelUI-history",
    shortcutId: "key_gotoHistory",
    tooltiptext: "history-panelmenu.tooltiptext2",
    defaultArea: CustomizableUI.AREA_PANEL,
    onViewShowing: function(aEvent) {
      // Populate our list of history
      const kMaxResults = 15;
      let doc = aEvent.detail.ownerDocument;
      let win = doc.defaultView;

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

      // Get all statically placed buttons to supply them with keyboard shortcuts.
      let staticButtons = items.parentNode.getElementsByTagNameNS(kNSXUL, "toolbarbutton");
      for (let i = 0, l = staticButtons.length; i < l; ++i)
        CustomizableUI.addShortcut(staticButtons[i]);

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
              item.setAttribute("targetURI", uri);
              item.setAttribute("class", "subviewbutton");
              item.addEventListener("click", function (aEvent) {
                onHistoryVisit(uri, aEvent, item);
              });
              if (icon) {
                let iconURL = PlacesUtils.getImageURLForResolution(win, "moz-anno:favicon:" + icon);
                item.setAttribute("image", iconURL);
              }
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

      let utils = RecentlyClosedTabsAndWindowsMenuUtils;
      let tabsFragment = utils.getTabsFragment(doc.defaultView, "toolbarbutton", true,
                                               "menuRestoreAllTabsSubview.label");
      let separator = doc.getElementById("PanelUI-recentlyClosedTabs-separator");
      let elementCount = tabsFragment.childElementCount;
      separator.hidden = !elementCount;
      while (--elementCount >= 0) {
        tabsFragment.children[elementCount].classList.add("subviewbutton");
      }
      recentlyClosedTabs.appendChild(tabsFragment);

      let windowsFragment = utils.getWindowsFragment(doc.defaultView, "toolbarbutton", true,
                                                     "menuRestoreAllWindowsSubview.label");
      separator = doc.getElementById("PanelUI-recentlyClosedWindows-separator");
      elementCount = windowsFragment.childElementCount;
      separator.hidden = !elementCount;
      while (--elementCount >= 0) {
        windowsFragment.children[elementCount].classList.add("subviewbutton");
      }
      recentlyClosedWindows.appendChild(windowsFragment);
    },
    onCreated: function(aNode) {
      // Middle clicking recently closed items won't close the panel - cope:
      let onRecentlyClosedClick = function(aEvent) {
        if (aEvent.button == 1) {
          CustomizableUI.hidePanelForNode(this);
        }
      };
      let doc = aNode.ownerDocument;
      let recentlyClosedTabs = doc.getElementById("PanelUI-recentlyClosedTabs");
      let recentlyClosedWindows = doc.getElementById("PanelUI-recentlyClosedWindows");
      recentlyClosedTabs.addEventListener("click", onRecentlyClosedClick);
      recentlyClosedWindows.addEventListener("click", onRecentlyClosedClick);
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
    tooltiptext: "save-page-button.tooltiptext3",
    defaultArea: CustomizableUI.AREA_PANEL,
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && typeof win.saveDocument == "function") {
        win.saveDocument(win.gBrowser.selectedBrowser.contentDocumentAsCPOW);
      }
    }
  }, {
    id: "find-button",
    shortcutId: "key_find",
    tooltiptext: "find-button.tooltiptext3",
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
    tooltiptext: "open-file-button.tooltiptext3",
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
    tooltiptext: "developer-button.tooltiptext2",
#ifdef MOZ_DEV_EDITION
    defaultArea: CustomizableUI.AREA_NAVBAR,
#else
    defaultArea: CustomizableUI.AREA_PANEL,
#endif
    onViewShowing: function(aEvent) {
      // Populate the subview with whatever menuitems are in the developer
      // menu. We skip menu elements, because the menu panel has no way
      // of dealing with those right now.
      let doc = aEvent.target.ownerDocument;
      let win = doc.defaultView;

      let menu = doc.getElementById("menuWebDeveloperPopup");

      let itemsToDisplay = [...menu.children];
      // Hardcode the addition of the "work offline" menuitem at the bottom:
      itemsToDisplay.push({localName: "menuseparator", getAttribute: () => {}});
      itemsToDisplay.push(doc.getElementById("goOfflineMenuitem"));
      fillSubviewFromMenuItems(itemsToDisplay, doc.getElementById("PanelUI-developerItems"));

    },
    onViewHiding: function(aEvent) {
      let doc = aEvent.target.ownerDocument;
      clearSubview(doc.getElementById("PanelUI-developerItems"));
    }
  }, {
    id: "sidebar-button",
    type: "view",
    viewId: "PanelUI-sidebar",
    tooltiptext: "sidebar-button.tooltiptext2",
    onViewShowing: function(aEvent) {
      // Largely duplicated from the developer-button above with a couple minor
      // alterations.
      // Populate the subview with whatever menuitems are in the
      // sidebar menu. We skip menu elements, because the menu panel has no way
      // of dealing with those right now.
      let doc = aEvent.target.ownerDocument;
      let win = doc.defaultView;
      let menu = doc.getElementById("viewSidebarMenu");

      // First clear any existing menuitems then populate. Social sidebar
      // options may not have been added yet, so we do that here. Add it to the
      // standard menu first, then copy all sidebar options to the panel.
      win.SocialSidebar.clearProviderMenus();
      let providerMenuSeps = menu.getElementsByClassName("social-provider-menu");
      if (providerMenuSeps.length > 0)
        win.SocialSidebar.populateProviderMenu(providerMenuSeps[0]);

      fillSubviewFromMenuItems([...menu.children], doc.getElementById("PanelUI-sidebarItems"));
    },
    onViewHiding: function(aEvent) {
      let doc = aEvent.target.ownerDocument;
      clearSubview(doc.getElementById("PanelUI-sidebarItems"));
    }
  }, {
    id: "social-share-button",
    // custom build our button so we can attach to the share command
    type: "custom",
    onBuild: function(aDocument) {
      let node = aDocument.createElementNS(kNSXUL, "toolbarbutton");
      node.setAttribute("id", this.id);
      node.classList.add("toolbarbutton-1");
      node.classList.add("chromeclass-toolbar-additional");
      node.setAttribute("label", CustomizableUI.getLocalizedProperty(this, "label"));
      node.setAttribute("tooltiptext", CustomizableUI.getLocalizedProperty(this, "tooltiptext"));
      node.setAttribute("removable", "true");
      node.setAttribute("observes", "Social:PageShareOrMark");
      node.setAttribute("command", "Social:SharePage");

      let listener = {
        onWidgetAdded: (aWidgetId) => {
          if (aWidgetId != this.id)
            return;

          Services.obs.notifyObservers(null, "social:" + this.id + "-added", null);
        },

        onWidgetRemoved: aWidgetId => {
          if (aWidgetId != this.id)
            return;

          Services.obs.notifyObservers(null, "social:" + this.id + "-removed", null);
        },

        onWidgetInstanceRemoved: (aWidgetId, aDoc) => {
          if (aWidgetId != this.id || aDoc != aDocument)
            return;

          CustomizableUI.removeListener(listener);
        }
      };
      CustomizableUI.addListener(listener);

      return node;
    }
  }, {
    id: "add-ons-button",
    shortcutId: "key_openAddons",
    tooltiptext: "add-ons-button.tooltiptext3",
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
    tooltiptext: "preferences-button.tooltipWin2",
#else
#ifdef XP_MACOSX
    tooltiptext: "preferences-button.tooltiptext.withshortcut",
    shortcutId: "key_preferencesCmdMac",
#else
    tooltiptext: "preferences-button.tooltiptext2",
#endif
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
    tooltiptext: "zoom-controls.tooltiptext2",
    defaultArea: CustomizableUI.AREA_PANEL,
    onBuild: function(aDocument) {
      const kPanelId = "PanelUI-popup";
      let areaType = CustomizableUI.getAreaType(this.currentArea);
      let inPanel = areaType == CustomizableUI.TYPE_MENU_PANEL;
      let inToolbar = areaType == CustomizableUI.TYPE_TOOLBAR;

      let buttons = [{
        id: "zoom-out-button",
        command: "cmd_fullZoomReduce",
        label: true,
        tooltiptext: "tooltiptext2",
        shortcutId: "key_fullZoomReduce",
      }, {
        id: "zoom-reset-button",
        command: "cmd_fullZoomReset",
        tooltiptext: "tooltiptext2",
        shortcutId: "key_fullZoomReset",
      }, {
        id: "zoom-in-button",
        command: "cmd_fullZoomEnlarge",
        label: true,
        tooltiptext: "tooltiptext2",
        shortcutId: "key_fullZoomEnlarge",
      }];

      let node = aDocument.createElementNS(kNSXUL, "toolbaritem");
      node.setAttribute("id", "zoom-controls");
      node.setAttribute("label", CustomizableUI.getLocalizedProperty(this, "label"));
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

      // The middle node is the 'Reset Zoom' button.
      let zoomResetButton = node.childNodes[2];
      let window = aDocument.defaultView;
      function updateZoomResetButton() {
        let updateDisplay = true;
        // Label should always show 100% in customize mode, so don't update:
        if (aDocument.documentElement.hasAttribute("customizing")) {
          updateDisplay = false;
        }
        //XXXgijs in some tests we get called very early, and there's no docShell on the
        // tabbrowser. This breaks the zoom toolkit code (see bug 897410). Don't let that happen:
        let zoomFactor = 100;
        try {
          zoomFactor = Math.round(window.ZoomManager.zoom * 100);
        } catch (e) {}
        zoomResetButton.setAttribute("label", CustomizableUI.getLocalizedProperty(
          buttons[1], "label", [updateDisplay ? zoomFactor : 100]
        ));
      };

      // Register ourselves with the service so we know when the zoom prefs change.
      Services.obs.addObserver(updateZoomResetButton, "browser-fullZoom:zoomChange", false);
      Services.obs.addObserver(updateZoomResetButton, "browser-fullZoom:zoomReset", false);
      Services.obs.addObserver(updateZoomResetButton, "browser-fullZoom:location-change", false);

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
      updateCombinedWidgetStyle(node, this.currentArea, true);

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
          Services.obs.removeObserver(updateZoomResetButton, "browser-fullZoom:location-change");
          let panel = aDoc.getElementById(kPanelId);
          panel.removeEventListener("popupshowing", updateZoomResetButton);
          let container = aDoc.defaultView.gBrowser.tabContainer;
          container.removeEventListener("TabSelect", updateZoomResetButton);
        }.bind(this),

        onCustomizeStart: function(aWindow) {
          if (aWindow.document == aDocument) {
            updateZoomResetButton();
          }
        },

        onCustomizeEnd: function(aWindow) {
          if (aWindow.document == aDocument) {
            updateZoomResetButton();
          }
        },

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
    tooltiptext: "edit-controls.tooltiptext2",
    defaultArea: CustomizableUI.AREA_PANEL,
    onBuild: function(aDocument) {
      let buttons = [{
        id: "cut-button",
        command: "cmd_cut",
        label: true,
        tooltiptext: "tooltiptext2",
        shortcutId: "key_cut",
      }, {
        id: "copy-button",
        command: "cmd_copy",
        label: true,
        tooltiptext: "tooltiptext2",
        shortcutId: "key_copy",
      }, {
        id: "paste-button",
        command: "cmd_paste",
        label: true,
        tooltiptext: "tooltiptext2",
        shortcutId: "key_paste",
      }];

      let node = aDocument.createElementNS(kNSXUL, "toolbaritem");
      node.setAttribute("id", "edit-controls");
      node.setAttribute("label", CustomizableUI.getLocalizedProperty(this, "label"));
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

      updateCombinedWidgetStyle(node, this.currentArea);

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
    tooltiptext: "feed-button.tooltiptext2",
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
    label: "characterencoding-button2.label",
    type: "view",
    viewId: "PanelUI-characterEncodingView",
    tooltiptext: "characterencoding-button2.tooltiptext",
    defaultArea: CustomizableUI.AREA_PANEL,
    maybeDisableMenu: function(aDocument) {
      let window = aDocument.defaultView;
      return !(window.gBrowser &&
               window.gBrowser.selectedBrowser.mayEnableCharacterEncodingMenu);
    },
    populateList: function(aDocument, aContainerId, aSection) {
      let containerElem = aDocument.getElementById(aContainerId);

      containerElem.addEventListener("command", this.onCommand, false);

      let list = this.charsetInfo[aSection];

      for (let item of list) {
        let elem = aDocument.createElementNS(kNSXUL, "toolbarbutton");
        elem.setAttribute("label", item.label);
        elem.setAttribute("type", "checkbox");
        elem.section = aSection;
        elem.value = item.value;
        elem.setAttribute("class", "subviewbutton");
        containerElem.appendChild(elem);
      }
    },
    updateCurrentCharset: function(aDocument) {
      let currentCharset = aDocument.defaultView.gBrowser.selectedBrowser.characterSet;
      currentCharset = CharsetMenu.foldCharset(currentCharset);

      let pinnedContainer = aDocument.getElementById("PanelUI-characterEncodingView-pinned");
      let charsetContainer = aDocument.getElementById("PanelUI-characterEncodingView-charsets");
      let elements = [...(pinnedContainer.childNodes), ...(charsetContainer.childNodes)];

      this._updateElements(elements, currentCharset);
    },
    updateCurrentDetector: function(aDocument) {
      let detectorContainer = aDocument.getElementById("PanelUI-characterEncodingView-autodetect");
      let currentDetector;
      try {
        currentDetector = Services.prefs.getComplexValue(
          "intl.charset.detector", Ci.nsIPrefLocalizedString).data;
      } catch (e) {}

      this._updateElements(detectorContainer.childNodes, currentDetector);
    },
    _updateElements: function(aElements, aCurrentItem) {
      if (!aElements.length) {
        return;
      }
      let disabled = this.maybeDisableMenu(aElements[0].ownerDocument);
      for (let elem of aElements) {
        if (disabled) {
          elem.setAttribute("disabled", "true");
        } else {
          elem.removeAttribute("disabled");
        }
        if (elem.value.toLowerCase() == aCurrentItem.toLowerCase()) {
          elem.setAttribute("checked", "true");
        } else {
          elem.removeAttribute("checked");
        }
      }
    },
    onViewShowing: function(aEvent) {
      let document = aEvent.target.ownerDocument;

      let autoDetectLabelId = "PanelUI-characterEncodingView-autodetect-label";
      let autoDetectLabel = document.getElementById(autoDetectLabelId);
      if (!autoDetectLabel.hasAttribute("value")) {
        let label = CharsetBundle.GetStringFromName("charsetMenuAutodet");
        autoDetectLabel.setAttribute("value", label);
        this.populateList(document,
                          "PanelUI-characterEncodingView-pinned",
                          "pinnedCharsets");
        this.populateList(document,
                          "PanelUI-characterEncodingView-charsets",
                          "otherCharsets");
        this.populateList(document,
                          "PanelUI-characterEncodingView-autodetect",
                          "detectors");
      }
      this.updateCurrentDetector(document);
      this.updateCurrentCharset(document);
    },
    onCommand: function(aEvent) {
      let node = aEvent.target;
      if (!node.hasAttribute || !node.section) {
        return;
      }

      let window = node.ownerDocument.defaultView;
      let section = node.section;
      let value = node.value;

      // The behavior as implemented here is directly based off of the
      // `MultiplexHandler()` method in browser.js.
      if (section != "detectors") {
        window.BrowserSetForcedCharacterSet(value);
      } else {
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
          aNode.removeAttribute("disabled");
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
      if (!this.charsetInfo) {
        this.charsetInfo = CharsetMenu.getData();
      }
    }
  }, {
    id: "email-link-button",
    tooltiptext: "email-link-button.tooltiptext3",
    onCommand: function(aEvent) {
      let win = aEvent.view;
      win.MailIntegration.sendLinkForBrowser(win.gBrowser.selectedBrowser)
    }
  }, {
    id: "loop-button",
    type: "custom",
    label: "loop-call-button3.label",
    tooltiptext: "loop-call-button3.tooltiptext",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    // Not in private browsing, see bug 1108187.
    showInPrivateBrowsing: false,
    introducedInVersion: 4,
    onBuild: function(aDocument) {
      // If we're not supposed to see the button, return zip.
      if (!Services.prefs.getBoolPref("loop.enabled")) {
        return null;
      }

      let node = aDocument.createElementNS(kNSXUL, "toolbarbutton");
      node.setAttribute("id", this.id);
      node.classList.add("toolbarbutton-1");
      node.classList.add("chromeclass-toolbar-additional");
      node.classList.add("badged-button");
      node.setAttribute("label", CustomizableUI.getLocalizedProperty(this, "label"));
      node.setAttribute("tooltiptext", CustomizableUI.getLocalizedProperty(this, "tooltiptext"));
      node.setAttribute("removable", "true");
      node.addEventListener("command", function(event) {
        aDocument.defaultView.LoopUI.togglePanel(event);
      });

      return node;
    }
  }, {
    id: "web-apps-button",
    label: "web-apps-button.label",
    tooltiptext: "web-apps-button.tooltiptext",
    onCommand: function(aEvent) {
      let win = aEvent.target &&
                aEvent.target.ownerDocument &&
                aEvent.target.ownerDocument.defaultView;
      if (win && typeof win.BrowserOpenApps == "function") {
        win.BrowserOpenApps();
      }
    }
  }];

if (Services.prefs.getBoolPref("privacy.panicButton.enabled")) {
  CustomizableWidgets.push({
    id: "panic-button",
    type: "view",
    viewId: "PanelUI-panicView",
    _sanitizer: null,
    _ensureSanitizer: function() {
      if (!this.sanitizer) {
        let scope = {};
        Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js",
                                            scope);
        this._Sanitizer = scope.Sanitizer;
        this._sanitizer = new scope.Sanitizer();
        this._sanitizer.ignoreTimespan = false;
      }
    },
    _getSanitizeRange: function(aDocument) {
      let group = aDocument.getElementById("PanelUI-panic-timeSpan");
      return this._Sanitizer.getClearRange(+group.value);
    },
    forgetButtonCalled: function(aEvent) {
      let doc = aEvent.target.ownerDocument;
      this._ensureSanitizer();
      this._sanitizer.range = this._getSanitizeRange(doc);
      let group = doc.getElementById("PanelUI-panic-timeSpan");
      BrowserUITelemetry.countPanicEvent(group.selectedItem.id);
      group.selectedItem = doc.getElementById("PanelUI-panic-5min");
      let itemsToClear = [
        "cookies", "history", "openWindows", "formdata", "sessions", "cache", "downloads"
      ];
      let newWindowPrivateState = PrivateBrowsingUtils.isWindowPrivate(doc.defaultView) ?
                                  "private" : "non-private";
      this._sanitizer.items.openWindows.privateStateForNewWindow = newWindowPrivateState;
      let promise = this._sanitizer.sanitize(itemsToClear);
      promise.then(function() {
        let otherWindow = Services.wm.getMostRecentWindow("navigator:browser");
        if (otherWindow.closed) {
          Cu.reportError("Got a closed window!");
        }
        if (otherWindow.PanicButtonNotifier) {
          otherWindow.PanicButtonNotifier.notify();
        } else {
          otherWindow.PanicButtonNotifierShouldNotify = true;
        }
      });
    },
    handleEvent: function(aEvent) {
      switch (aEvent.type) {
        case "command":
          this.forgetButtonCalled(aEvent);
          break;
      }
    },
    onViewShowing: function(aEvent) {
      let forgetButton = aEvent.target.querySelector("#PanelUI-panic-view-button");
      forgetButton.addEventListener("command", this);
    },
    onViewHiding: function(aEvent) {
      let forgetButton = aEvent.target.querySelector("#PanelUI-panic-view-button");
      forgetButton.removeEventListener("command", this);
    },
  });
}

if (Services.prefs.getBoolPref("browser.pocket.enabled")) {
  let isEnabledForLocale = true;
  if (Services.prefs.getBoolPref("browser.pocket.useLocaleList")) {
    let chromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"]
                           .getService(Ci.nsIXULChromeRegistry);
    let browserLocale = chromeRegistry.getSelectedLocale("browser");
    let enabledLocales = [];
    try {
      enabledLocales = Services.prefs.getCharPref("browser.pocket.enabledLocales").split(' ');
    } catch (ex) {
      Cu.reportError(ex);
    }
    isEnabledForLocale = enabledLocales.indexOf(browserLocale) != -1;
  }

  if (isEnabledForLocale) {
    let pocketButton = {
      id: "pocket-button",
      type: "view",
      viewId: "PanelUI-pocketView",
      label: PocketBundle.GetStringFromName("pocket-button.label"),
      tooltiptext: PocketBundle.GetStringFromName("pocket-button.tooltiptext"),
      onCreated(node) {
        let doc = node.ownerDocument;
        let elementsNeedingStrings = [
          "pocket-header",
          "pocket-login-required-tagline",
          "pocket-signup-with-fxa",
          "pocket-signup-with-email",
          "pocket-account-question",
          "pocket-login-now",
          "pocket-page-saved-header",
          "pocket-open-pocket",
          "pocket-remove-page",
          "pocket-page-tags-field",
          "pocket-page-tags-add",
          "pocket-page-suggested-tags-header",
          "pocket-signup-or",
        ];

        for (let elementId of elementsNeedingStrings) {
          let el = doc.getElementById(elementId);
          let string = PocketBundle.GetStringFromName(elementId);

          switch (el.localName) {
            case "button":
              el.label = string;
              break;
            case "textbox":
              el.setAttribute("placeholder", string);
              break;
            default:
              el.textContent = string;
              break;
          }
        }

        let addTagsField = doc.getElementById("pocket-page-tags-field");
        let addTagsButton = doc.getElementById("pocket-page-tags-add");
        addTagsField.addEventListener("input", this);
        addTagsButton.addEventListener("command", this);
      },
      onViewShowing(event) {
        let doc = event.target.ownerDocument;

        let loginView = doc.getElementById("pocket-login-required");
        let pageSavedView = doc.getElementById("pocket-page-saved");
        let showPageSaved = Pocket.isLoggedIn;
        loginView.hidden = showPageSaved;
        pageSavedView.hidden = !showPageSaved;

        if (!showPageSaved)
          return;

        let gBrowser = doc.defaultView.gBrowser;
        let uri = gBrowser.currentURI;
        if (uri.schemeIs("about"))
          uri = ReaderMode.getOriginalUrl(uri.spec);
        else
          uri = uri.spec;
        if (!uri)
          return; //TODO should prevent the panel from showing

        Pocket.save(uri, gBrowser.contentTitle).then(
          item => {
            doc.getElementById("pocket-remove-page").itemId = item.item_id;
          },
          error => {dump(error + "\n");}
        );
      },
      onViewHiding(event) {
        let doc = event.target.ownerDocument;
        doc.getElementById("pocket-remove-page").itemId = null;
      },

      handleEvent: function(event) {
        let doc = event.target.ownerDocument;
        let field = doc.getElementById("pocket-page-tags-field");
        let button = doc.getElementById("pocket-page-tags-add");
        switch (event.type) {
          case "input":
            button.disabled = !field.value.trim();
            break;
          case "command":
            Pocket.tag(doc.getElementById("pocket-remove-page").itemId,
                       field.value);
            field.value = "";
            break;
        }
      },

      USER_REMOVED_PREF: "browser.pocket.removedByUser",
      onWidgetAdded(aWidgetId, aArea, aPosition) {
        if (aWidgetId != this.id) {
          return;
        }

        let placement = CustomizableUI.getPlacementOfWidget(this.id);
        let widgetInUI = placement && placement.area;
        Services.prefs.setBoolPref(this.USER_REMOVED_PREF, !widgetInUI);
      },
      onWidgetRemoved(aWidgetId, aArea) {
        if (aWidgetId != this.id) {
          return;
        }

        let placement = CustomizableUI.getPlacementOfWidget(this.id);
        let widgetInUI = placement && placement.area;
        Services.prefs.setBoolPref(this.USER_REMOVED_PREF, !widgetInUI);
      },
      onWidgetReset(aNode, aContainer) {
        if (aNode.id != this.id) {
          return;
        }

        Services.prefs.setBoolPref(this.USER_REMOVED_PREF, !aContainer);
      },
      onWidgetUndoMove(aNode, aContainer) {
        if (aNode.id != this.id) {
          return;
        }

        Services.prefs.setBoolPref(this.USER_REMOVED_PREF, !aContainer);
      }
    };

    CustomizableWidgets.push(pocketButton);
    CustomizableUI.addListener(pocketButton);
  }
}

#ifdef E10S_TESTING_ONLY
let e10sDisabled = Services.appinfo.inSafeMode;
#ifdef XP_MACOSX
// On OS X, "Disable Hardware Acceleration" also disables OMTC and forces
// a fallback to Basic Layers. This is incompatible with e10s.
e10sDisabled |= Services.prefs.getBoolPref("layers.acceleration.disabled");
#endif

if (Services.appinfo.browserTabsRemoteAutostart) {
  CustomizableWidgets.push({
    id: "e10s-button",
    label: "New Non-e10s Window",
    tooltiptext: "New Non-e10s Window",
    disabled: e10sDisabled,
    defaultArea: CustomizableUI.AREA_PANEL,
    onCommand: function(aEvent) {
      let win = aEvent.view;
      if (win && typeof win.OpenBrowserWindow == "function") {
        win.OpenBrowserWindow({remote: false});
      }
    },
  });
}
#endif
